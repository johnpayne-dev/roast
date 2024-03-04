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

static struct ast_node *peek_node(struct semantics *semantics)
{
	g_assert(semantics->position < semantics->nodes->len);
	return g_array_index(semantics->nodes, struct ast_node *,
			     semantics->position);
}

static void declare_method(struct semantics *semantics,
			   struct ir_method *method, symbol_table_t *symbols)
{
	if (symbol_table_get_method(symbols, method->identifier))
		semantic_error(semantics, "Redeclaration of method");
	else if (symbol_table_get_field(symbols, method->identifier))
		semantic_error(semantics,
			       "Identifier already declared as field");
	else
		symbol_table_add_method(symbols, method->identifier, method);
}

static void declare_field(struct semantics *semantics, struct ir_field *field,
			  symbol_table_t *symbols)
{
	if (symbol_table_get_field(symbols, field->identifier))
		semantic_error(semantics, "Redeclaration of field");
	else if (symbol_table_get_method(symbols, field->identifier))
		semantic_error(semantics,
			       "Identifier already declared as method");
	else
		symbol_table_add_field(symbols, field->identifier, field);
}

enum ir_type ir_type_from_ast(struct semantics *semantics)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_VOID ||
		 node->type == AST_NODE_TYPE_TYPE);
	if (node->type == AST_NODE_TYPE_VOID) {
		return IR_TYPE_VOID;
	} else {
		switch (node->token->type) {
		case TOKEN_TYPE_KEYWORD_INT:
			return IR_TYPE_INT;
		case TOKEN_TYPE_KEYWORD_BOOL:
			return IR_TYPE_BOOL;
		default:
			break;
		}
	}
	g_assert(!"Couldn't extract data type from ast node");
	return -1;
}

static uint64_t character_to_int(char character)
{
	if (character >= 'a' && character <= 'f')
		return character - 'a';
	else if (character >= 'A' && character <= 'F')
		return character - 'A';
	else
		return character - '0';
}

static uint64_t string_to_int(struct semantics *semantics,
			      struct ast_node *node, char *string,
			      uint64_t max_value, uint64_t base)
{
	uint64_t value = 0;
	uint64_t i = strlen(string);
	uint64_t place = 1;

	while (i-- > 0) {
		uint64_t digit = character_to_int(string[i]);
		value += digit * place;

		if (value > max_value) {
			semantic_error(semantics, "Overflow in int literal",
				       node);
			break;
		}

		place *= base;
	}

	return value;
}

int64_t ir_int_literal_from_ast(struct semantics *semantics, bool negate)
{
	g_assert(node->type == AST_NODE_TYPE_INT_LITERAL);
	g_assert(node->token != NULL);

	char *source_value = token_get_string(node->token, semantics->source);

	uint64_t max_value = negate ? -INT64_MIN : INT64_MAX;
	bool hex = node->token->type == TOKEN_TYPE_HEX_LITERAL;

	uint64_t value = string_to_int(semantics, node,
				       hex ? source_value : source_value + 2,
				       max_value, hex ? 16 : 10);

	g_free(source_value);
	return (negate ? -1 : 1) * (int64_t)value;
}

bool ir_bool_literal_from_ast(struct semantics *semantics)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_BOOL_LITERAL);
	switch (node->token->type) {
	case TOKEN_TYPE_KEYWORD_TRUE:
		return true;
	case TOKEN_TYPE_KEYWORD_FALSE:
		return false;
	default:
		break;
	}
	g_assert(!"Couldn't extract bool literal from ast node");
	return -1;
}

static char handle_backslash_sequence(char *backlash_sequence)
{
	g_assert(backlash_sequence[0] == '\\');
	switch (backlash_sequence[1]) {
	case '\"':
		return '\"';
	case '\'':
		return '\'';
	case '\\':
		return '\\';
	case 't':
		return '\t';
	case 'n':
		return '\n';
	default:
		break;
	}
	g_assert(!"Invalid backslash sequence");
	return -1;
}

// John
char ir_char_literal_from_ast(struct semantics *semantics)
{
	return '\0';
}

char *ir_string_literal_from_ast(struct semantics *semantics)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_STRING_LITERAL);

	char *token_string = token_get_string(node->token, semantics->source);
	g_assert(token_string != NULL || token_string[0] == '\"');

	GString *string_literal = g_string_new(NULL);
	g_assert(string_literal != NULL);

	for (char *c = &token_string[1]; *c != '\"'; ++c) {
		if (*c == '\\') {
			g_string_append_c(string_literal,
					  handle_backslash_sequence(c));
			++c;
		} else {
			g_string_append_c(string_literal, *c);
		}
	}

	g_free(token_string);

	char *char_data = string_literal->str;

	g_string_free(string_literal, FALSE);

	g_assert(char_data != NULL);

	return char_data;
}

// John
char *ir_identifier_from_ast(struct semantics *semantics)
{
	return NULL;
}

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

struct ir_literal *ir_literal_new(struct semantics *semantics,
				  enum ir_type *out_data_type)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_INT_LITERAL ||
		 node->type == AST_NODE_TYPE_BOOL_LITERAL ||
		 node->type == AST_NODE_TYPE_CHAR_LITERAL);
	struct ir_literal *literal = g_new(struct ir_literal, 1);
	g_assert(literal != NULL);
	switch (node->type) {
	case AST_NODE_TYPE_INT_LITERAL:
		literal->type = IR_LITERAL_TYPE_INT;
		literal->int_literal = ir_int_literal_from_ast(semantics);
		*out_data_type = IR_TYPE_INT;
		break;
	case AST_NODE_TYPE_BOOL_LITERAL:
		literal->type = IR_LITERAL_TYPE_BOOL;
		literal->bool_literal = ir_bool_literal_from_ast(semantics);
		*out_data_type = IR_TYPE_BOOL;
		break;
	case AST_NODE_TYPE_CHAR_LITERAL:
		literal->type = IR_LITERAL_TYPE_CHAR;
		literal->char_literal = ir_char_literal_from_ast(semantics);
		*out_data_type = IR_TYPE_INT;
		break;
	default:
		g_assert(!"Couldn't extract literal from node");
		return NULL;
	}
	return literal;
}

void ir_literal_free(struct ir_literal *literal)
{
	g_free(literal);
}
