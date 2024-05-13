#include "optimizations/dce.h"

GHashTable *live_set = NULL;

char *current_method = NULL;

static GHashTable *live_set_init(void)
{
	return g_hash_table_new(g_str_hash, g_str_equal);
}

static void live_set_free(void)
{
	g_hash_table_destroy(live_set);
}

static uint32_t live_set_count(void)
{
	return g_hash_table_size(live_set);
}

static bool live_set_contains(char *variable)
{
	return g_hash_table_lookup(live_set, variable);
}

static void live_set_add(char *variable)
{
	if (!live_set_contains(variable)) {
		g_hash_table_insert(live_set, variable, (gpointer) true);
	}
}

void live_set_print_helper(gpointer key, gpointer value, gpointer user_data)
{
	g_print("%s,", (gchar *)key);
}

static void live_set_print(void)
{
	g_hash_table_foreach(live_set, live_set_print_helper, NULL);
	g_print("\n");
}

static bool is_current_method(struct llir_iterator *iterator)
{
	return strcmp(iterator->method->identifier, current_method) != 0;
}

static void add_live_variables_from_globals(struct llir *llir)
{
	for (uint32_t i = 0; i < llir->fields->len; i++) {
		live_set_add(g_array_index(llir->fields, struct llir_field *, i)
				     ->identifier);
	}
}

static void add_live_variables_from_fields(struct llir_iterator *iterator)
{
	for (uint32_t i = 0; i < iterator->block->fields->len; i++) {
		struct llir_field *field = g_array_index(
			iterator->block->fields, struct llir_field *, i);
		if (field->is_array)
			live_set_add(field->identifier);
	}
}

static void
add_live_variables_from_fields_of_current_method(struct llir_iterator *iterator)
{
	if (is_current_method(iterator))
		return;
	add_live_variables_from_fields(iterator);
}

static void add_live_variable_from_operand(struct llir_operand operand)
{
	if (operand.type == LLIR_OPERAND_TYPE_FIELD)
		live_set_add(operand.field);
}

static void
add_live_variables_from_method_call_arguments(struct llir_iterator *iterator)
{
	if (iterator->assignment->type == LLIR_ASSIGNMENT_TYPE_METHOD_CALL)
		for (u_int32_t i = 0; i < iterator->assignment->argument_count;
		     i++) {
			add_live_variable_from_operand(
				iterator->assignment->arguments[i]);
		}
}

static void add_live_variables_from_method_call_arguments_of_current_method(
	struct llir_iterator *iterator)
{
	if (is_current_method(iterator))
		return;
	add_live_variables_from_method_call_arguments(iterator);
}

static void add_live_variables_from_terminals(struct llir_iterator *iterator)
{
	switch (iterator->block->terminal_type) {
	case LLIR_BLOCK_TERMINAL_TYPE_BRANCH:
		add_live_variable_from_operand(iterator->block->branch->left);
		add_live_variable_from_operand(iterator->block->branch->right);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_RETURN:
		add_live_variable_from_operand(
			iterator->block->llir_return->source);
		break;
	default:
		break;
	}
}

static void add_live_variables_from_terminals_of_current_method(
	struct llir_iterator *iterator)
{
	if (is_current_method(iterator))
		return;
	add_live_variables_from_terminals(iterator);
}

static void add_live_variables_from_assignments(struct llir_iterator *iterator)
{
	if (!live_set_contains(iterator->assignment->destination))
		return;

	switch (iterator->assignment->type) {
	case LLIR_ASSIGNMENT_TYPE_MOVE:
	case LLIR_ASSIGNMENT_TYPE_NEGATE:
	case LLIR_ASSIGNMENT_TYPE_NOT:
		add_live_variable_from_operand(iterator->assignment->source);
		break;
	case LLIR_ASSIGNMENT_TYPE_ADD:
	case LLIR_ASSIGNMENT_TYPE_SUBTRACT:
	case LLIR_ASSIGNMENT_TYPE_MULTIPLY:
	case LLIR_ASSIGNMENT_TYPE_DIVIDE:
	case LLIR_ASSIGNMENT_TYPE_MODULO:
	case LLIR_ASSIGNMENT_TYPE_GREATER:
	case LLIR_ASSIGNMENT_TYPE_GREATER_EQUAL:
	case LLIR_ASSIGNMENT_TYPE_LESS:
	case LLIR_ASSIGNMENT_TYPE_LESS_EQUAL:
	case LLIR_ASSIGNMENT_TYPE_EQUAL:
	case LLIR_ASSIGNMENT_TYPE_NOT_EQUAL:
		add_live_variable_from_operand(iterator->assignment->left);
		add_live_variable_from_operand(iterator->assignment->right);
		break;
	case LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE:
		add_live_variable_from_operand(
			iterator->assignment->update_index);
		add_live_variable_from_operand(
			iterator->assignment->update_value);
		break;
	case LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS:
		add_live_variable_from_operand(
			iterator->assignment->access_index);
		break;
	case LLIR_ASSIGNMENT_TYPE_METHOD_CALL:
		for (u_int32_t i = 0; i < iterator->assignment->argument_count;
		     i++) {
			add_live_variable_from_operand(
				iterator->assignment->arguments[i]);
		}
		return;
	default:
		break;
	}
}

static void add_live_variables_from_assignments_of_current_method(
	struct llir_iterator *iterator)
{
	if (is_current_method(iterator))
		return;
	add_live_variables_from_assignments(iterator);
}

static void prune_dead_variables(struct llir_iterator *iterator)
{
	if (!live_set_contains(iterator->assignment->destination) &&
	    iterator->assignment->type != LLIR_ASSIGNMENT_TYPE_METHOD_CALL) {
		g_array_remove_index(iterator->block->assignments,
				     iterator->assignment_index);
		iterator->assignment_index--;
	}
}

static void
prune_dead_variables_of_curent_method(struct llir_iterator *iterator)
{
	if (is_current_method(iterator))
		return;
	prune_dead_variables(iterator);
}

static void
dead_code_elimination_of_current_method(struct llir_iterator *iterator)
{
	live_set = live_set_init();

	add_live_variables_from_globals(iterator->llir);

	current_method = iterator->method->identifier;

	llir_iterate(
		iterator->llir, NULL,
		add_live_variables_from_fields_of_current_method,
		add_live_variables_from_method_call_arguments_of_current_method,
		add_live_variables_from_terminals_of_current_method, true);

	uint32_t prev_live_count = 0;

	do {
		prev_live_count = live_set_count();
		llir_iterate(
			iterator->llir, NULL, NULL,
			add_live_variables_from_assignments_of_current_method,
			NULL, true);
	} while (live_set_count() != prev_live_count);

	llir_iterate(iterator->llir, NULL, NULL,
		     prune_dead_variables_of_curent_method, NULL, true);

	live_set_free();
}

void optimization_dead_code_elimination(struct llir *llir)
{
	llir_iterate(llir, dead_code_elimination_of_current_method, NULL, NULL,
		     NULL, true);
}
