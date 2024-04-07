#include "semantics/ir.h"

static struct ast_node *next_node(struct ast_node **nodes)
{
	return (*nodes)++;
}

static struct ast_node *peek_node(struct ast_node **nodes)
{
	return *nodes;
}

static struct ast_node *last_node(struct ast_node **nodes)
{
	return *nodes - 1;
}

bool ir_data_type_is_array(enum ir_data_type type)
{
	return type % 2 == 1;
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
	program->token = last_node(nodes)->token;
	program->fields_table = fields_table_new();
	program->methods_table = methods_table_new();
	program->imports =
		g_array_new(false, false, sizeof(struct ir_method *));
	program->fields = g_array_new(false, false, sizeof(struct ir_field *));
	program->methods =
		g_array_new(false, false, sizeof(struct ir_method *));

	while (peek_node(nodes)->type == AST_NODE_TYPE_IMPORT) {
		struct ir_method *method = ir_method_new_from_import(nodes);
		g_array_append_val(program->imports, method);
	}

	while (peek_node(nodes)->type == AST_NODE_TYPE_FIELD)
		iterate_fields(nodes, program->fields);

	while (peek_node(nodes)->type == AST_NODE_TYPE_METHOD) {
		struct ir_method *method = ir_method_new(nodes);
		g_array_append_val(program->methods, method);
	}

	g_assert(peek_node(nodes)->type == AST_NODE_TYPE_PROGRAM_END);
	return program;
}

void ir_program_free(struct ir_program *program)
{
	for (uint32_t i = 0; i < program->imports->len; i++) {
		struct ir_method *method =
			g_array_index(program->imports, struct ir_method *, i);
		ir_method_free(method);
	}
	g_array_free(program->imports, true);

	for (uint32_t i = 0; i < program->fields->len; i++) {
		struct ir_field *field =
			g_array_index(program->fields, struct ir_field *, i);
		ir_field_free(field);
	}
	g_array_free(program->fields, true);

	for (uint32_t i = 0; i < program->methods->len; i++) {
		struct ir_method *method =
			g_array_index(program->methods, struct ir_method *, i);
		ir_method_free(method);
	}
	g_array_free(program->methods, true);

	fields_table_free(program->fields_table);
	methods_table_free(program->methods_table);

	g_free(program);
}

struct ir_method *ir_method_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_METHOD);

	struct ir_method *method = g_new(struct ir_method, 1);
	method->token = last_node(nodes)->token;
	method->imported = false;
	method->return_type = ir_data_type_from_ast(nodes);
	method->identifier = ir_identifier_from_ast(nodes);

	method->arguments =
		g_array_new(false, false, sizeof(struct ir_method_argument *));

	while (peek_node(nodes)->type == AST_NODE_TYPE_METHOD_ARGUMENT) {
		struct ir_field *argument =
			ir_field_new_from_method_argument(nodes);
		g_array_append_val(method->arguments, argument);
	}

	method->block = ir_block_new(nodes);

	return method;
}

struct ir_method *ir_method_new_from_import(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_IMPORT);

	struct ir_method *method = g_new(struct ir_method, 1);
	method->token = last_node(nodes)->token;
	method->imported = true;
	method->return_type = IR_DATA_TYPE_INT;
	method->identifier = ir_identifier_from_ast(nodes);
	method->arguments = NULL;
	method->block = NULL;
	return method;
}

void ir_method_free(struct ir_method *method)
{
	g_free(method->identifier);
	if (!method->imported) {
		for (uint32_t i = 0; i < method->arguments->len; i++) {
			struct ir_field *argument = g_array_index(
				method->arguments, struct ir_field *, i);
			ir_field_free(argument);
		}
		g_array_free(method->arguments, true);
		ir_block_free(method->block);
	}
	g_free(method);
}

struct ir_field *ir_field_new(struct ast_node **nodes, bool constant,
			      enum ir_data_type type)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_FIELD_IDENTIFIER);

	struct ir_field *field = g_new(struct ir_field, 1);
	field->token = last_node(nodes)->token;
	field->constant = constant;
	field->type = type;
	field->identifier = ir_identifier_from_ast(nodes);

	if (peek_node(nodes)->type == AST_NODE_TYPE_INT_LITERAL) {
		field->type += 1;
		field->array_length = ir_int_literal_from_ast(nodes, false);
	} else if (peek_node(nodes)->type == AST_NODE_TYPE_EMPTY_ARRAY_LENGTH) {
		field->type += 1;
		field->array_length = -1;
		next_node(nodes);
	} else {
		field->array_length = 1;
	}

	if (peek_node(nodes)->type == AST_NODE_TYPE_INITIALIZER)
		field->initializer = ir_initializer_new(nodes);
	else
		field->initializer = NULL;

	return field;
}

