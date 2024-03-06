#include "semantics/ir.h"

static struct ast_node *next_node(struct ast_node **nodes)
{
	return (*nodes)++;
}

static struct ast_node *peek_node(struct ast_node **nodes)
{
	return *nodes;
}

static void iterate_fields(struct ast_node **nodes, GArray *fields)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_FIELD);

	bool constant = peek_node(nodes)->type == AST_NODE_TYPE_CONST;
	if (constant)
		next_node(nodes);

	enum ir_data_type type = ir_data_type_from_ast(nodes);

	while (peek_node(nodes)->type == AST_NODE_TYPE_FIELD_IDENTIFIER) {
		struct ir_field *field = ir_field_new(nodes, constant, type);
		g_array_append_val(fields, field);
	}
}

struct ir_program *ir_program_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_PROGRAM);

	struct ir_program *program = g_new(struct ir_program, 1);
	program->fields_table = fields_table_new(NULL);
	program->fields = g_array_new(false, false, sizeof(struct ir_field *));
	program->methods_table = methods_table_new();
	program->methods =
		g_array_new(false, false, sizeof(struct ir_method *));

	while (peek_node(nodes)->type == AST_NODE_TYPE_IMPORT) {
		struct ir_method *method = ir_method_new_from_import(nodes);
		g_array_append_val(program->methods, method);
	}

	while (peek_node(nodes)->type == AST_NODE_TYPE_FIELD)
		iterate_fields(nodes, program->fields);

	while (peek_node(nodes)->type == AST_NODE_TYPE_METHOD) {
		struct ir_method *method = ir_method_new(nodes);
		g_array_append_val(program->methods, method);
	}

	g_assert(peek_node(nodes)->type == AST_NODE_TYPE_END);
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

	fields_table_free(program->fields_table);
	methods_table_free(program->methods_table);
	g_free(program);
}

// Karl
struct ir_method *ir_method_new(struct ast_node **nodes)
{
	g_assert(0);
	return NULL;
}

struct ir_method *ir_method_new_from_import(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_IMPORT);

	struct ir_method *method = g_new(struct ir_method, 1);
	method->imported = true;
	method->return_type = IR_DATA_TYPE_INT;
	method->identifier = ir_identifier_from_ast(nodes);
	method->arguments = NULL;
	method->block = NULL;
	return method;
}

void ir_method_free(struct ir_method *method)
{
	g_assert(0);
}

struct ir_field *ir_field_new(struct ast_node **nodes, bool constant,
			      enum ir_data_type type)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_FIELD_IDENTIFIER);

	struct ir_field *field = g_new(struct ir_field, 1);
	field->constant = constant;
	field->type = type;
	field->identifier = ir_identifier_from_ast(nodes);

	if (peek_node(nodes)->type == AST_NODE_TYPE_INT_LITERAL) {
		field->array = true;
		field->array_length = ir_int_literal_from_ast(nodes, false);
	} else if (peek_node(nodes)->type == AST_NODE_TYPE_EMPTY_ARRAY_LENGTH) {
		field->array = true;
		field->array_length = -1;
		next_node(nodes);
	} else {
		field->array = false;
		field->array_length = 1;
	}

	if (peek_node(nodes)->type == AST_NODE_TYPE_INITIALIZER)
		field->initializer = ir_initializer_new(nodes);
	else
		field->initializer = NULL;

	return field;
}

void ir_field_free(struct ir_field *field)
{
	g_free(field->identifier);
	if (field->initializer != NULL)
		ir_initializer_free(field->initializer);
	g_free(field);
}

struct ir_initializer *ir_initializer_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_INITIALIZER);

	struct ir_initializer *initializer = g_new(struct ir_initializer, 1);
	initializer->literals =
		g_array_new(false, false, sizeof(struct ir_literal *));
	initializer->array = peek_node(nodes)->type ==
			     AST_NODE_TYPE_ARRAY_LITERAL;
	if (initializer->array)
		next_node(nodes);

	while (peek_node(nodes)->type == AST_NODE_TYPE_LITERAL) {
		struct ir_literal *literal = ir_literal_new(nodes);
		g_array_append_val(initializer->literals, literal);
	}

	return initializer;
}

void ir_initializer_free(struct ir_initializer *initializer)
{
	for (uint32_t i = 0; i < initializer->literals->len; i++) {
		struct ir_literal *literal = g_array_index(
			initializer->literals, struct ir_literal *, i);
		ir_literal_free(literal);
	}
	g_array_free(initializer->literals, true);
	g_free(initializer);
}

