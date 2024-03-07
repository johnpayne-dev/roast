#include "parser/parser.h"

static const enum token_type BOOL_LITERALS[] = {
	TOKEN_TYPE_KEYWORD_TRUE,
	TOKEN_TYPE_KEYWORD_FALSE,
};

static const enum token_type INT_LITERALS[] = {
	TOKEN_TYPE_HEX_LITERAL,
	TOKEN_TYPE_DECIMAL_LITERAL,
};

static const enum token_type TYPES[] = {
	TOKEN_TYPE_KEYWORD_INT,
	TOKEN_TYPE_KEYWORD_BOOL,
};

static const enum token_type BINARY_OPERATORS[] = {
	TOKEN_TYPE_OR,
	TOKEN_TYPE_AND,
	TOKEN_TYPE_EQUAL,
	TOKEN_TYPE_NOT_EQUAL,
	TOKEN_TYPE_LESS,
	TOKEN_TYPE_LESS_EQUAL,
	TOKEN_TYPE_GREATER_EQUAL,
	TOKEN_TYPE_GREATER,
	TOKEN_TYPE_ADD,
	TOKEN_TYPE_SUB,
	TOKEN_TYPE_MUL,
	TOKEN_TYPE_DIV,
	TOKEN_TYPE_MOD,
};

static const enum token_type ASSIGN_OPERATORS[] = {
	TOKEN_TYPE_ASSIGN,     TOKEN_TYPE_ADD_ASSIGN, TOKEN_TYPE_SUB_ASSIGN,
	TOKEN_TYPE_MUL_ASSIGN, TOKEN_TYPE_DIV_ASSIGN, TOKEN_TYPE_MOD_ASSIGN,
};

static const enum token_type INCREMENT_OPERATORS[] = {
	TOKEN_TYPE_INCREMENT,
	TOKEN_TYPE_DECREMENT,
};

static const enum token_type KEYWORDS[] = {
	TOKEN_TYPE_KEYWORD_BOOL,   TOKEN_TYPE_KEYWORD_BREAK,
	TOKEN_TYPE_KEYWORD_CONST,  TOKEN_TYPE_KEYWORD_CONTINUE,
	TOKEN_TYPE_KEYWORD_ELSE,   TOKEN_TYPE_KEYWORD_FALSE,
	TOKEN_TYPE_KEYWORD_FOR,	   TOKEN_TYPE_KEYWORD_IF,
	TOKEN_TYPE_KEYWORD_IMPORT, TOKEN_TYPE_KEYWORD_INT,
	TOKEN_TYPE_KEYWORD_LEN,	   TOKEN_TYPE_KEYWORD_RETURN,
	TOKEN_TYPE_KEYWORD_TRUE,   TOKEN_TYPE_KEYWORD_VOID,
	TOKEN_TYPE_KEYWORD_WHILE,
};

static const uint32_t BINARY_OPERATOR_PRECEDENCE[] = {
	[TOKEN_TYPE_OR] = 0,
	[TOKEN_TYPE_AND] = 1,
	[TOKEN_TYPE_EQUAL] = 2,
	[TOKEN_TYPE_NOT_EQUAL] = 2,
	[TOKEN_TYPE_LESS] = 3,
	[TOKEN_TYPE_LESS_EQUAL] = 3,
	[TOKEN_TYPE_GREATER_EQUAL] = 3,
	[TOKEN_TYPE_GREATER] = 3,
	[TOKEN_TYPE_ADD] = 4,
	[TOKEN_TYPE_SUB] = 4,
	[TOKEN_TYPE_MUL] = 5,
	[TOKEN_TYPE_DIV] = 5,
	[TOKEN_TYPE_MOD] = 5,
};

static bool next_token(struct parser *parser, enum token_type token_type,
		       struct token *token)
{
	if (parser->position >= parser->token_count)
		return false;

	if (parser->tokens[parser->position].type != token_type)
		return false;

	if (token != NULL)
		*token = parser->tokens[parser->position];

	parser->position++;
	return true;
}

static bool peek_token(struct parser *parser, uint32_t relative_position,
		       enum token_type token_type)
{
	uint32_t position = parser->position + relative_position;

