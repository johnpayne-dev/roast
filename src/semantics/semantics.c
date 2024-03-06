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
	fields_table_t *fields_table = semantics_current_scope(semantics);

	g_assert(fields_table_get_parent(fields_table) == NULL);

	if (methods_table_get(semantics->methods_table, method->identifier))
		semantic_error(semantics, "Redeclaration of method");
	else if (fields_table_get(fields_table, method->identifier, false))
		semantic_error(semantics,
			       "Identifier already declared as field");
	else
		methods_table_add(semantics->methods_table, method->identifier,
				  method);
}

static void declare_field(struct semantics *semantics, struct ir_field *field)
{
	fields_table_t *fields_table = semantics_current_scope(semantics);

	if (methods_table_get(semantics->methods_table, field->identifier))
		semantic_error(semantics, "Redeclaration of field");
	else if (fields_table_get(fields_table, field->identifier, true))
		semantic_error(semantics,
			       "Identifier already declared as method");
	else
		fields_table_add(fields_table, field->identifier, field);
}

static struct ir_method *get_method_declaration(struct semantics *semantics,
						char *identifier)
{
	struct ir_method *method =
		methods_table_get(semantics->methods_table, identifier);
	if (method == NULL)
		semantic_error(semantics, "Undeclared method");

	return method;
}

static struct ir_field *get_field_declaration(struct semantics *semantics,
					      char *identifier)
{
	fields_table_t *fields_table = semantics_current_scope(semantics);
	struct ir_field *field =
		fields_table_get(fields_table, identifier, true);
	if (field == NULL)
		semantic_error(semantics, "Undeclared field");

	return field;
}

static void analyze_program(struct semantics *semantics,
			    struct ir_program *program)
{
}

struct semantics *semantics_new(void)
{
	struct semantics *semantics = g_new(struct semantics, 1);
	semantics->fields_table_stack =
		g_array_new(false, false, sizeof(fields_table_t *));
	return semantics;
}

int semantics_analyze(struct semantics *semantics, struct ast_node *ast,
		      struct ir_program **ir)
{
	struct ast_node *linear_nodes = ast_node_linearize(ast);
	struct ir_program *program =
		ir_program_new(&(struct ast_node *){ linear_nodes });
	g_free(linear_nodes);

	semantics->error = false;
	semantics->methods_table = program->methods_table;
	analyze_program(semantics, program);

	if (semantics->error || ir == NULL)
		ir_program_free(program);
	else
		*ir = program;

	return semantics->error ? -1 : 0;
}

void semantics_push_scope(struct semantics *semantics,
			  fields_table_t *fields_table)
{
	g_array_append_val(semantics->fields_table_stack, fields_table);
}

fields_table_t *semantics_current_scope(struct semantics *semantics)
{
	return g_array_index(semantics->fields_table_stack, fields_table_t *,
			     semantics->fields_table_stack->len - 1);
}

void semantics_pop_scope(struct semantics *semantics)
{
	g_array_remove_index(semantics->fields_table_stack,
			     semantics->fields_table_stack->len - 1);
}

void semantics_free(struct semantics *semantics)
{
	g_array_free(semantics->fields_table_stack, true);
	g_free(semantics);
}
