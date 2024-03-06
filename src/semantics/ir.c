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

enum ir_data_type ir_type_from_ast(struct semantics *semantics)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_VOID ||
		 node->type == AST_NODE_TYPE_DATA_TYPE);
	if (node->type == AST_NODE_TYPE_VOID) {
		return IR_DATA_TYPE_VOID;
	} else {
		switch (node->token->type) {
		case TOKEN_TYPE_KEYWORD_INT:
			return IR_DATA_TYPE_INT;
		case TOKEN_TYPE_KEYWORD_BOOL:
			return IR_DATA_TYPE_BOOL;
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

static uint64_t string_to_int(struct semantics *semantics, char *string,
			      uint64_t max_value, uint64_t base)
{
	uint64_t value = 0;
	uint64_t i = strlen(string);
	uint64_t place = 1;

	while (i-- > 0) {
		uint64_t digit = character_to_int(string[i]);
		value += digit * place;

		if (value > max_value) {
			semantic_error(semantics, "Overflow in int literal");
			break;
		}

		place *= base;
	}

	return value;
}

int64_t ir_int_literal_from_ast(struct semantics *semantics, bool negate)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_INT_LITERAL);

	char *source_value = token_get_string(node->token, semantics->source);

	uint64_t max_value = negate ? -INT64_MIN : INT64_MAX;
	bool hex = node->token->type == TOKEN_TYPE_HEX_LITERAL;

	uint64_t value = string_to_int(semantics,
				       hex ? source_value + 2 : source_value,
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

char ir_char_literal_from_ast(struct semantics *semantics)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_CHAR_LITERAL);

	char *source_value = token_get_string(node->token, semantics->source);

	char value = '\0';
	if (source_value[1] == '\\')
		value = handle_backslash_sequence(source_value + 1);
	else
		value = source_value[1];

	g_free(source_value);
	return value;
}

char *ir_string_literal_from_ast(struct semantics *semantics)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_STRING_LITERAL);

	char *token_string = token_get_string(node->token, semantics->source);
	g_assert(token_string[0] == '\"');

	GString *string_literal = g_string_new(NULL);

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

	return char_data;
}

char *ir_identifier_from_ast(struct semantics *semantics)
{
	struct ast_node *node = next_node(semantics);
	g_assert(node->type == AST_NODE_TYPE_IDENTIFIER);
	return token_get_string(node->token, semantics->source);
}

static void iterate_fields(struct semantics *semantics, GArray *fields)
{
	g_assert(next_node(semantics)->type == AST_NODE_TYPE_FIELD);

	bool constant = peek_node(semantics)->type == AST_NODE_TYPE_CONST;
	if (constant)
		next_node(semantics);

	enum ir_data_type type = ir_type_from_ast(semantics);

	while (peek_node(semantics)->type == AST_NODE_TYPE_FIELD_IDENTIFIER) {
		struct ir_field *field =
			ir_field_new(semantics, constant, type);
		declare_field(semantics, field);
		g_array_append_val(fields, field);
	}
}

struct ir_program *ir_program_new(struct semantics *semantics)
{
	g_assert(next_node(semantics)->type == AST_NODE_TYPE_PROGRAM);

	struct ir_program *program = g_new(struct ir_program, 1);
	program->symbols = symbol_table_new(NULL);
	program->fields = g_array_new(false, false, sizeof(struct ir_field *));
	program->methods =
		g_array_new(false, false, sizeof(struct ir_method *));

	semantics_push_scope(semantics, program->symbols);

	while (peek_node(semantics)->type == AST_NODE_TYPE_IMPORT) {
		struct ir_method *method = ir_method_new_from_import(semantics);
		declare_method(semantics, method);
		g_array_append_val(program->methods, method);
	}

	while (peek_node(semantics)->type == AST_NODE_TYPE_FIELD)
		iterate_fields(semantics, program->fields);

	while (peek_node(semantics)->type == AST_NODE_TYPE_METHOD) {
		struct ir_method *method = ir_method_new(semantics);
		declare_method(semantics, method);
		g_array_append_val(program->methods, method);
	}

	semantics_pop_scope(semantics);

	g_assert(peek_node(semantics)->type == (uint32_t)-1);
	return program;
}

void ir_program_free(struct ir_program *program)
{
	for (uint32_t i = 0; i < program->methods->len; i++) {
		struct ir_method *method =
			g_array_index(program->methods, struct ir_method *, i);
		ir_method_free(method);
	}
	g_array_free(program->methods, true);

	for (uint32_t i = 0; i < program->fields->len; i++) {
		struct ir_field *field =
			g_array_index(program->fields, struct ir_field *, i);
		ir_field_free(field);
	}
	g_array_free(program->fields, true);

	symbol_table_free(program->symbols);
	g_free(program);
}