	if (position >= parser->token_count)
		return false;

	if (parser->tokens[position].type != token_type)
		return false;

	return true;
}

static bool next_token_in_table(struct parser *parser, const uint32_t table[],
				uint32_t table_length, struct token *token)
{
	for (uint32_t i = 0; i < table_length; i++) {
		if (next_token(parser, table[i], token))
			return true;
	}

	return false;
}

static bool peek_token_in_table(struct parser *parser,
				uint32_t relative_position,
				const uint32_t table[], uint32_t table_length)
{
	for (uint32_t i = 0; i < table_length; i++) {
		if (peek_token(parser, relative_position, table[i]))
			return true;
	}

	return false;
}

static void parse_error(struct parser *parser, const char *message)
{
	uint32_t token_index = parser->position == 0 ? 0 : parser->position - 1;
	struct token *token = &parser->tokens[token_index];

	uint32_t line_number = token_get_line_number(token);
	uint32_t column_number = token_get_column_number(token);

	parser->parse_error = true;
	g_printerr("ERROR at %i:%i: %s\n", line_number, column_number, message);
}

static struct ast_node *parse_identifier(struct parser *parser)
{
	struct token token;
	if (next_token_in_table(parser, KEYWORDS, G_N_ELEMENTS(KEYWORDS),
				&token))
		parse_error(parser, "Keyword cannot be used as identifier");
	else if (!next_token(parser, TOKEN_TYPE_IDENTIFIER, &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_IDENTIFIER, token);
}

static struct ast_node *parse_char_literal(struct parser *parser)
{
	struct token token;
	if (!next_token(parser, TOKEN_TYPE_CHAR_LITERAL, &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_CHAR_LITERAL, token);
}

static struct ast_node *parse_string_literal(struct parser *parser)
{
	struct token token;
	if (!next_token(parser, TOKEN_TYPE_STRING_LITERAL, &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_STRING_LITERAL, token);
}

static struct ast_node *parse_bool_literal(struct parser *parser)
{
	struct token token;
	if (!next_token_in_table(parser, BOOL_LITERALS,
				 G_N_ELEMENTS(BOOL_LITERALS), &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_BOOL_LITERAL, token);
}

static struct ast_node *parse_int_literal(struct parser *parser)
{
	struct token token;
	if (!next_token_in_table(parser, INT_LITERALS,
				 G_N_ELEMENTS(INT_LITERALS), &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_INT_LITERAL, token);
}

static struct ast_node *parse_type(struct parser *parser)
{
	struct token token;
	if (next_token(parser, TOKEN_TYPE_KEYWORD_VOID, &token))
		parse_error(parser, "void type not permitted here");
	else if (!next_token_in_table(parser, TYPES, G_N_ELEMENTS(TYPES),
				      &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_DATA_TYPE, token);
}

static struct ast_node *parse_binary_operator(struct parser *parser)
{
	struct token token;
	if (!next_token_in_table(parser, BINARY_OPERATORS,
				 G_N_ELEMENTS(BINARY_OPERATORS), &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_BINARY_OPERATOR, token);
}

static struct ast_node *parse_increment_operator(struct parser *parser)
{
	struct token token;
	if (!next_token_in_table(parser, INCREMENT_OPERATORS,
				 G_N_ELEMENTS(INCREMENT_OPERATORS), &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_INCREMENT_OPERATOR, token);
}

static struct ast_node *parse_assign_operator(struct parser *parser)
{
	struct token token;
	if (!next_token_in_table(parser, ASSIGN_OPERATORS,
				 G_N_ELEMENTS(ASSIGN_OPERATORS), &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_ASSIGN_OPERATOR, token);
}

static struct ast_node *parse_literal_negation(struct parser *parser)
{
	struct token token;
	if (!next_token(parser, TOKEN_TYPE_SUB, &token))
		return NULL;

	return ast_node_new_terminal(AST_NODE_TYPE_LITERAL_NEGATION, token);
}

static struct ast_node *parse_literal(struct parser *parser)
{
	struct ast_node *literal = ast_node_new(AST_NODE_TYPE_LITERAL);

	struct ast_node *negation = parse_literal_negation(parser);
	if (negation != NULL)
		ast_node_add_child(literal, negation);