struct ir_field *ir_field_new_from_method_argument(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_METHOD_ARGUMENT);

	struct ir_field *argument = g_new(struct ir_field, 1);
	argument->constant = false;
	argument->type = ir_data_type_from_ast(nodes);
	argument->identifier = ir_identifier_from_ast(nodes);
	argument->array_length = 1;
	argument->initializer = NULL;
	return argument;
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
	initializer->token = last_node(nodes)->token;
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

struct ir_block *ir_block_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_BLOCK);

	struct ir_block *block = g_new(struct ir_block, 1);
	block->token = last_node(nodes)->token;
	block->fields_table = fields_table_new();
	block->fields = g_array_new(false, false, sizeof(struct ir_field *));

	while (peek_node(nodes)->type == AST_NODE_TYPE_FIELD)
		iterate_fields(nodes, block->fields);

	block->statements =
		g_array_new(false, false, sizeof(struct ir_statements *));

	while (peek_node(nodes)->type == AST_NODE_TYPE_STATEMENT) {
		struct ir_statement *statement = ir_statement_new(nodes);
		g_array_append_val(block->statements, statement);
	}

	g_assert(next_node(nodes)->type == AST_NODE_TYPE_BLOCK_END);
	return block;
}

void ir_block_free(struct ir_block *block)
{
	fields_table_free(block->fields_table);

	for (uint32_t i = 0; i < block->fields->len; i++) {
		struct ir_field *field =
			g_array_index(block->fields, struct ir_field *, i);
		ir_field_free(field);
	}
	g_array_free(block->fields, true);

	for (uint32_t i = 0; i < block->statements->len; i++) {
		struct ir_statement *statement = g_array_index(
			block->statements, struct ir_statement *, i);
		ir_statement_free(statement);
	}
	g_array_free(block->statements, true);

	g_free(block);
}

struct ir_statement *ir_statement_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_STATEMENT);

	struct ir_statement *statement = g_new(struct ir_statement, 1);
	statement->token = last_node(nodes)->token;

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
		next_node(nodes);
		statement->type = IR_STATEMENT_TYPE_RETURN;
		if (peek_node(nodes)->type == AST_NODE_TYPE_EXPRESSION)
			statement->return_expression = ir_expression_new(nodes);
		else
			statement->return_expression = NULL;
		break;
	case AST_NODE_TYPE_BREAK_STATEMENT:
		next_node(nodes);
		statement->type = IR_STATEMENT_TYPE_BREAK;
		break;
	case AST_NODE_TYPE_CONTINUE_STATEMENT:
		next_node(nodes);
		statement->type = IR_STATEMENT_TYPE_CONTINUE;
		break;
	case AST_NODE_TYPE_METHOD_CALL:
		statement->type = IR_STATEMENT_TYPE_METHOD_CALL;
		statement->method_call = ir_method_call_new(nodes);
		break;
	case AST_NODE_TYPE_ASSIGNMENT:
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
		if (statement->return_expression != NULL)
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

struct ir_assignment *ir_assignment_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_ASSIGNMENT);

	struct ir_assignment *assignment = g_new(struct ir_assignment, 1);
	assignment->token = last_node(nodes)->token;
	assignment->location = ir_location_new(nodes);

	switch (next_node(nodes)->token.type) {
	case TOKEN_TYPE_ASSIGN:
		assignment->assign_operator = IR_ASSIGN_OPERATOR_SET;
		break;
	case TOKEN_TYPE_ADD_ASSIGN:
		assignment->assign_operator = IR_ASSIGN_OPERATOR_ADD;
		break;
	case TOKEN_TYPE_SUB_ASSIGN:
		assignment->assign_operator = IR_ASSIGN_OPERATOR_SUB;
		break;
	case TOKEN_TYPE_MUL_ASSIGN:
		assignment->assign_operator = IR_ASSIGN_OPERATOR_MUL;
		break;
	case TOKEN_TYPE_DIV_ASSIGN:
		assignment->assign_operator = IR_ASSIGN_OPERATOR_DIV;
		break;
	case TOKEN_TYPE_MOD_ASSIGN:
		assignment->assign_operator = IR_ASSIGN_OPERATOR_MOD;
		break;
	case TOKEN_TYPE_INCREMENT:
		assignment->assign_operator = IR_ASSIGN_OPERATOR_INCREMENT;
		break;
	case TOKEN_TYPE_DECREMENT:
		assignment->assign_operator = IR_ASSIGN_OPERATOR_DECREMENT;
		break;
	default:
		g_assert(!"Couldn't extract assign operator from token type");
		assignment->assign_operator = -1;
	}

	if (assignment->assign_operator != IR_ASSIGN_OPERATOR_INCREMENT &&
	    assignment->assign_operator != IR_ASSIGN_OPERATOR_DECREMENT)
		assignment->expression = ir_expression_new(nodes);
	else
		assignment->expression = NULL;

	return assignment;
}