// Karl
struct ir_block *ir_block_new(struct ast_node **nodes)
{
	g_assert(0);
	return NULL;
}

void ir_block_free(struct ir_block *block)
{
	g_assert(0);
}

struct ir_statement *ir_statement_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_STATEMENT);

	struct ir_statement *statement = g_new(struct ir_statement, 1);

	switch (peek_node(nodes)->type) {
	case AST_NODE_TYPE_IF_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_IF;
		statement->if_statement = ir_if_statement_new(nodes);
		break;
	case AST_NODE_TYPE_FOR_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_FOR;
		statement->for_statement = ir_for_statement_new(nodes);
		break;
	case AST_NODE_TYPE_WHILE_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_WHILE;
		statement->while_statement = ir_while_statement_new(nodes);
		break;
	case AST_NODE_TYPE_RETURN_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_RETURN;
		statement->return_expression = ir_expression_new(nodes);
		break;
	case AST_NODE_TYPE_BREAK_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_BREAK;
		break;
	case AST_NODE_TYPE_CONTINUE_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_CONTINUE;
		break;
	case AST_NODE_TYPE_METHOD_CALL:
		statement->type = IR_STATEMENT_TYPE_METHOD_CALL;
		statement->method_call = ir_method_call_new(nodes);
		break;
	case AST_NODE_TYPE_ASSIGN_STATEMENT:
		statement->type = IR_STATEMENT_TYPE_ASSIGNMENT;
		statement->assignment = ir_assignment_new(nodes);
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
struct ir_assignment *ir_assignment_new(struct ast_node **nodes)
{
	g_assert(0);
	return NULL;
}

void ir_assignment_free(struct ir_assignment *assignment)
{
	g_assert(0);
}

struct ir_method_call *ir_method_call_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_METHOD_CALL);

	struct ir_method_call *call = g_new(struct ir_method_call, 1);
	call->identifier = ir_identifier_from_ast(nodes);
	call->arguments = g_array_new(false, false,
				      sizeof(struct ir_method_call_argument *));

	while (peek_node(nodes)->type == AST_NODE_TYPE_METHOD_CALL_ARGUMENT) {
		struct ir_method_call_argument *argument =
			ir_method_call_argument_new(nodes);
		g_array_append_val(call->arguments, argument);
	}

	return call;
}

void ir_method_call_free(struct ir_method_call *call)
{
	g_free(call->identifier);
	for (uint32_t i = 0; i < call->arguments->len; i++) {
		struct ir_method_call_argument *argument = g_array_index(
			call->arguments, struct ir_method_call_argument *, i);
		ir_method_call_argument_free(argument);
	}
	g_array_free(call->arguments, true);
	g_free(call);
}

struct ir_method_call_argument *
ir_method_call_argument_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_METHOD_CALL_ARGUMENT);

	struct ir_method_call_argument *argument =
		g_new(struct ir_method_call_argument, 1);

	if (peek_node(nodes)->type == AST_NODE_TYPE_STRING_LITERAL) {
		argument->type = IR_METHOD_CALL_ARGUMENT_TYPE_STRING;
		argument->string = ir_string_literal_from_ast(nodes);
	} else if (peek_node(nodes)->type == AST_NODE_TYPE_EXPRESSION) {
		argument->type = IR_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION;
		argument->expression = ir_expression_new(nodes);
	} else {
		g_assert(!"Invalid node type in method call argument");
	}

	return argument;
}

void ir_method_call_argument_free(struct ir_method_call_argument *argument)
{
	switch (argument->type) {
	case IR_METHOD_CALL_ARGUMENT_TYPE_STRING:
		g_free(argument->string);
		break;
	case IR_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION:
		ir_expression_free(argument->expression);
		break;
	default:
		break;
	}
	g_free(argument);
}

// Karl
struct ir_if_statement *ir_if_statement_new(struct ast_node **nodes)
{
	g_assert(0);
	return NULL;
}

void ir_if_statement_free(struct ir_if_statement *statement)
{
	g_assert(0);
}

struct ir_for_statement *ir_for_statement_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_FOR_STATEMENT);

	struct ir_for_statement *statement = g_new(struct ir_for_statement, 1);

	statement->identifier = ir_identifier_from_ast(nodes);
	statement->initializer = ir_expression_new(nodes);
	statement->condition = ir_expression_new(nodes);
	statement->update = ir_assignment_new(nodes);
	statement->block = ir_block_new(nodes);

	return statement;
}