	struct ast_node *child = NULL;
	if ((child = parse_bool_literal(parser))) {
	} else if ((child = parse_int_literal(parser))) {
	} else if ((child = parse_char_literal(parser))) {
	} else if ((child = parse_string_literal(parser))) {
		parse_error(parser, "String literal not permitted here");
	} else if (negation != NULL) {
		parse_error(parser, "Expected literal after negation");
	} else {
		ast_node_free(literal);
		return NULL;
	}

	ast_node_add_child(literal, child);
	return literal;
}

static struct ast_node *parse_array_literal(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_OPEN_CURLY_BRACKET, NULL))
		return NULL;

	struct ast_node *array = ast_node_new(AST_NODE_TYPE_ARRAY_LITERAL);

	while (true) {
		struct ast_node *literal = parse_literal(parser);
		if (literal == NULL)
			parse_error(parser,
				    "Expected literal in array literal");

		ast_node_add_child(array, literal);

		if (next_token(parser, TOKEN_TYPE_COMMA, NULL))
			continue;

		if (next_token(parser, TOKEN_TYPE_CLOSE_CURLY_BRACKET, NULL))
			break;

		parse_error(parser,
			    "Expected closing brace or comma in array literal");
		break;
	}

	return array;
}

static struct ast_node *parse_initializer(struct parser *parser)
{
	struct ast_node *initializer = ast_node_new(AST_NODE_TYPE_INITIALIZER);

	struct ast_node *child;
	if ((child = parse_array_literal(parser))) {
	} else if ((child = parse_literal(parser))) {
	} else {
		ast_node_free(initializer);
		return NULL;
	}

	ast_node_add_child(initializer, child);
	return initializer;
}

static struct ast_node *parse_expression(struct parser *parser);

static struct ast_node *parse_method_call_argument(struct parser *parser)
{
	struct ast_node *argument =
		ast_node_new(AST_NODE_TYPE_METHOD_CALL_ARGUMENT);

	struct ast_node *child;
	if ((child = parse_string_literal(parser))) {
	} else if ((child = parse_expression(parser))) {
	} else {
		ast_node_free(argument);
		return NULL;
	}

	ast_node_add_child(argument, child);
	return argument;
}

static struct ast_node *parse_method_call(struct parser *parser)
{
	if (!peek_token(parser, 0, TOKEN_TYPE_IDENTIFIER) ||
	    !peek_token(parser, 1, TOKEN_TYPE_OPEN_PARENTHESIS))
		return NULL;

	struct ast_node *call = ast_node_new(AST_NODE_TYPE_METHOD_CALL);

	struct ast_node *identifier = parse_identifier(parser);
	ast_node_add_child(call, identifier);

	next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL);

