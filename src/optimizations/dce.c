#include "optimizations/dce.h"

GHashTable *live_set = NULL;

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

static void get_live_variables_from_fields(struct llir_iterator *iterator)
{
	for (uint32_t i = 0; i < iterator->block->fields->len; i++) {
		struct llir_field *field = g_array_index(
			iterator->block->fields, struct llir_field *, i);
		if (field->is_array)
			live_set_add(field->identifier);
	}
}

static void
get_live_variables_from_method_call_arguments(struct llir_iterator *iterator)
{
	if (iterator->assignment->type == LLIR_ASSIGNMENT_TYPE_METHOD_CALL)
		for (u_int32_t i = 0; i < iterator->assignment->argument_count;
		     i++) {
			if (iterator->assignment->arguments[i].type ==
			    LLIR_OPERAND_TYPE_FIELD)
				live_set_add(iterator->assignment->arguments[i]
						     .field);
		}
}

static void get_live_variables_from_terminals(struct llir_iterator *iterator)
{
	switch (iterator->block->terminal_type) {
	case LLIR_BLOCK_TERMINAL_TYPE_BRANCH:
		if (iterator->block->branch->left.type ==
		    LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->block->branch->left.field);
		if (iterator->block->branch->right.type ==
		    LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->block->branch->right.field);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_RETURN:
		if (iterator->block->llir_return->source.type ==
		    LLIR_OPERAND_TYPE_FIELD) {
			live_set_add(
				iterator->block->llir_return->source.field);
		}
		break;
	default:
		break;
	}
}

static void get_live_variables_from_assignments(struct llir_iterator *iterator)
{
	if (!live_set_contains(iterator->assignment->destination))
		return;

	switch (iterator->assignment->type) {
	case LLIR_ASSIGNMENT_TYPE_MOVE:
		if (iterator->assignment->source.type ==
		    LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->assignment->source.field);
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
		if (iterator->assignment->left.type == LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->assignment->left.field);
		if (iterator->assignment->right.type == LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->assignment->right.field);
		break;
	case LLIR_ASSIGNMENT_TYPE_NEGATE:
	case LLIR_ASSIGNMENT_TYPE_NOT:
		if (iterator->assignment->source.type ==
		    LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->assignment->source.field);
		break;
	case LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE:
		if (iterator->assignment->update_index.type ==
		    LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->assignment->update_index.field);
		if (iterator->assignment->update_value.type ==
		    LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->assignment->update_value.field);
		break;
	case LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS:
		if (iterator->assignment->access_index.type ==
		    LLIR_OPERAND_TYPE_FIELD)
			live_set_add(iterator->assignment->access_index.field);
		break;
	case LLIR_ASSIGNMENT_TYPE_METHOD_CALL:
		for (u_int32_t i = 0; i < iterator->assignment->argument_count;
		     i++) {
			if (iterator->assignment->arguments[i].type ==
			    LLIR_OPERAND_TYPE_FIELD)
				live_set_add(iterator->assignment->arguments[i]
						     .field);
		}
		return;
	default:
		break;
	}
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

void optimization_dead_code_elimination(struct llir *llir)
{
	live_set = live_set_init();

	// add all global fields to live set
	for (uint32_t i = 0; i < llir->fields->len; i++) {
		live_set_add(g_array_index(llir->fields, struct llir_field *, i)
				     ->identifier);
	}

	// add terminal variables and method call arguments to live set
	llir_iterate(llir, NULL, get_live_variables_from_fields,
		     get_live_variables_from_method_call_arguments,
		     get_live_variables_from_terminals, true);

	uint32_t prev_live_count = 0;

	do {
		prev_live_count = live_set_count();
		llir_iterate(llir, NULL, NULL,
			     get_live_variables_from_assignments, NULL, true);
	} while (live_set_count() != prev_live_count);

	llir_iterate(llir, NULL, NULL, prune_dead_variables, NULL, true);

	// live_set_print();

	live_set_free();
}