void ir_for_statement_free(struct ir_for_statement *statement)
{
	ir_block_free(statement->block);
	ir_assignment_free(statement->update);
	ir_expression_free(statement->condition);
	ir_expression_free(statement->initializer);
	g_free(statement->identifier);
	g_free(statement);
}

// Karl
struct ir_while_statement *ir_while_statement_new(struct ast_node **nodes)
{
	g_assert(0);
	return NULL;
}

void ir_while_statement_free(struct ir_while_statement *statement)
{
	g_assert(0);
}

struct ir_location *ir_location_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_LOCATION);

	struct ir_location *location = g_new(struct ir_location, 1);
	location->identifier = ir_identifier_from_ast(nodes);
	location->index = NULL;

	if (peek_node(nodes)->type == AST_NODE_TYPE_EXPRESSION)
		location->index = ir_expression_new(nodes);

	return location;
}

void ir_location_free(struct ir_location *location)
{
	g_free(location->identifier);
	if (location->index != NULL)
		ir_expression_free(location->index);
	g_free(location);
}

// Karl
struct ir_expression *ir_expression_new(struct ast_node **nodes)
{
	g_assert(0);
	return NULL;
}

void ir_expression_free(struct ir_expression *expression)
{
	g_assert(0);
}

// John
struct ir_binary_expression *ir_binary_expression_new(struct ast_node **nodes)
{
	g_assert(0);
	return NULL;
}

void ir_binary_expression_free(struct ir_binary_expression *expression)
{
	g_assert(0);
}

struct ir_literal *ir_literal_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_LITERAL);

	struct ast_node *node = peek_node(nodes);

	struct ir_literal *literal = g_new(struct ir_literal, 1);
	literal->negate = false;

	if (node->type == AST_NODE_TYPE_LITERAL_NEGATION) {
		literal->negate = true;
		next_node(nodes);
	}

	switch (peek_node(nodes)->type) {
	case AST_NODE_TYPE_INT_LITERAL:
		literal->type = IR_LITERAL_TYPE_INT;
		literal->value =
			ir_int_literal_from_ast(nodes, literal->negate);
		break;

	case AST_NODE_TYPE_BOOL_LITERAL:
		literal->type = IR_LITERAL_TYPE_BOOL;
		literal->value = ir_bool_literal_from_ast(nodes);
		break;

	case AST_NODE_TYPE_CHAR_LITERAL:
		literal->type = IR_LITERAL_TYPE_CHAR;
		literal->value = ir_char_literal_from_ast(nodes);
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

enum ir_data_type ir_data_type_from_ast(struct ast_node **nodes)
{
	struct ast_node *node = next_node(nodes);
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

static uint64_t string_to_int(char *string, uint64_t max_value, uint64_t base)
{
	uint64_t value = 0;
	uint64_t i = strlen(string);
	uint64_t place = 1;

	while (i-- > 0) {
		uint64_t digit = character_to_int(string[i]);
		value += digit * place;

		if (value > max_value)
			break;

		place *= base;
	}

	return value;
}

uint64_t ir_int_literal_from_ast(struct ast_node **nodes, bool negate)
{
	struct ast_node *node = next_node(nodes);
	g_assert(node->type == AST_NODE_TYPE_INT_LITERAL);

	char *source_value = token_get_string(node->token);

	uint64_t max_value = negate ? -INT64_MIN : INT64_MAX;
	bool hex = node->token->type == TOKEN_TYPE_HEX_LITERAL;

	uint64_t value = string_to_int(hex ? source_value + 2 : source_value,
				       max_value, hex ? 16 : 10);

	g_free(source_value);
	return value;
}

bool ir_bool_literal_from_ast(struct ast_node **nodes)
{
	struct ast_node *node = next_node(nodes);
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

char ir_char_literal_from_ast(struct ast_node **nodes)
{
	struct ast_node *node = next_node(nodes);
	g_assert(node->type == AST_NODE_TYPE_CHAR_LITERAL);

	char *source_value = token_get_string(node->token);

	char value = '\0';
	if (source_value[1] == '\\')
		value = handle_backslash_sequence(source_value + 1);
	else
		value = source_value[1];

	g_free(source_value);
	return value;
}

char *ir_string_literal_from_ast(struct ast_node **nodes)
{
	struct ast_node *node = next_node(nodes);
	g_assert(node->type == AST_NODE_TYPE_STRING_LITERAL);

	char *token_string = token_get_string(node->token);
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

char *ir_identifier_from_ast(struct ast_node **nodes)
{
	struct ast_node *node = next_node(nodes);
	g_assert(node->type == AST_NODE_TYPE_IDENTIFIER);
	return token_get_string(node->token);
}