	if (next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		return call;

	while (true) {
		struct ast_node *argument = parse_method_call_argument(parser);
		if (argument == NULL)
			parse_error(parser, "Expected argument in method call");
		else
			ast_node_add_child(call, argument);

		if (next_token(parser, TOKEN_TYPE_COMMA, NULL))
			continue;
		if (next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
			break;

		parse_error(
			parser,
			"Expected comma or closing parenthesis in method call");
		break;
	}

	return call;
}

static bool is_binary_operator(struct parser *parser, uint32_t i)
{
	if (peek_token(parser, i, TOKEN_TYPE_SUB)) {
		if (i == 0)
			return false;

		if (peek_token_in_table(parser, i - 1, BINARY_OPERATORS,
					G_N_ELEMENTS(BINARY_OPERATORS)))
			return false;

		if (peek_token(parser, i - 1, TOKEN_TYPE_NOT))
			return false;
	}

	return peek_token_in_table(parser, i, BINARY_OPERATORS,
				   G_N_ELEMENTS(BINARY_OPERATORS));
}

static bool is_expression_termination(struct parser *parser, uint32_t depth,
				      uint32_t i)
{
	return depth == 0 &&
	       (peek_token(parser, i, TOKEN_TYPE_CLOSE_PARENTHESIS) ||
		peek_token(parser, i, TOKEN_TYPE_CLOSE_SQUARE_BRACKET) ||
		peek_token(parser, i, TOKEN_TYPE_SEMICOLON) ||
		peek_token(parser, i, TOKEN_TYPE_COMMA));
}

static GArray *find_binary_operator_indices(struct parser *parser,
					    uint32_t length)
{
	GArray *indices = g_array_new(false, false, sizeof(uint32_t));

	uint32_t depth = 0;
	uint32_t i = 0;

	while (i < length && !is_expression_termination(parser, depth, i)) {
		if (peek_token(parser, i, TOKEN_TYPE_OPEN_PARENTHESIS) ||
		    peek_token(parser, i, TOKEN_TYPE_OPEN_SQUARE_BRACKET))
			depth++;

		if (peek_token(parser, i, TOKEN_TYPE_CLOSE_PARENTHESIS) ||
		    peek_token(parser, i, TOKEN_TYPE_CLOSE_SQUARE_BRACKET))
			depth--;

		if (depth == 0 && is_binary_operator(parser, i))
			g_array_append_val(indices, i);

		i++;
	}

	return indices;
}

static uint32_t get_lowest_precedence_index(struct parser *parser,
					    GArray *operator_indices)
{
	uint32_t lowest_precedence_index = 0;
	uint32_t lowest_precedence = -1;

	for (uint32_t i = 0; i < operator_indices->len; i++) {
		uint32_t index = g_array_index(operator_indices, uint32_t, i);
		enum token_type type =
			parser->tokens[parser->position + index].type;

		uint32_t precedence = BINARY_OPERATOR_PRECEDENCE[type];
		if (precedence <= lowest_precedence) {
			lowest_precedence = precedence;
			lowest_precedence_index = index;
		}
	}

	return lowest_precedence_index;
}

static struct ast_node *parse_unary_expression(struct parser *parser);

static struct ast_node *parse_expression_with_length(struct parser *parser,
						     uint32_t length);

static struct ast_node *parse_binary_expression(struct parser *parser,
						uint32_t length)
{
	GArray *indices = find_binary_operator_indices(parser, length);
	if (indices->len == 0) {
		g_array_free(indices, true);
		return NULL;
	}

	struct ast_node *expression =
		ast_node_new(AST_NODE_TYPE_BINARY_EXPRESSION);

	uint32_t operator_index = get_lowest_precedence_index(parser, indices);
	uint32_t expected_position = parser->position + operator_index;

	struct ast_node *left =
		parse_expression_with_length(parser, operator_index);
	if (left == NULL)
		parse_error(parser, "Expected expression before operator");
	ast_node_add_child(expression, left);

	if (parser->position != expected_position) {
		parse_error(parser, "Expected binary operator in expression");
		parser->position = expected_position;
	}
	struct ast_node *binary_operator = parse_binary_operator(parser);
	ast_node_add_child(expression, binary_operator);

	if (length != (uint32_t)-1)
		length -= operator_index + 1;

	struct ast_node *right = parse_expression_with_length(parser, length);
	if (right == NULL)
		parse_error(parser, "Expected expression after operator");
	ast_node_add_child(expression, right);

	g_array_free(indices, true);
	return expression;
}

static struct ast_node *parse_parenthesis_expression(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		return NULL;

	struct ast_node *expression = parse_expression(parser);
	if (expression == NULL)
		parse_error(parser, "Expected expresion in parenthesis");

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected closing parenthesis in expression");

	return expression;
}

static struct ast_node *parse_not_expression(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_NOT, NULL))
		return NULL;

	struct ast_node *not = ast_node_new(AST_NODE_TYPE_NOT_EXPRESSION);

	struct ast_node *expression = parse_unary_expression(parser);
	if (expression == NULL)
		parse_error(parser, "Expected expression after not operator");
	ast_node_add_child(not, expression);

	return not ;
}

static struct ast_node *parse_negate_expression(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_SUB, NULL))
		return NULL;

	struct ast_node *negate = ast_node_new(AST_NODE_TYPE_NEGATE_EXPRESSION);

	struct ast_node *expression = parse_unary_expression(parser);
	if (expression == NULL)
		parse_error(parser,
			    "Expected expression after negate operator");
	ast_node_add_child(negate, expression);