struct ir_assignment *
ir_assignment_new_from_identifier(char *identifier,
				  struct ir_expression *expression)
{
	struct ir_assignment *assignment = g_new(struct ir_assignment, 1);
	assignment->token = expression->token;
	assignment->location = g_new(struct ir_location, 1);
	assignment->location->identifier = identifier;
	assignment->location->index = NULL;
	assignment->assign_operator = IR_ASSIGN_OPERATOR_SET;
	assignment->expression = expression;
	return assignment;
}

void ir_assignment_free(struct ir_assignment *assignment)
{
	ir_location_free(assignment->location);
	if (assignment->expression != NULL)
		ir_expression_free(assignment->expression);
	g_free(assignment);
}

struct ir_method_call *ir_method_call_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_METHOD_CALL);

	struct ir_method_call *call = g_new(struct ir_method_call, 1);
	call->token = last_node(nodes)->token;
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
	argument->token = last_node(nodes)->token;

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

struct ir_if_statement *ir_if_statement_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_IF_STATEMENT);

	struct ir_if_statement *statement = g_new(struct ir_if_statement, 1);
	statement->token = last_node(nodes)->token;
	statement->condition = ir_expression_new(nodes);
	statement->if_block = ir_block_new(nodes);

	if (peek_node(nodes)->type == AST_NODE_TYPE_BLOCK)
		statement->else_block = ir_block_new(nodes);
	else
		statement->else_block = NULL;

	return statement;
}

void ir_if_statement_free(struct ir_if_statement *statement)
{
	ir_expression_free(statement->condition);
	ir_block_free(statement->if_block);
	if (statement->else_block != NULL)
		ir_block_free(statement->else_block);
	g_free(statement);
}

struct ir_for_statement *ir_for_statement_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_FOR_STATEMENT);

	struct ir_for_statement *statement = g_new(struct ir_for_statement, 1);
	statement->token = last_node(nodes)->token;

	char *identifier = ir_identifier_from_ast(nodes);
	struct ir_expression *initializer = ir_expression_new(nodes);
	statement->initial =
		ir_assignment_new_from_identifier(identifier, initializer);
	statement->condition = ir_expression_new(nodes);
	statement->update = ir_for_update_new(nodes);
	statement->block = ir_block_new(nodes);

	return statement;
}

void ir_for_statement_free(struct ir_for_statement *statement)
{
	ir_block_free(statement->block);
	ir_for_update_free(statement->update);
	ir_expression_free(statement->condition);
	ir_assignment_free(statement->initial);
	g_free(statement);
}

struct ir_for_update *ir_for_update_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_FOR_UPDATE);

	struct ir_for_update *update = g_new(struct ir_for_update, 1);
	update->token = last_node(nodes)->token;

	if (peek_node(nodes)->type == AST_NODE_TYPE_METHOD_CALL) {
		update->type = IR_FOR_UPDATE_TYPE_METHOD_CALL;
		update->method_call = ir_method_call_new(nodes);
	} else if (peek_node(nodes)->type == AST_NODE_TYPE_ASSIGNMENT) {
		update->type = IR_FOR_UPDATE_TYPE_ASSIGNMENT;
		update->assignment = ir_assignment_new(nodes);
	} else {
		update->type = -1;
		g_assert(!"Invalid for update type");
	}

	return update;
}

void ir_for_update_free(struct ir_for_update *update)
{
	switch (update->type) {
	case IR_FOR_UPDATE_TYPE_METHOD_CALL:
		ir_method_call_free(update->method_call);
		break;
	case IR_FOR_UPDATE_TYPE_ASSIGNMENT:
		ir_assignment_free(update->assignment);
		break;
	default:
		break;
	}
	g_free(update);
}

