#include "semantics/ir.h"
#include "semantics/semantics.h"

static void semantic_error(struct semantics *semantics, const char *message)
{
	semantics->error = true;
	g_printerr("ERROR at %i:%i: %s\n", 0, 0, message);
}

static struct ast_node *next_node(struct semantics *semantics)
{
	g_assert(semantics->position < semantics->nodes->len);
	return g_array_index(semantics->nodes, struct ast_node *,
			     semantics->position++);
}

static void declare_method(struct semantics *semantics,
			   struct ir_method *method, symbol_table_t *symbols)
{
	if (symbol_table_get_method(symbols, method->identifier))
		semantic_error(semantics, "Redeclaration of method");
	else
		symbol_table_add_method(symbols, method->identifier, method);
}

static void declare_field(struct semantics *semantics, struct ir_field *field,
			  symbol_table_t *symbols)
{
	if (symbol_table_get_field(symbols, field->identifier))
		semantic_error(semantics, "Redeclaration of field");
	else
		symbol_table_add_field(symbols, field->identifier, field);
}

// Karl
enum ir_type ir_type_from_ast(struct semantics *semantics)
{
	return 0;
}

// John
int64_t ir_int_literal_from_ast(struct semantics *semantics)
{
	return 0;
}

// Karl
bool ir_bool_literal_from_ast(struct semantics *semantics)
{
	return false;
}

// John
char ir_char_literal_from_ast(struct semantics *semantics)
{
	return '\0';
}

// Karl
char *ir_string_literal_from_ast(struct semantics *semantics)
{
	return NULL;
}

// John
struct ir_program *ir_program_new(struct semantics *semantics)
{
	return NULL;
}

void ir_program_free(struct ir_program *program)
{
}

// Karl
struct ir_method *ir_method_new(struct semantics *semantics)
{
	return NULL;
}

void ir_method_free(struct ir_method *method)
{
}

// John
struct ir_field *ir_field_new(struct semantics *semantics)
{
	return NULL;
}

void ir_field_free(struct ir_field *field)
{
}

// Karl
struct ir_block *ir_block_new(struct semantics *semantics)
{
	return NULL;
}

void ir_block_free(struct ir_block *block)
{
}

// John
struct ir_statement *ir_statement_new(struct semantics *semantics)
{
	return NULL;
}

void ir_statement_free(struct ir_statement *statement)
{
}

// Karl
struct ir_assignment *ir_assignment_new(struct semantics *semantics)
{
	return NULL;
}

void ir_assignment_free(struct ir_assignment *assignment)
{
}

// John
struct ir_method_call *ir_method_call_new(struct semantics *semantics)
{
	return NULL;
}

void ir_method_call_free(struct ir_method_call *method_call)
{
}

// Karl
struct ir_if_statement *ir_if_statement_new(struct semantics *semantics)
{
	return NULL;
}

void ir_if_statement_free(struct ir_if_statement *statement)
{
}

// John
struct ir_for_statement *ir_for_statement_new(struct semantics *semantics)
{
	return NULL;
}

void ir_for_statement_free(struct ir_for_statement *statement)
{
}

// Karl
struct ir_while_statement *ir_while_statement_new(struct semantics *semantics)
{
	return NULL;
}

void ir_while_statement_free(struct ir_while_statement *statement)
{
}

// John
struct ir_location *ir_location_new(struct semantics *semantics)
{
	return NULL;
}

void ir_location_free(struct ir_location *location)
{
}

// Karl
struct ir_expression *ir_expression_new(struct semantics *semantics)
{
	return NULL;
}

void ir_expression_free(struct ir_expression *expression)
{
}

// John
struct ir_binary_expression *
ir_binary_expression_new(struct semantics *semantics)
{
	return NULL;
}

void ir_binary_expression_free(struct ir_binary_expression *expression)
{
}

// Karl
struct ir_literal *ir_literal_new(struct semantics *semantics)
{
	return NULL;
}

void ir_literal_free(struct ir_literal *expression)
{
}