	return negate;
}

static struct ast_node *parse_len_expression(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_LEN, NULL))
		return false;

	struct ast_node *len = ast_node_new(AST_NODE_TYPE_LEN_EXPRESSION);

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected open parenthesis in len expression");

	struct ast_node *identifier = parse_identifier(parser);
	if (identifier == NULL)
		parse_error(parser, "Expected identifier in len expression");
	ast_node_add_child(len, identifier);

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected closing parenthesis in len expression");

	return len;
}

static struct ast_node *parse_location(struct parser *parser);

static struct ast_node *parse_unary_expression(struct parser *parser)
{
	struct ast_node *expression = ast_node_new(AST_NODE_TYPE_EXPRESSION);

	struct ast_node *child;
	if ((child = parse_len_expression(parser))) {
	} else if ((child = parse_not_expression(parser))) {
	} else if ((child = parse_negate_expression(parser))) {
	} else if ((child = parse_literal(parser))) {
	} else if ((child = parse_method_call(parser))) {
	} else if ((child = parse_location(parser))) {
	} else if ((child = parse_parenthesis_expression(parser))) {
		ast_node_free(expression);
		return child;
	} else {
		ast_node_free(expression);
		return NULL;
	}

	ast_node_add_child(expression, child);
	return expression;
}

static struct ast_node *parse_expression_with_length(struct parser *parser,
						     uint32_t length)
{
	struct ast_node *expression = ast_node_new(AST_NODE_TYPE_EXPRESSION);

	struct ast_node *child;
	if ((child = parse_binary_expression(parser, length))) {
	} else if ((child = parse_unary_expression(parser))) {
		ast_node_free(expression);
		return child;
	} else {
		ast_node_free(expression);
		return NULL;
	}

	ast_node_add_child(expression, child);
	return expression;
}

static struct ast_node *parse_expression(struct parser *parser)
{
	return parse_expression_with_length(parser, (uint32_t)-1);
}

static struct ast_node *parse_assignment(struct parser *parser)
{
	struct ast_node *location = parse_location(parser);
	if (location == NULL)
		return NULL;

	struct ast_node *assignment = ast_node_new(AST_NODE_TYPE_ASSIGNMENT);
	ast_node_add_child(assignment, location);

	struct ast_node *assign_operator;
	if ((assign_operator = parse_assign_operator(parser))) {
		ast_node_add_child(assignment, assign_operator);

		struct ast_node *expression = parse_expression(parser);
		if (expression == NULL)
			parse_error(parser,
				    "Expected expression in assignment");
		ast_node_add_child(assignment, expression);
	} else if ((assign_operator = parse_increment_operator(parser))) {
		ast_node_add_child(assignment, assign_operator);
	} else {
		parse_error(parser, "Expected operator in assignment");
	}

	return assignment;
}

static struct ast_node *parse_location(struct parser *parser)
{
	struct ast_node *identifier = parse_identifier(parser);
	if (identifier == NULL)
		return NULL;

	struct ast_node *location = ast_node_new(AST_NODE_TYPE_LOCATION);
	ast_node_add_child(location, identifier);

	if (next_token(parser, TOKEN_TYPE_OPEN_SQUARE_BRACKET, NULL)) {
		struct ast_node *index =
			ast_node_new(AST_NODE_TYPE_LOCATION_INDEX);
		struct ast_node *expression = parse_expression(parser);
		if (expression == NULL)
			parse_error(parser,
				    "Expected expression in identifier index");
		ast_node_add_child(index, expression);
		ast_node_add_child(location, index);

		if (!next_token(parser, TOKEN_TYPE_CLOSE_SQUARE_BRACKET, NULL))
			parse_error(
				parser,
				"Expected closing square bracket in index expression");
	}

	return location;
}

static struct ast_node *parse_for_update(struct parser *parser)
{
	struct ast_node *update = ast_node_new(AST_NODE_TYPE_FOR_UPDATE);

	struct ast_node *child;
	if ((child = parse_method_call(parser))) {
	} else if ((child = parse_assignment(parser))) {
	} else {
		ast_node_free(update);
		return NULL;
	}

