#include "optimizations/dce.h"

GArray *live_set = NULL;

static bool live_set_contains(char *variable)
{
	for (uint32_t i = 0; i < live_set->len; i++) {
		if (g_array_index(live_set, char *, i) == variable) {
			return true;
		}
	}
	return false;
}

static void live_set_add(char *variable)
{
	if (!live_set_contains(variable))
		g_array_append_val(live_set, variable);
}

static void live_set_print(void)
{
	for (uint32_t i = 0; i < live_set->len; i++) {
		g_print("%s,", g_array_index(live_set, char *, i));
	}
	g_print("\n");
}

static void optimize_method(struct llir_iterator *iterator)
{
	g_print("method\n");
}

static void optimize_block(struct llir_iterator *iterator)
{
	g_print("block\n");
}

static void optimize_assignment(struct llir_iterator *iterator)
{
	g_print("assignment\n");
	if (iterator->assignment->type == LLIR_ASSIGNMENT_TYPE_METHOD_CALL)
		live_set_add(iterator->assignment->destination);

	if (!live_set_contains(iterator->assignment->destination))
		g_array_remove_index(iterator->block->assignments,
				     iterator->assignment_index);

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

static void optimize_terminal(struct llir_iterator *iterator)
{
	g_print("terminal\n");
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

void optimization_dead_code_elimination(struct llir *llir)
{
	// init live set
	live_set = g_array_new(false, false, sizeof(char *));

	// add all global fields to live set
	for (uint32_t i = 0; i < llir->fields->len; i++) {
		live_set_add(g_array_index(llir->fields, struct llir_field *, i)
				     ->identifier);
	}

	llir_iterate(llir, optimize_method, optimize_block, optimize_assignment,
		     optimize_terminal, false);

	live_set_print();

	g_array_free(live_set, true);
}
