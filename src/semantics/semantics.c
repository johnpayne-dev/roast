#include "semantics/semantics.h"
#include "semantics/ir.h"

static void semantic_error(struct semantics *semantics, const char *message)
{
	semantics->error = true;
	g_printerr("ERROR at %i:%i: %s\n", 0, 0, message);
}

static void declare_method(struct semantics *semantics,
			   struct ir_method *method)
{
	symbol_table_t *symbols = semantics_current_scope(semantics);

	if (symbol_table_get_method(symbols, method->identifier))
		semantic_error(semantics, "Redeclaration of method");
	else if (symbol_table_get_field(symbols, method->identifier))
		semantic_error(semantics,
			       "Identifier already declared as field");
	else
		symbol_table_add_method(symbols, method->identifier, method);
}

static void declare_field(struct semantics *semantics, struct ir_field *field)
{
	symbol_table_t *symbols = semantics_current_scope(semantics);

	if (symbol_table_get_field(symbols, field->identifier))
		semantic_error(semantics, "Redeclaration of field");
	else if (symbol_table_get_method(symbols, field->identifier))
		semantic_error(semantics,
			       "Identifier already declared as method");
	else
		symbol_table_add_field(symbols, field->identifier, field);
}

static struct ir_method *get_method_declaration(struct semantics *semantics,
						char *identifier)
{
	symbol_table_t *symbols = semantics_current_scope(semantics);
	struct ir_method *method = symbol_table_get_method(symbols, identifier);
	if (method == NULL)
		semantic_error(semantics, "Undeclared method");

	return method;
}

static struct ir_field *get_field_declaration(struct semantics *semantics,
					      char *identifier)
{
	symbol_table_t *symbols = semantics_current_scope(semantics);
	struct ir_field *field = NULL;

	while (field == NULL && symbols != NULL) {
		field = symbol_table_get_field(symbols, identifier);
		symbols = symbol_table_get_parent(symbols);
	}

	if (field == NULL)
		semantic_error(semantics, "Undeclared field");

	return field;
}

struct semantics *semantics_new(void)
{
	struct semantics *semantics = g_new(struct semantics, 1);
	semantics->symbol_table_stack =
		g_array_new(false, false, sizeof(symbol_table_t *));
	return semantics;
}

int semantics_analyze(struct semantics *semantics, struct ast_node *ast,
		      struct ir_program **ir)
{
	semantics->error = false;
	semantics->nodes = g_array_new(false, false, sizeof(struct ast_node *));
	semantics->position = 0;

	struct ast_node *linear_nodes = ast_node_linearize(ast);

	struct ast_node *head = linear_nodes;
	struct ir_program *program = ir_program_new(&head);

	if (semantics->error || ir == NULL)
		ir_program_free(program);
	else
		*ir = program;

	g_free(linear_nodes);
	return semantics->error ? -1 : 0;
}

void semantics_push_scope(struct semantics *semantics, symbol_table_t *table)
{
	g_array_append_val(semantics->symbol_table_stack, table);
}

symbol_table_t *semantics_current_scope(struct semantics *semantics)
{
	return g_array_index(semantics->symbol_table_stack, symbol_table_t *,
			     semantics->symbol_table_stack->len - 1);
}

void semantics_pop_scope(struct semantics *semantics)
{
	g_array_remove_index(semantics->symbol_table_stack,
			     semantics->symbol_table_stack->len - 1);
}

void semantics_free(struct semantics *semantics)
{
	g_array_free(semantics->symbol_table_stack, true);
	g_free(semantics);
}