	ast_node_add_child(update, child);
	return update;
}

static struct ast_node *parse_continue_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_CONTINUE, NULL))
		return NULL;

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser, "Expected semicolon in continue statement");

	return ast_node_new(AST_NODE_TYPE_CONTINUE_STATEMENT);
}

static struct ast_node *parse_break_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_BREAK, NULL))
		return NULL;

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser, "Expected semicolon in break statement");

	return ast_node_new(AST_NODE_TYPE_BREAK_STATEMENT);
}

static struct ast_node *parse_return_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_RETURN, NULL))
		return NULL;

	struct ast_node *statement =
		ast_node_new(AST_NODE_TYPE_RETURN_STATEMENT);

	struct ast_node *expression = parse_expression(parser);
	if (expression == NULL)
		parse_error(parser, "Expected expression in return statement");
	ast_node_add_child(statement, expression);

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser, "Expected semicolon in return statement");

	return statement;
}

static struct ast_node *parse_block(struct parser *parser);

static struct ast_node *parse_while_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_WHILE, NULL))
		return NULL;

	struct ast_node *statement =
		ast_node_new(AST_NODE_TYPE_WHILE_STATEMENT);

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected open parenthesis in while statement");

	struct ast_node *expression = parse_expression(parser);
	if (expression == NULL)
		parse_error(parser, "Expected expression in while statement");
	ast_node_add_child(statement, expression);

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected closing parenthesis in while statement");

	struct ast_node *block = parse_block(parser);
	if (block == NULL)
		parse_error(parser, "Expected block in while statement");
	ast_node_add_child(statement, block);

	return statement;
}

static struct ast_node *parse_for_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_FOR, NULL))
		return NULL;

	struct ast_node *statement = ast_node_new(AST_NODE_TYPE_FOR_STATEMENT);

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected open parenthesis in for statement");

	struct ast_node *identifier = parse_identifier(parser);
	if (identifier == NULL)
		parse_error(parser,
			    "Expected identifier in for statement assignment");
	ast_node_add_child(statement, identifier);

	if (!next_token(parser, TOKEN_TYPE_ASSIGN, NULL))
		parse_error(parser, "Expected assignment in for statement");

	struct ast_node *initial_expression = parse_expression(parser);
	if (initial_expression == NULL)
		parse_error(parser,
			    "Expected expression in for statement assignment");
	ast_node_add_child(statement, initial_expression);

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser,
			    "Expected semicolon in for statement assignment");

	struct ast_node *break_expression = parse_expression(parser);
	if (break_expression == NULL)
		parse_error(parser,
			    "Expected expression in for statement condition");
	ast_node_add_child(statement, break_expression);

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser,
			    "Expected semicolon in for statement condition");

	struct ast_node *for_update = parse_for_update(parser);
	if (for_update == NULL)
		parse_error(parser,
			    "Expected update expression in for statement");
	ast_node_add_child(statement, for_update);

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected closing parenthesis in for statement");

	struct ast_node *block = parse_block(parser);
	if (block == NULL)
		parse_error(parser, "Expected block in for statement");
	ast_node_add_child(statement, block);

	return statement;
}

static struct ast_node *parse_if_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_IF, NULL))
		return NULL;

	struct ast_node *statement = ast_node_new(AST_NODE_TYPE_IF_STATEMENT);

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected open parenthesis in if statement");

	struct ast_node *expression = parse_expression(parser);
	if (expression == NULL)
		parse_error(parser, "Expected expression in if statement");
	ast_node_add_child(statement, expression);

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected close parenthesis in if statement");

	struct ast_node *if_block = parse_block(parser);
	if (if_block == NULL)
		parse_error(parser, "Expected block in if statement");
	ast_node_add_child(statement, if_block);

	if (next_token(parser, TOKEN_TYPE_KEYWORD_ELSE, NULL)) {
		struct ast_node *else_block = parse_block(parser);
		if (else_block == NULL)
			parse_error(parser, "Expected block in else statement");
		ast_node_add_child(statement, else_block);
	}

	return statement;
}

static struct ast_node *parse_method_call_statement(struct parser *parser)
{
	struct ast_node *call = parse_method_call(parser);
	if (call == NULL)
		return NULL;

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser,
			    "Expected semicolon in method call statement");

	return call;
}