// Karl
struct ir_method *ir_method_new(struct semantics *semantics)
{
	return NULL;
}

struct ir_method *ir_method_new_from_import(struct semantics *semantics)
{
	g_assert(next_node(semantics)->type == AST_NODE_TYPE_IMPORT);

	struct ir_method *method = g_new(struct ir_method, 1);
	method->imported = true;
	method->return_type = IR_DATA_TYPE_INT;
	method->identifier = ir_identifier_from_ast(semantics);
	method->arguments = NULL;
	method->block = NULL;
	return method;
}

void ir_method_free(struct ir_method *method)
{
}

static int64_t *read_array_literal(struct semantics *semantics, size_t *length,
				   enum ir_data_type *type)
{
	g_assert(next_node(semantics)->type == AST_NODE_TYPE_ARRAY_LITERAL);

	GArray *values_array = g_array_new(false, false, sizeof(int64_t));
	enum ir_data_type array_type = -1;

	while (peek_node(semantics)->type == AST_NODE_TYPE_LITERAL) {
		enum ir_data_type literal_type;
		struct ir_literal *literal =
			ir_literal_new(semantics, &literal_type);

		if (array_type == (uint32_t)-1)
			array_type = literal_type;
		else if (array_type != literal_type)
			semantic_error(
				semantics,
				"Array literal does not contain uniform types");

		g_array_append_val(values_array, literal->value);
		ir_literal_free(literal);
	}

	int64_t *values = &g_array_index(values_array, int64_t, 0);
	*length = values_array->len;
	*type = array_type;

	g_array_free(values_array, false);
	return values;
}

static int64_t *read_initializer(struct semantics *semantics, bool *array,
				 size_t *length, enum ir_data_type *type)
{
	g_assert(next_node(semantics)->type == AST_NODE_TYPE_INITIALIZER);

	if (peek_node(semantics)->type == AST_NODE_TYPE_LITERAL) {
		int64_t *value = g_new(int64_t, 1);

		struct ir_literal *literal = ir_literal_new(semantics, type);
		*array = false;
		*length = 1;
		*value = literal->value;
		ir_literal_free(literal);

		return value;
	} else if (peek_node(semantics)->type == AST_NODE_TYPE_ARRAY_LITERAL) {
		*array = true;
		return read_array_literal(semantics, length, type);
	} else {
		g_assert(!"Invalid initializer type");
		return NULL;
	}
}

struct ir_field *ir_field_new(struct semantics *semantics, bool constant,
			      enum ir_data_type type)
{
	g_assert(next_node(semantics)->type == AST_NODE_TYPE_FIELD_IDENTIFIER);

	struct ir_field *field = g_new(struct ir_field, 1);
	field->constant = constant;
	field->type = type;
	field->identifier = ir_identifier_from_ast(semantics);

	if (peek_node(semantics)->type == AST_NODE_TYPE_INT_LITERAL) {
		field->array = true;
		field->array_length = ir_int_literal_from_ast(semantics, false);
		if (field->array_length < 1) {
			semantic_error(semantics,
				       "Array length must be greater than 0");
			field->array_length = 1;
		}
	} else if (peek_node(semantics)->type ==
		   AST_NODE_TYPE_EMPTY_ARRAY_LENGTH) {
		field->array = true;
		field->array_length = 0;
		next_node(semantics);
	} else {
		field->array = false;
		field->array_length = 0;
	}

	if (peek_node(semantics)->type == AST_NODE_TYPE_INITIALIZER) {
		bool initializer_array;
		size_t initializer_length;
		enum ir_data_type initializer_type;
		int64_t *initializers = read_initializer(semantics,
							 &initializer_array,
							 &initializer_length,
							 &initializer_type);

		if (initializer_array && field->array &&
		    field->array_length > 0)
			semantic_error(
				semantics,
				"Array initializer cannot be used to initialize field with declared length");
		if (initializer_array && !field->array)
			semantic_error(
				semantics,
				"Array initializer cannot be used to initialize non-array field");
		if (!initializer_array && field->array)
			semantic_error(
				semantics,
				"Non-array initializer cannot be used to initialize array field");
		if (initializer_type != field->type)
			semantic_error(
				semantics,
				"Initializer type does not match field type");

		if (initializer_array)
			field->array_length = initializer_length;
		field->initializers = initializers;
	} else {
		if (field->array && field->array_length == 0)
			semantic_error(
				semantics,
				"Missing array literal for field declaration");

		field->initializers = NULL;
	}

	return field;
}

