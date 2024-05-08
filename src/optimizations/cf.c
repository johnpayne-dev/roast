#include "optimizations/cf.h"

static void find_definition_in_block(struct llir_block *block,
				     int32_t start_index, char *identifier,
				     GHashTable *visited, GArray *definitions)
{
	for (int32_t i = start_index; i >= 0; i--) {
		struct llir_assignment *assignment = g_array_index(
			block->assignments, struct llir_assignment *, i);
		if (g_strcmp0(assignment->destination, identifier) == 0) {
			g_array_append_val(definitions, assignment);
			return;
		}
	}

	for (uint32_t i = 0; i < block->predecessors->len; i++) {
		struct llir_block *predecessor = g_array_index(
			block->predecessors, struct llir_block *, i);
		if (g_hash_table_lookup(visited, predecessor))
			continue;

		g_hash_table_insert(visited, predecessor, (gpointer) true);
		find_definition_in_block(block, block->assignments->len - 1,
					 identifier, visited, definitions);
	}
}

static GArray *find_definitions(struct llir_block *block,
				uint32_t assignment_index, char *identifier)
{
	GHashTable *visited = g_hash_table_new(g_direct_hash, g_direct_equal);
	GArray *definitions =
		g_array_new(false, false, sizeof(struct llir_assignment *));
	find_definition_in_block(block, assignment_index - 1, identifier,
				 visited, definitions);

	g_hash_table_unref(visited);
	return definitions;
}

static bool all_definitions_are_constant(GArray *definitions, int64_t *constant)
{
	if (definitions->len == 0)
		return false;

	int64_t initializer = 0;
	for (uint32_t i = 0; i < definitions->len; i++) {
		struct llir_assignment *assignment =
			g_array_index(definitions, struct llir_assignment *, i);
		if (assignment->type != LLIR_ASSIGNMENT_TYPE_MOVE ||
		    assignment->source.type != LLIR_OPERAND_TYPE_LITERAL)
			return false;

		if (i == 0)
			initializer = assignment->source.literal;
		else if (assignment->source.literal != initializer)
			return false;
	}

	*constant = initializer;
	return true;
}

static void optimize_operand(struct llir_iterator *iterator,
			     struct llir_operand *operand)
{
	if (operand->type != LLIR_OPERAND_TYPE_FIELD)
		return;
	if (llir_operand_is_field_global(*operand))
		return;

	GArray *definitions = find_definitions(
		iterator->block, iterator->assignment_index, operand->field);

	int64_t constant;
	if (all_definitions_are_constant(definitions, &constant))
		*operand = llir_operand_from_literal(constant);
}

static int64_t unary_operation(int64_t literal,
			       enum llir_assignment_type operation)
{
	switch (operation) {
	case LLIR_ASSIGNMENT_TYPE_MOVE:
		return literal;
	case LLIR_ASSIGNMENT_TYPE_NOT:
		return !literal;
	case LLIR_ASSIGNMENT_TYPE_NEGATE:
		return -literal;
	default:
		g_assert(!"You fucked up");
		return -1;
	}
}

static void optimize_unary_operation(struct llir_iterator *iterator)
{
	struct llir_assignment *assignment = iterator->assignment;
	optimize_operand(iterator, &assignment->source);

	if (assignment->source.type == LLIR_OPERAND_TYPE_LITERAL) {
		int64_t new_literal = unary_operation(
			assignment->source.literal, assignment->type);
		assignment->type = LLIR_ASSIGNMENT_TYPE_MOVE;
		assignment->source = llir_operand_from_literal(new_literal);
	}
}

static int64_t binary_operation(int64_t left, int64_t right,
				enum llir_assignment_type operation)
{
	switch (operation) {
	case LLIR_ASSIGNMENT_TYPE_ADD:
		return left + right;
	case LLIR_ASSIGNMENT_TYPE_SUBTRACT:
		return left - right;
	case LLIR_ASSIGNMENT_TYPE_MULTIPLY:
		return left * right;
	case LLIR_ASSIGNMENT_TYPE_DIVIDE:
		return left / right;
	case LLIR_ASSIGNMENT_TYPE_MODULO:
		return left % right;
	case LLIR_ASSIGNMENT_TYPE_EQUAL:
		return left == right;
	case LLIR_ASSIGNMENT_TYPE_NOT_EQUAL:
		return left != right;
	case LLIR_ASSIGNMENT_TYPE_LESS:
		return left < right;
	case LLIR_ASSIGNMENT_TYPE_LESS_EQUAL:
		return left <= right;
	case LLIR_ASSIGNMENT_TYPE_GREATER:
		return left > right;
	case LLIR_ASSIGNMENT_TYPE_GREATER_EQUAL:
		return left >= right;
	default:
		g_assert(!"You fucked up");
		return -1;
	}
}

static void optimize_binary_operation(struct llir_iterator *iterator)
{
	struct llir_assignment *assignment = iterator->assignment;
	optimize_operand(iterator, &assignment->left);
	optimize_operand(iterator, &assignment->right);

	if (assignment->left.type == LLIR_OPERAND_TYPE_LITERAL &&
	    assignment->right.type == LLIR_OPERAND_TYPE_LITERAL) {
		int64_t new_literal = binary_operation(
			assignment->left.literal, assignment->right.literal,
			assignment->type);
		iterator->assignment->type = LLIR_ASSIGNMENT_TYPE_MOVE;
		iterator->assignment->source =
			llir_operand_from_literal(new_literal);
	}
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

static int64_t branch_operation(int64_t left, int64_t right,
				enum llir_branch_type operation)
{
	switch (operation) {
	case LLIR_BRANCH_TYPE_LESS:
		return left < right;
	case LLIR_BRANCH_TYPE_LESS_EQUAL:
		return left <= right;
	case LLIR_BRANCH_TYPE_GREATER:
		return left > right;
	case LLIR_BRANCH_TYPE_GREATER_EQUAL:
		return left >= right;
	case LLIR_BRANCH_TYPE_EQUAL:
		return left == right;
	case LLIR_BRANCH_TYPE_NOT_EQUAL:
		return left != right;
	default:
		g_assert(!"You fucked up");
		return -1;
	}
}

static int64_t branch_operation_unsigned(uint64_t left, uint64_t right,
					 enum llir_branch_type operation)
{
	switch (operation) {
	case LLIR_BRANCH_TYPE_LESS:
		return left < right;
	case LLIR_BRANCH_TYPE_LESS_EQUAL:
		return left <= right;
	case LLIR_BRANCH_TYPE_GREATER:
		return left > right;
	case LLIR_BRANCH_TYPE_GREATER_EQUAL:
		return left >= right;
	case LLIR_BRANCH_TYPE_EQUAL:
		return left == right;
	case LLIR_BRANCH_TYPE_NOT_EQUAL:
		return left != right;
	default:
		g_assert(!"You fucked up");
		return -1;
	}
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

void optimization_constant_folding(struct llir *llir)
{
	llir_iterate(llir, NULL, NULL, optimize_assignment, optimize_terminal);
}