struct ir_while_statement *ir_while_statement_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_WHILE_STATEMENT);

	struct ir_while_statement *statement =
		g_new(struct ir_while_statement, 1);
	statement->token = last_node(nodes)->token;
	statement->condition = ir_expression_new(nodes);
	statement->block = ir_block_new(nodes);
	return statement;
}

void ir_while_statement_free(struct ir_while_statement *statement)
{
	ir_block_free(statement->block);
	ir_expression_free(statement->condition);
	g_free(statement);
}

struct ir_location *ir_location_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_LOCATION);

	struct ir_location *location = g_new(struct ir_location, 1);
	location->token = last_node(nodes)->token;
	location->identifier = ir_identifier_from_ast(nodes);
	location->index = NULL;

	if (peek_node(nodes)->type == AST_NODE_TYPE_LOCATION_INDEX) {
		next_node(nodes);
		location->index = ir_expression_new(nodes);
	}

	return location;
}

void ir_location_free(struct ir_location *location)
{
	g_free(location->identifier);
	if (location->index != NULL)
		ir_expression_free(location->index);
	g_free(location);
}

struct ir_expression *ir_expression_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_EXPRESSION);

	struct ir_expression *expression = g_new(struct ir_expression, 1);
	expression->token = last_node(nodes)->token;

	switch (peek_node(nodes)->type) {
	case AST_NODE_TYPE_BINARY_EXPRESSION:
		expression->type = IR_EXPRESSION_TYPE_BINARY;
		expression->binary_expression = ir_binary_expression_new(nodes);
		break;
	case AST_NODE_TYPE_NOT_EXPRESSION:
		next_node(nodes);
		expression->type = IR_EXPRESSION_TYPE_NOT;
		expression->not_expression = ir_expression_new(nodes);
		break;
	case AST_NODE_TYPE_NEGATE_EXPRESSION:
		next_node(nodes);
		expression->type = IR_EXPRESSION_TYPE_NEGATE;
		expression->negate_expression = ir_expression_new(nodes);
		break;
	case AST_NODE_TYPE_LEN_EXPRESSION:
		expression->type = IR_EXPRESSION_TYPE_LEN;
		expression->length_expression = ir_length_expression_new(nodes);
		break;
	case AST_NODE_TYPE_METHOD_CALL:
		expression->type = IR_EXPRESSION_TYPE_METHOD_CALL;
		expression->method_call = ir_method_call_new(nodes);
		break;
	case AST_NODE_TYPE_LITERAL:
		expression->type = IR_EXPRESSION_TYPE_LITERAL;
		expression->literal = ir_literal_new(nodes);
		break;
	case AST_NODE_TYPE_LOCATION:
		expression->type = IR_EXPRESSION_TYPE_LOCATION;
		expression->location = ir_location_new(nodes);
		break;
	default:
		g_free(expression);
		g_assert(!"Couldn't extract sub expression from ast node");
		return NULL;
	}

	return expression;
}

void ir_expression_free(struct ir_expression *expression)
{
	switch (expression->type) {
	case IR_EXPRESSION_TYPE_BINARY:
		ir_binary_expression_free(expression->binary_expression);
		break;
	case IR_EXPRESSION_TYPE_NOT:
		ir_expression_free(expression->not_expression);
		break;
	case IR_EXPRESSION_TYPE_NEGATE:
		ir_expression_free(expression->negate_expression);
		break;
	case IR_EXPRESSION_TYPE_LEN:
		ir_length_expression_free(expression->length_expression);
		break;
	case IR_EXPRESSION_TYPE_METHOD_CALL:
		ir_method_call_free(expression->method_call);
		break;
	case IR_EXPRESSION_TYPE_LITERAL:
		ir_literal_free(expression->literal);
		break;
	case IR_EXPRESSION_TYPE_LOCATION:
		ir_location_free(expression->location);
		break;
	default:
		g_assert(!"Invalid expression type");
		break;
	}
	g_free(expression);
}