void ir_field_free(struct ir_field *field)
{
	g_free(field->identifier);
	g_free(field->initializers);
	g_free(field);
}

// Karl
struct ir_block *ir_block_new(struct semantics *semantics)
{
	return NULL;
}

void ir_block_free(struct ir_block *block)
{
}

struct ir_statement *ir_statement_new(struct semantics *semantics)
{
	g_assert(next_node(semantics)->type == AST_NODE_TYPE_STATEMENT);

	struct ir_statement *statement = g_new(struct ir_statement, 1);

	switch (peek_node(semantics)->type) {
	case AST_NODE_TYPE_IF_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_IF;
		statement->if_statement = ir_if_statement_new(semantics);
		break;
	case AST_NODE_TYPE_FOR_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_FOR;
		statement->for_statement = ir_for_statement_new(semantics);
		break;
	case AST_NODE_TYPE_WHILE_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_WHILE;
		statement->while_statement = ir_while_statement_new(semantics);
		break;
	case AST_NODE_TYPE_RETURN_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_RETURN;
		statement->return_expression =
			ir_expression_new(semantics, NULL);
		break;
	case AST_NODE_TYPE_BREAK_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_BREAK;
		break;
	case AST_NODE_TYPE_CONTINUE_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_CONTINUE;
		break;
	case AST_NODE_TYPE_METHOD_CALL:
		statement->type = IR_STATEMENT_TYPE_METHOD_CALL;
		statement->method_call = ir_method_call_new(semantics, NULL);
		break;
	case AST_NODE_TYPE_ASSIGN_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_ASSIGNMENT;
		statement->assignment = ir_assignment_new(semantics);
		break;
	default:
		statement->type = -1;
		g_assert(!"Invalid statement type");
		break;
	}

	return statement;
}

void ir_statement_free(struct ir_statement *statement)
{
	switch (statement->type) {
	case IR_STATEMENT_TYPE_IF:
		ir_if_statement_free(statement->if_statement);
		break;
	case IR_STATEMENT_TYPE_FOR:
		ir_for_statement_free(statement->for_statement);
		break;
	case IR_STATEMENT_TYPE_WHILE:
		ir_while_statement_free(statement->while_statement);
		break;
	case IR_STATEMENT_TYPE_RETURN:
		ir_expression_free(statement->return_expression);
		break;
	case IR_STATEMENT_TYPE_METHOD_CALL:
		ir_method_call_free(statement->method_call);
		break;
	case IR_STATEMENT_TYPE_ASSIGNMENT:
		ir_assignment_free(statement->assignment);
		break;
	default:
		break;
	}
	g_free(statement);
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
struct ir_method_call *ir_method_call_new(struct semantics *semantics,
					  enum ir_data_type *out_data_type)
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
struct ir_location *ir_location_new(struct semantics *semantics,
				    enum ir_data_type *out_data_type)
{
	return NULL;
}

void ir_location_free(struct ir_location *location)
{
}

// Karl
struct ir_expression *ir_expression_new(struct semantics *semantics,
					enum ir_data_type *out_data_type)
{
	return NULL;
}

void ir_expression_free(struct ir_expression *expression)
{
}

// John
struct ir_binary_expression *
ir_binary_expression_new(struct semantics *semantics,
			 enum ir_data_type *out_data_type)
{
	return NULL;
}

void ir_binary_expression_free(struct ir_binary_expression *expression)
{
}

struct ir_literal *ir_literal_new(struct semantics *semantics,
				  enum ir_data_type *out_data_type)
{
	g_assert(next_node(semantics)->type == AST_NODE_TYPE_LITERAL);

	struct ast_node *node = peek_node(semantics);

	bool negate = false;

	if (node->type == AST_NODE_TYPE_LITERAL_NEGATION) {
		negate = true;
		node = next_node(semantics);
	}

	struct ir_literal *literal = g_new(struct ir_literal, 1);

	switch (node->type) {
	case AST_NODE_TYPE_INT_LITERAL:
		literal->type = IR_LITERAL_TYPE_INT;
		literal->value = ir_int_literal_from_ast(semantics, negate);
		*out_data_type = IR_DATA_TYPE_INT;
		break;

	case AST_NODE_TYPE_BOOL_LITERAL:
		literal->type = IR_LITERAL_TYPE_BOOL;
		literal->value = ir_bool_literal_from_ast(semantics);
		*out_data_type = IR_DATA_TYPE_BOOL;
		break;

	case AST_NODE_TYPE_CHAR_LITERAL:
		literal->type = IR_LITERAL_TYPE_CHAR;
		literal->value = ir_char_literal_from_ast(semantics);
		*out_data_type = IR_DATA_TYPE_INT;
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
