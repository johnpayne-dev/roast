#include "optimizations/cp.h"

static void find_definition_in_block(struct llir_block *block,
				     int32_t start_index, char *identifier,
				     GHashTable *visited, GArray *definitions,
				     GHashTable *mutated)
{
	for (int32_t i = start_index; i >= 0; i--) {
		struct llir_assignment *assignment = g_array_index(
			block->assignments, struct llir_assignment *, i);
		if (g_strcmp0(assignment->destination, identifier) == 0) {
			g_array_append_val(definitions, assignment);
			return;
		}

		g_hash_table_insert(mutated, assignment->destination,
				    (gpointer)1);
	}

	for (uint32_t i = 0; i < block->predecessors->len; i++) {
		struct llir_block *predecessor = g_array_index(
			block->predecessors, struct llir_block *, i);
		if (g_hash_table_lookup(visited, predecessor))
			continue;

		g_hash_table_insert(visited, predecessor, (gpointer)1);
		find_definition_in_block(predecessor,
					 predecessor->assignments->len - 1,
					 identifier, visited, definitions,
					 mutated);
	}
}

static GArray *find_definitions(struct llir_block *block,
				uint32_t assignment_index, char *identifier,
				GHashTable **mutated)
{
	GHashTable *visited = g_hash_table_new(g_direct_hash, g_direct_equal);
	GArray *definitions =
		g_array_new(false, false, sizeof(struct llir_assignment *));
	*mutated = g_hash_table_new(g_str_hash, g_str_equal);
	find_definition_in_block(block, assignment_index - 1, identifier,
				 visited, definitions, *mutated);

	g_hash_table_unref(visited);
	return definitions;
}

static bool can_propagate_copy(GArray *definitions, GHashTable *mutated,
			       char **identifier)
{
	if (definitions->len == 0)
		return false;

	char *field = NULL;
	for (uint32_t i = 0; i < definitions->len; i++) {
		struct llir_assignment *assignment =
			g_array_index(definitions, struct llir_assignment *, i);
		if (assignment->type != LLIR_ASSIGNMENT_TYPE_MOVE ||
		    assignment->source.type != LLIR_OPERAND_TYPE_FIELD)
			return false;

		if (llir_operand_is_field_global(assignment->source))
			return false;

		if (i == 0)
			field = assignment->source.field;
		else if (g_strcmp0(field, assignment->source.field) != 0)
			return false;
	}

	if (g_hash_table_lookup(mutated, field))
		return false;

	*identifier = field;
	return true;
}

static void optimize_operand(struct llir_iterator *iterator,
			     struct llir_operand *operand)
{
	if (operand->type != LLIR_OPERAND_TYPE_FIELD)
		return;
	if (llir_operand_is_field_global(*operand))
		return;

	GHashTable *mutations;
	GArray *definitions = find_definitions(iterator->block,
					       iterator->assignment_index,
					       operand->field, &mutations);

	char *field;
	if (can_propagate_copy(definitions, mutations, &field))
		*operand = llir_operand_from_field(field);

	g_array_free(definitions, true);
	g_hash_table_unref(mutations);
}

static void optimize_unary_operation(struct llir_iterator *iterator)
{
	struct llir_assignment *assignment = iterator->assignment;
	optimize_operand(iterator, &assignment->source);
}

static void optimize_binary_operation(struct llir_iterator *iterator)
{
	struct llir_assignment *assignment = iterator->assignment;
	optimize_operand(iterator, &assignment->left);
	optimize_operand(iterator, &assignment->right);
}

static void optimize_array_update(struct llir_iterator *iterator)
{
	struct llir_assignment *assignment = iterator->assignment;
	optimize_operand(iterator, &assignment->update_index);
	optimize_operand(iterator, &assignment->update_value);
}

static void optimzie_array_access(struct llir_iterator *iterator)
{
	struct llir_assignment *assignment = iterator->assignment;
	optimize_operand(iterator, &assignment->access_index);
}

static void optimize_method_call(struct llir_iterator *iterator)
{
	struct llir_assignment *assignment = iterator->assignment;
	for (uint32_t i = 0; i < assignment->argument_count; i++)
		optimize_operand(iterator, &assignment->arguments[i]);
}

static void optimize_assignment(struct llir_iterator *iterator)
{
	if (llir_assignment_is_unary(iterator->assignment))
		optimize_unary_operation(iterator);
	else if (llir_assignment_is_binary(iterator->assignment))
		optimize_binary_operation(iterator);
	else if (iterator->assignment->type ==
		 LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE)
		optimize_array_update(iterator);
	else if (iterator->assignment->type ==
		 LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS)
		optimzie_array_access(iterator);
	else if (iterator->assignment->type == LLIR_ASSIGNMENT_TYPE_METHOD_CALL)
		optimize_method_call(iterator);
	else
		g_assert(!"You fucked up");
}

static void optimize_branch(struct llir_iterator *iterator)
{
	struct llir_branch *branch = iterator->block->branch;
	optimize_operand(iterator, &branch->left);
	optimize_operand(iterator, &branch->right);
}

static void optimize_return(struct llir_iterator *iterator)
{
	optimize_operand(iterator, &iterator->block->llir_return->source);
}

static void optimize_terminal(struct llir_iterator *iterator)
{
	switch (iterator->block->terminal_type) {
	case LLIR_BLOCK_TERMINAL_TYPE_JUMP:
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_BRANCH:
		optimize_branch(iterator);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_RETURN:
		optimize_return(iterator);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_SHIT_YOURSELF:
		break;
	default:
		g_assert(!"You fucked up");
		break;
	}
}

void optimization_copy_propagation(struct llir *llir)
{
	llir_iterate(llir, NULL, NULL, optimize_assignment, optimize_terminal,
		     true);
}