struct ir_binary_expression *ir_binary_expression_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_BINARY_EXPRESSION);

	struct ir_binary_expression *expression =
		g_new(struct ir_binary_expression, 1);
	expression->token = last_node(nodes)->token;
	expression->left = ir_expression_new(nodes);

	switch (next_node(nodes)->token.type) {
	case TOKEN_TYPE_OR:
		expression->binary_operator = IR_BINARY_OPERATOR_OR;
		break;
	case TOKEN_TYPE_AND:
		expression->binary_operator = IR_BINARY_OPERATOR_AND;
		break;
	case TOKEN_TYPE_EQUAL:
		expression->binary_operator = IR_BINARY_OPERATOR_EQUAL;
		break;
	case TOKEN_TYPE_NOT_EQUAL:
		expression->binary_operator = IR_BINARY_OPERATOR_NOT_EQUAL;
		break;
	case TOKEN_TYPE_LESS:
		expression->binary_operator = IR_BINARY_OPERATOR_LESS;
		break;
	case TOKEN_TYPE_LESS_EQUAL:
		expression->binary_operator = IR_BINARY_OPERATOR_LESS_EQUAL;
		break;
	case TOKEN_TYPE_GREATER_EQUAL:
		expression->binary_operator = IR_BINARY_OPERATOR_GREATER_EQUAL;
		break;
	case TOKEN_TYPE_GREATER:
		expression->binary_operator = IR_BINARY_OPERATOR_GREATER;
		break;
	case TOKEN_TYPE_ADD:
		expression->binary_operator = IR_BINARY_OPERATOR_ADD;
		break;
	case TOKEN_TYPE_SUB:
		expression->binary_operator = IR_BINARY_OPERATOR_SUB;
		break;
	case TOKEN_TYPE_MUL:
		expression->binary_operator = IR_BINARY_OPERATOR_MUL;
		break;
	case TOKEN_TYPE_DIV:
		expression->binary_operator = IR_BINARY_OPERATOR_DIV;
		break;
	case TOKEN_TYPE_MOD:
		expression->binary_operator = IR_BINARY_OPERATOR_MOD;
		break;
	default:
		g_assert(!"Invalid operator type in binary expression");
		break;
	}

	expression->right = ir_expression_new(nodes);

	return expression;
}

void ir_binary_expression_free(struct ir_binary_expression *expression)
{
	ir_expression_free(expression->left);
	ir_expression_free(expression->right);
	g_free(expression);
}

struct ir_length_expression *ir_length_expression_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_LEN_EXPRESSION);

	struct ir_length_expression *length_expression =
		g_new(struct ir_length_expression, 1);

	length_expression->identifier = ir_identifier_from_ast(nodes);
	length_expression->length = 1;

	return length_expression;
}

void ir_length_expression_free(struct ir_length_expression *length_expression)
{
	g_free(length_expression->identifier);
	g_free(length_expression);
}

struct ir_literal *ir_literal_new(struct ast_node **nodes)
{
	g_assert(next_node(nodes)->type == AST_NODE_TYPE_LITERAL);

	struct ast_node *node = peek_node(nodes);

	struct ir_literal *literal = g_new(struct ir_literal, 1);
	literal->token = last_node(nodes)->token;
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
		switch (node->token.type) {
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
		return character - 'a' + 10;
	else if (character >= 'A' && character <= 'F')
		return character - 'A' + 10;
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
		uint64_t increment = digit * place;

		if (increment / place != digit)
			return (uint64_t)-1;
		if (value + increment < value)
			return (uint64_t)-1;

		value += increment;
		if (value > max_value)
			return (uint64_t)-1;

		if (i != 0 && (place * base) / place != base)
			return (uint64_t)-1;

		place *= base;
	}

	return value;
}

uint64_t ir_int_literal_from_ast(struct ast_node **nodes, bool negate)
{
	struct ast_node *node = next_node(nodes);
	g_assert(node->type == AST_NODE_TYPE_INT_LITERAL);

	char *source_value = token_get_string(&node->token);

	uint64_t max_value = (uint64_t)INT64_MAX + (uint64_t)negate;
	bool hex = node->token.type == TOKEN_TYPE_HEX_LITERAL;

	uint64_t value = string_to_int(hex ? source_value + 2 : source_value,
				       max_value, hex ? 16 : 10);

	g_free(source_value);
	return value;
}

bool ir_bool_literal_from_ast(struct ast_node **nodes)
{
	struct ast_node *node = next_node(nodes);
	g_assert(node->type == AST_NODE_TYPE_BOOL_LITERAL);
	switch (node->token.type) {
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

	char *source_value = token_get_string(&node->token);

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

	return token_get_string(&node->token);
}

char *ir_identifier_from_ast(struct ast_node **nodes)
{
	struct ast_node *node = next_node(nodes);
	g_assert(node->type == AST_NODE_TYPE_IDENTIFIER);
	return token_get_string(&node->token);
}