static struct ast_node *parse_assign_statement(struct parser *parser)
{
	struct ast_node *assignment = parse_assignment(parser);
	if (assignment == NULL)
		return NULL;

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser,
			    "Expected semicolon in assignment statement");

	return assignment;
}

static struct ast_node *parse_statement(struct parser *parser)
{
	struct ast_node *statement = ast_node_new(AST_NODE_TYPE_STATEMENT);

	struct ast_node *child;
	if ((child = parse_if_statement(parser))) {
	} else if ((child = parse_for_statement(parser))) {
	} else if ((child = parse_while_statement(parser))) {
	} else if ((child = parse_return_statement(parser))) {
	} else if ((child = parse_break_statement(parser))) {
	} else if ((child = parse_continue_statement(parser))) {
	} else if ((child = parse_method_call_statement(parser))) {
	} else if ((child = parse_assign_statement(parser))) {
	} else {
		ast_node_free(statement);
		return NULL;
	}

	ast_node_add_child(statement, child);
	return statement;
}

static struct ast_node *parse_field(struct parser *parser);

static struct ast_node *parse_block(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_OPEN_CURLY_BRACKET, NULL))
		return NULL;

	struct ast_node *block = ast_node_new(AST_NODE_TYPE_BLOCK);

	struct ast_node *field;
	while ((field = parse_field(parser)))
		ast_node_add_child(block, field);

	struct ast_node *statement;
	while ((statement = parse_statement(parser)))
		ast_node_add_child(block, statement);

	if (!next_token(parser, TOKEN_TYPE_CLOSE_CURLY_BRACKET, NULL))
		parse_error(parser, "Expected closing curly bracket in block");

	return block;
}

static struct ast_node *parse_method_argument(struct parser *parser)
{
	struct ast_node *type = parse_type(parser);
	if (type == NULL)
		return NULL;

	struct ast_node *argument = ast_node_new(AST_NODE_TYPE_METHOD_ARGUMENT);
	ast_node_add_child(argument, type);

	struct ast_node *identifier = parse_identifier(parser);
	if (identifier == NULL)
		parse_error(parser, "Expected identifier in method argument");
	ast_node_add_child(argument, identifier);

	return argument;
}

static struct ast_node *parse_void(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_VOID, NULL))
		return NULL;

	return ast_node_new(AST_NODE_TYPE_VOID);
}

static struct ast_node *parse_method(struct parser *parser)
{
	struct ast_node *type = parse_void(parser);
	if (type == NULL) {
		type = parse_type(parser);
		if (type == NULL)
			return NULL;
	}

	struct ast_node *method = ast_node_new(AST_NODE_TYPE_METHOD);
	ast_node_add_child(method, type);

	struct ast_node *identifier = parse_identifier(parser);
	if (identifier == NULL)
		parse_error(parser,
			    "Expected identifier in method declaration");
	ast_node_add_child(method, identifier);

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected parenthesis in method declaraction");

	while (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL)) {
		struct ast_node *argument = parse_method_argument(parser);
		if (argument == NULL) {
			parse_error(parser,
				    "Expected argument in method declaration");
			break;
		}
		ast_node_add_child(method, argument);

		if (next_token(parser, TOKEN_TYPE_COMMA, NULL) &&
		    peek_token(parser, 0, TOKEN_TYPE_CLOSE_PARENTHESIS))
			parse_error(parser,
				    "Extra comma in method declaration");
	}

	struct ast_node *block = parse_block(parser);
	if (block == NULL)
		parse_error(parser, "Expected block after method declaration");
	ast_node_add_child(method, block);

	return method;
}

static struct ast_node *parse_field_identifier(struct parser *parser)
{
	struct ast_node *identifier = parse_identifier(parser);
	if (identifier == NULL)
		return NULL;

	struct ast_node *field_identifier =
		ast_node_new(AST_NODE_TYPE_FIELD_IDENTIFIER);
	ast_node_add_child(field_identifier, identifier);

	if (next_token(parser, TOKEN_TYPE_OPEN_SQUARE_BRACKET, NULL)) {
		struct ast_node *array_length = parse_int_literal(parser);
		if (array_length == NULL)
			array_length =
				ast_node_new(AST_NODE_TYPE_EMPTY_ARRAY_LENGTH);
		ast_node_add_child(field_identifier, array_length);

		if (!next_token(parser, TOKEN_TYPE_CLOSE_SQUARE_BRACKET, NULL))
			parse_error(
				parser,
				"Expected closing square bracket in field delcaration");
	}

	if (next_token(parser, TOKEN_TYPE_ASSIGN, NULL)) {
		struct ast_node *initializer = parse_initializer(parser);
		if (initializer == NULL)
			parse_error(
				parser,
				"Expected initializer in field declaration");
		ast_node_add_child(field_identifier, initializer);
	}

	return field_identifier;
}

static struct ast_node *parse_const(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_CONST, NULL))
		return NULL;

	return ast_node_new(AST_NODE_TYPE_CONST);
}

static struct ast_node *parse_field(struct parser *parser)
{
	struct ast_node *constant = parse_const(parser);

	struct ast_node *type = parse_type(parser);
	if (type == NULL) {
		if (constant)
			parse_error(parser,
				    "Expected type in field declaration");
		else
			return NULL;
	}

	struct ast_node *field = ast_node_new(AST_NODE_TYPE_FIELD);
	if (constant != NULL)
		ast_node_add_child(field, constant);
	ast_node_add_child(field, type);

	while (true) {
		struct ast_node *identifier = parse_field_identifier(parser);
		if (identifier == NULL)
			parse_error(parser,
				    "Expected identifier in field declaration");
		ast_node_add_child(field, identifier);

		if (next_token(parser, TOKEN_TYPE_COMMA, NULL))
			continue;

		if (next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
			break;

		parse_error(parser,
			    "Expected comma or semicolon in field declaration");
		break;
	}

	return field;
}

static bool peek_field(struct parser *parser)
{
	uint32_t i = peek_token(parser, 0, TOKEN_TYPE_KEYWORD_CONST);
	return (peek_token(parser, i, TOKEN_TYPE_KEYWORD_INT) ||
		peek_token(parser, i, TOKEN_TYPE_KEYWORD_BOOL)) &&
	       peek_token(parser, i + 1, TOKEN_TYPE_IDENTIFIER) &&
	       !peek_token(parser, i + 2, TOKEN_TYPE_OPEN_PARENTHESIS);
}

static struct ast_node *parse_import(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_IMPORT, NULL))
		return NULL;

	struct ast_node *import = ast_node_new(AST_NODE_TYPE_IMPORT);

	struct ast_node *identifier = parse_identifier(parser);
	if (identifier == NULL)
		parse_error(parser,
			    "Expected identifier in import declaration");
	ast_node_add_child(import, identifier);

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser, "Expected semicolon in import declaration");

	return import;
}

static struct ast_node *parse_program(struct parser *parser)
{
	struct ast_node *program = ast_node_new(AST_NODE_TYPE_PROGRAM);

	struct ast_node *import;
	while ((import = parse_import(parser)))
		ast_node_add_child(program, import);

	while (peek_field(parser)) {
		struct ast_node *field = parse_field(parser);
		ast_node_add_child(program, field);
	}

	struct ast_node *method;
	while ((method = parse_method(parser)))
		ast_node_add_child(program, method);

	if (parser->position != parser->token_count)
		parse_error(
			parser,
			"Unrecognized or unexpected declaration in program");

	return program;
}

struct parser *parser_new(void)
{
	struct parser *parser = g_new(struct parser, 1);
	return parser;
}

int parser_parse(struct parser *parser, GArray *token_list,
		 struct ast_node **ast)
{
	*parser = (struct parser){
		.tokens = &g_array_index(token_list, struct token, 0),
		.token_count = token_list->len,
		.position = 0,
		.parse_error = false,
	};
	struct ast_node *program = parse_program(parser);

	if (ast != NULL && !parser->parse_error)
		*ast = program;
	else
		ast_node_free(program);

	return parser->parse_error ? -1 : 0;
}

void parser_free(struct parser *parser)
{
	g_free(parser);
}
