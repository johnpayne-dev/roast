#include "parser/parser.h"

static const uint32_t BOOL_LITERALS[][2] = {
	{ TOKEN_TYPE_KEYWORD_TRUE, AST_BOOL_LITERAL_TYPE_TRUE },
	{ TOKEN_TYPE_KEYWORD_FALSE, AST_BOOL_LITERAL_TYPE_FALSE },
};

static const uint32_t INT_LITERALS[][2] = {
	{ TOKEN_TYPE_HEX_LITERAL, AST_INT_LITERAL_TYPE_HEX },
	{ TOKEN_TYPE_DECIMAL_LITERAL, AST_INT_LITERAL_TYPE_DECIMAL },
};

static const uint32_t TYPES[][2] = {
	{ TOKEN_TYPE_KEYWORD_INT, AST_TYPE_INT },
	{ TOKEN_TYPE_KEYWORD_BOOL, AST_TYPE_BOOL },
};

static const uint32_t BINARY_OPERATORS[][2] = {
	{ TOKEN_TYPE_OR, AST_BINARY_OPERATOR_TYPE_OR },
	{ TOKEN_TYPE_AND, AST_BINARY_OPERATOR_TYPE_AND },
	{ TOKEN_TYPE_EQUAL, AST_BINARY_OPERATOR_TYPE_EQUAL },
	{ TOKEN_TYPE_NOT_EQUAL, AST_BINARY_OPERATOR_TYPE_NOT_EQUAL },
	{ TOKEN_TYPE_LESS, AST_BINARY_OPERATOR_TYPE_LESS },
	{ TOKEN_TYPE_LESS_EQUAL, AST_BINARY_OPERATOR_TYPE_GREATER },
	{ TOKEN_TYPE_GREATER_EQUAL, AST_BINARY_OPERATOR_TYPE_GREATER_EQUAL },
	{ TOKEN_TYPE_GREATER, AST_BINARY_OPERATOR_TYPE_GREATER },
	{ TOKEN_TYPE_ADD, AST_BINARY_OPERATOR_TYPE_ADD },
	{ TOKEN_TYPE_SUB, AST_BINARY_OPERATOR_TYPE_SUB },
	{ TOKEN_TYPE_MUL, AST_BINARY_OPERATOR_TYPE_MUL },
	{ TOKEN_TYPE_DIV, AST_BINARY_OPERATOR_TYPE_DIV },
	{ TOKEN_TYPE_MOD, AST_BINARY_OPERATOR_TYPE_MOD },
};

static const uint32_t ASSIGN_OPERATORS[][2] = {
	{ TOKEN_TYPE_ASSIGN, AST_ASSIGN_OPERATOR_TYPE_ASSIGN },
	{ TOKEN_TYPE_ADD_ASSIGN, AST_ASSIGN_OPERATOR_TYPE_ADD },
	{ TOKEN_TYPE_SUB_ASSIGN, AST_ASSIGN_OPERATOR_TYPE_SUB },
	{ TOKEN_TYPE_MUL_ASSIGN, AST_ASSIGN_OPERATOR_TYPE_MUL },
	{ TOKEN_TYPE_DIV_ASSIGN, AST_ASSIGN_OPERATOR_TYPE_DIV },
	{ TOKEN_TYPE_MOD_ASSIGN, AST_ASSIGN_OPERATOR_TYPE_MOD },
};

static const uint32_t INCREMENT_OPERATORS[][2] = {
	{ TOKEN_TYPE_INCREMENT, AST_INCREMENT_OPERATOR_TYPE_ADD },
	{ TOKEN_TYPE_DECREMENT, AST_INCREMENT_OPERATOR_TYPE_SUB },
};

static const uint32_t KEYWORDS[][2] = {
	{ TOKEN_TYPE_KEYWORD_BOOL },   { TOKEN_TYPE_KEYWORD_BREAK },
	{ TOKEN_TYPE_KEYWORD_CONST },  { TOKEN_TYPE_KEYWORD_CONTINUE },
	{ TOKEN_TYPE_KEYWORD_ELSE },   { TOKEN_TYPE_KEYWORD_FALSE },
	{ TOKEN_TYPE_KEYWORD_FOR },    { TOKEN_TYPE_KEYWORD_IF },
	{ TOKEN_TYPE_KEYWORD_IMPORT }, { TOKEN_TYPE_KEYWORD_INT },
	{ TOKEN_TYPE_KEYWORD_LEN },    { TOKEN_TYPE_KEYWORD_RETURN },
	{ TOKEN_TYPE_KEYWORD_TRUE },   { TOKEN_TYPE_KEYWORD_VOID },
	{ TOKEN_TYPE_KEYWORD_WHILE },
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

static bool next_token_in_table(struct parser *parser,
				const uint32_t table[][2],
				uint32_t table_length, struct token *token,
				uint32_t *type)
{
	uint32_t table_index = -1;

	for (uint32_t i = 0; i < table_length; i++) {
		if (next_token(parser, table[i][0], token)) {
			table_index = i;
			break;
		}
	}

	if (table_index == (uint32_t)-1)
		return false;

	if (type != NULL)
		*type = table[table_index][1];

	return true;
}

static bool peek_token_in_table(struct parser *parser,
				uint32_t relative_position,
				const uint32_t table[][2],
				uint32_t table_length)
{
	for (uint32_t i = 0; i < table_length; i++) {
		if (peek_token(parser, relative_position, table[i][0]))
			return true;
	}

	return false;
}

static void parse_error(struct parser *parser, const char *message)
{
	uint32_t token_index = parser->position == 0 ? 0 : parser->position - 1;
	struct token *token = &parser->tokens[token_index];

	uint32_t line_number = token_get_line_number(token, parser->source);
	uint32_t column_number = token_get_column_number(token, parser->source);

	parser->parse_error = true;
	g_printerr("ERROR at %i:%i: %s\n", line_number, column_number, message);
}

static struct ast_identifier *parse_identifier(struct parser *parser)
{
	struct token token;
	if (next_token_in_table(parser, KEYWORDS, G_N_ELEMENTS(KEYWORDS),
				&token, NULL))
		parse_error(parser, "Keyword cannot be used as identifier");
	else if (!next_token(parser, TOKEN_TYPE_IDENTIFIER, &token))
		return NULL;

	struct ast_identifier *identifier = g_new0(struct ast_identifier, 1);
	identifier->token = token;
	return identifier;
}

static void free_identifier(struct ast_identifier *identifier)
{
	if (identifier == NULL)
		return;

	g_free(identifier);
}

static struct ast_char_literal *parse_char_literal(struct parser *parser)
{
	struct token token;
	if (!next_token(parser, TOKEN_TYPE_CHAR_LITERAL, &token))
		return NULL;

	struct ast_char_literal *literal = g_new0(struct ast_char_literal, 1);
	literal->token = token;
	return literal;
}

static void free_char_literal(struct ast_char_literal *literal)
{
	if (literal == NULL)
		return;

	g_free(literal);
}

static struct ast_string_literal *parse_string_literal(struct parser *parser)
{
	struct token token;
	if (!next_token(parser, TOKEN_TYPE_STRING_LITERAL, &token))
		return NULL;

	struct ast_string_literal *literal =
		g_new0(struct ast_string_literal, 1);
	literal->token = token;
	return literal;
}

static void free_string_literal(struct ast_string_literal *literal)
{
	if (literal == NULL)
		return;

	g_free(literal);
}

static struct ast_bool_literal *parse_bool_literal(struct parser *parser)
{
	struct token token;
	enum ast_bool_literal_type type;
	if (!next_token_in_table(parser, BOOL_LITERALS,
				 G_N_ELEMENTS(BOOL_LITERALS), &token, &type))
		return NULL;

	struct ast_bool_literal *literal = g_new0(struct ast_bool_literal, 1);
	literal->token = token;
	literal->type = type;
	return literal;
}

static void free_bool_literal(struct ast_bool_literal *literal)
{
	if (literal == NULL)
		return;

	g_free(literal);
}

static struct ast_int_literal *parse_int_literal(struct parser *parser)
{
	struct token token;
	enum ast_int_literal_type type;
	if (!next_token_in_table(parser, INT_LITERALS,
				 G_N_ELEMENTS(INT_LITERALS), &token, &type))
		return NULL;

	struct ast_int_literal *literal = g_new0(struct ast_int_literal, 1);
	literal->token = token;
	literal->type = type;
	return literal;
}

static void free_int_literal(struct ast_int_literal *literal)
{
	if (literal == NULL)
		return;

	g_free(literal);
}

static void free_literal(struct ast_literal *literal);

static struct ast_literal *parse_literal(struct parser *parser)
{
	struct ast_literal *literal = g_new0(struct ast_literal, 1);

	literal->negate = next_token(parser, TOKEN_TYPE_SUB, NULL);

	if ((literal->bool_literal = parse_bool_literal(parser))) {
		literal->type = AST_LITERAL_TYPE_BOOL;
	} else if ((literal->int_literal = parse_int_literal(parser))) {
		literal->type = AST_LITERAL_TYPE_INT;
	} else if ((literal->char_literal = parse_char_literal(parser))) {
		literal->type = AST_LITERAL_TYPE_CHAR;
	} else if (next_token(parser, TOKEN_TYPE_STRING_LITERAL, NULL)) {
		parse_error(parser, "String literal not permitted here");
	} else if (literal->negate) {
		parse_error(parser, "Expected literal after negation");
	} else {
		free_literal(literal);
		return NULL;
	}

	return literal;
}

static void free_literal(struct ast_literal *literal)
{
	if (literal == NULL)
		return;

	switch (literal->type) {
	case AST_LITERAL_TYPE_INT:
		free_int_literal(literal->int_literal);
		break;
	case AST_LITERAL_TYPE_BOOL:
		free_bool_literal(literal->bool_literal);
		break;
	case AST_LITERAL_TYPE_CHAR:
		free_char_literal(literal->char_literal);
		break;
	default:
		break;
	}

	g_free(literal);
}

static struct ast_array_literal *parse_array_literal(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_OPEN_CURLY_BRACKET, NULL))
		return NULL;

	struct ast_array_literal *array = g_new0(struct ast_array_literal, 1);
	array->literals =
		g_array_new(false, false, sizeof(struct ast_literal *));

	while (true) {
		struct ast_literal *literal = parse_literal(parser);
		if (literal == NULL)
			parse_error(parser,
				    "Expected literal in array literal");

		g_array_append_val(array->literals, literal);

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

static void free_array_literal(struct ast_array_literal *array)
{
	if (array == NULL)
		return;

	for (uint32_t i = 0; i < array->literals->len; i++)
		free_literal(g_array_index(array->literals,
					   struct ast_literal *, i));
	g_array_free(array->literals, true);

	g_free(array);
}

static void free_initializer(struct ast_initializer *initializer);

static struct ast_initializer *parse_initializer(struct parser *parser)
{
	struct ast_initializer *initializer = g_new0(struct ast_initializer, 1);

	if ((initializer->array_literal = parse_array_literal(parser))) {
		initializer->type = AST_INITIALIZER_TYPE_ARRAY_LITERAL;
	} else if ((initializer->literal = parse_literal(parser))) {
		initializer->type = AST_INITIALIZER_TYPE_LITERAL;
	} else {
		free_initializer(initializer);
		return NULL;
	}

	return initializer;
}

static void free_initializer(struct ast_initializer *initializer)
{
	if (initializer == NULL)
		return;

	switch (initializer->type) {
	case AST_INITIALIZER_TYPE_LITERAL:
		free_literal(initializer->literal);
		break;
	case AST_INITIALIZER_TYPE_ARRAY_LITERAL:
		free_array_literal(initializer->array_literal);
		break;
	default:
		break;
	}

	g_free(initializer);
}

static struct ast_type *parse_type(struct parser *parser)
{
	struct token token;
	enum ast_type_type type;

	if (next_token(parser, TOKEN_TYPE_KEYWORD_VOID, &token)) {
		parse_error(parser, "void type not permitted here");
		type = 0;
	} else if (!next_token_in_table(parser, TYPES, G_N_ELEMENTS(TYPES),
					&token, &type)) {
		return NULL;
	}

	struct ast_type *ast_type = g_new0(struct ast_type, 1);
	ast_type->type = type;
	ast_type->token = token;
	return ast_type;
}

static void free_type(struct ast_type *type)
{
	if (type == NULL)
		return;

	g_free(type);
}

static struct ast_binary_operator *parse_binary_operator(struct parser *parser)
{
	struct token token;
	enum ast_binary_operator_type type;
	if (!next_token_in_table(parser, BINARY_OPERATORS,
				 G_N_ELEMENTS(BINARY_OPERATORS), &token, &type))
		return NULL;

	struct ast_binary_operator *binary =
		g_new0(struct ast_binary_operator, 1);
	binary->type = type;
	binary->token = token;
	return binary;
}

static void free_binary_operator(struct ast_binary_operator *binary)
{
	if (binary == NULL)
		return;

	g_free(binary);
}

static struct ast_increment_operator *
parse_increment_operator(struct parser *parser)
{
	struct token token;
	enum ast_increment_operator_type type;
	if (!next_token_in_table(parser, INCREMENT_OPERATORS,
				 G_N_ELEMENTS(INCREMENT_OPERATORS), &token,
				 &type))
		return NULL;

	struct ast_increment_operator *increment =
		g_new0(struct ast_increment_operator, 1);
	increment->type = type;
	increment->token = token;
	return increment;
}

static void free_increment_operator(struct ast_increment_operator *increment)
{
	if (increment == NULL)
		return;

	g_free(increment);
}

static struct ast_assign_operator *parse_assign_operator(struct parser *parser)
{
	struct token token;
	enum ast_assign_operator_type type;
	if (!next_token_in_table(parser, ASSIGN_OPERATORS,
				 G_N_ELEMENTS(ASSIGN_OPERATORS), &token, &type))
		return NULL;

	struct ast_assign_operator *assign =
		g_new0(struct ast_assign_operator, 1);
	assign->type = type;
	assign->token = token;
	return assign;
}

static void free_assign_operator(struct ast_assign_operator *assign)
{
	if (assign == NULL)
		return;

	g_free(assign);
}

static struct ast_expression *parse_expression(struct parser *parser);

static void free_expression(struct ast_expression *expression);

static void
free_method_call_argument(struct ast_method_call_argument *argument);

static struct ast_method_call_argument *
parse_method_call_argument(struct parser *parser)
{
	struct ast_method_call_argument *argument =
		g_new0(struct ast_method_call_argument, 1);

	if ((argument->string_literal = parse_string_literal(parser))) {
		argument->type = AST_METHOD_CALL_ARGUMENT_TYPE_STRING;
	} else if ((argument->expression = parse_expression(parser))) {
		argument->type = AST_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION;
	} else {
		free_method_call_argument(argument);
		return NULL;
	}

	return argument;
}

static void free_method_call_argument(struct ast_method_call_argument *argument)
{
	if (argument == NULL)
		return;

	if (argument->type == AST_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION)
		free_expression(argument->expression);
	else
		free_string_literal(argument->string_literal);

	g_free(argument);
}

static struct ast_method_call *parse_method_call(struct parser *parser)
{
	if (!peek_token(parser, 0, TOKEN_TYPE_IDENTIFIER) ||
	    !peek_token(parser, 1, TOKEN_TYPE_OPEN_PARENTHESIS))
		return NULL;

	struct ast_method_call *call = g_new0(struct ast_method_call, 1);

	call->identifer = parse_identifier(parser);
	next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL);

	call->arguments = g_array_new(
		false, false, sizeof(struct ast_method_call_argument *));

	if (next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		return call;

	while (true) {
		struct ast_method_call_argument *argument =
			parse_method_call_argument(parser);
		if (argument == NULL)
			parse_error(parser, "Expected argument in method call");
		else
			g_array_append_val(call->arguments, argument);

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

static void free_method_call(struct ast_method_call *call)
{
	if (call == NULL)
		return;

	free_identifier(call->identifer);
	for (uint32_t i = 0; i < call->arguments->len; i++)
		free_method_call_argument(g_array_index(
			call->arguments, struct ast_method_call_argument *, i));
	g_array_free(call->arguments, true);

	g_free(call);
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

static struct ast_unary_expression *
parse_unary_expression(struct parser *parser);

static struct ast_expression *
parse_expression_with_length(struct parser *parser, uint32_t length);

static struct ast_binary_expression *
parse_binary_expression(struct parser *parser, uint32_t length)
{
	GArray *indices = find_binary_operator_indices(parser, length);
	if (indices->len == 0) {
		g_array_free(indices, true);
		return NULL;
	}

	struct ast_binary_expression *expression =
		g_new0(struct ast_binary_expression, 1);

	uint32_t operator_index = get_lowest_precedence_index(parser, indices);
	uint32_t expected_position = parser->position + operator_index;

	expression->left = parse_expression_with_length(parser, operator_index);
	if (expression->left == NULL)
		parse_error(parser, "Expected expression before operator");

	if (parser->position != expected_position) {
		parse_error(parser, "Expected binary operator in expression");
		parser->position = expected_position;
	}
	expression->binary_operator = parse_binary_operator(parser);

	if (length != (uint32_t)-1)
		length -= operator_index + 1;
	expression->right = parse_expression_with_length(parser, length);
	if (expression->right == NULL)
		parse_error(parser, "Expected expression after operator");

	g_array_free(indices, true);
	return expression;
}

static void free_binary_expression(struct ast_binary_expression *expression)
{
	if (expression == NULL)
		return;

	free_expression(expression->left);
	free_binary_operator(expression->binary_operator);
	free_expression(expression->right);

	g_free(expression);
}

static struct ast_expression *
parse_parenthesis_expression(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		return NULL;

	struct ast_expression *expression = parse_expression(parser);
	if (expression == NULL)
		parse_error(parser, "Expected expresion in parenthesis");

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected closing parenthesis in expression");

	return expression;
}

static struct ast_unary_expression *parse_not_expression(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_NOT, NULL))
		return NULL;

	struct ast_unary_expression *expression =
		parse_unary_expression(parser);
	if (expression == NULL)
		parse_error(parser, "Expected expression after not operator");

	return expression;
}

static struct ast_unary_expression *
parse_negate_expression(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_SUB, NULL))
		return NULL;

	struct ast_unary_expression *expression =
		parse_unary_expression(parser);
	if (expression == NULL)
		parse_error(parser,
			    "Expected expression after negate operator");

	return expression;
}

static struct ast_identifier *parse_len_expression(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_LEN, NULL))
		return false;

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected open parenthesis in len expression");

	struct ast_identifier *identifier = parse_identifier(parser);
	if (identifier == NULL)
		parse_error(parser, "Expected identifier in len expression");

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected closing parenthesis in len expression");

	return identifier;
}

static struct ast_location *parse_location(struct parser *parser);

static void free_location(struct ast_location *location);

static void
free_unary_expression(struct ast_unary_expression *unary_expression);

static struct ast_unary_expression *
parse_unary_expression(struct parser *parser)
{
	struct ast_unary_expression *expression =
		g_new0(struct ast_unary_expression, 1);

	if ((expression->literal = parse_literal(parser))) {
		expression->type = AST_UNARY_EXPRESSION_TYPE_LITERAL;
	} else if ((expression->len_identifier =
			    parse_len_expression(parser))) {
		expression->type = AST_UNARY_EXPRESSION_TYPE_LEN;
	} else if ((expression->method_call = parse_method_call(parser))) {
		expression->type = AST_UNARY_EXPRESSION_TYPE_METHOD_CALL;
	} else if ((expression->location = parse_location(parser))) {
		expression->type = AST_UNARY_EXPRESSION_TYPE_LOCATION;
	} else if ((expression->not_expression =
			    parse_not_expression(parser))) {
		expression->type = AST_UNARY_EXPRESSION_TYPE_NOT;
	} else if ((expression->negate_expression =
			    parse_negate_expression(parser))) {
		expression->type = AST_UNARY_EXPRESSION_TYPE_NEGATE;
	} else if ((expression->parenthesis_expression =
			    parse_parenthesis_expression(parser))) {
		expression->type = AST_UNARY_EXPRESSION_TYPE_PARENTHESIS;
	} else {
		free_unary_expression(expression);
		return NULL;
	}

	return expression;
}

static void free_unary_expression(struct ast_unary_expression *expression)
{
	if (expression == NULL)
		return;

	switch (expression->type) {
	case AST_UNARY_EXPRESSION_TYPE_METHOD_CALL:
		free_method_call(expression->method_call);
		break;
	case AST_UNARY_EXPRESSION_TYPE_LOCATION:
		free_location(expression->location);
		break;
	case AST_UNARY_EXPRESSION_TYPE_NOT:
		free_unary_expression(expression->not_expression);
		break;
	case AST_UNARY_EXPRESSION_TYPE_NEGATE:
		free_unary_expression(expression->negate_expression);
		break;
	case AST_UNARY_EXPRESSION_TYPE_LITERAL:
		free_literal(expression->literal);
		break;
	case AST_UNARY_EXPRESSION_TYPE_PARENTHESIS:
		free_expression(expression->parenthesis_expression);
		break;
	case AST_UNARY_EXPRESSION_TYPE_LEN:
		free_identifier(expression->len_identifier);
		break;
	default:
		break;
	}

	g_free(expression);
}

static struct ast_expression *
parse_expression_with_length(struct parser *parser, uint32_t length)
{
	struct ast_expression *expression = g_new0(struct ast_expression, 1);

	if ((expression->binary = parse_binary_expression(parser, length))) {
		expression->type = AST_EXPRESSION_TYPE_BINARY;
	} else if ((expression->unary = parse_unary_expression(parser))) {
		expression->type = AST_EXPRESSION_TYPE_UNARY;
	} else {
		free_expression(expression);
		return NULL;
	}

	return expression;
}

static struct ast_expression *parse_expression(struct parser *parser)
{
	return parse_expression_with_length(parser, (uint32_t)-1);
}

static void free_expression(struct ast_expression *expression)
{
	if (expression == NULL)
		return;

	switch (expression->type) {
	case AST_EXPRESSION_TYPE_BINARY:
		free_binary_expression(expression->binary);
		break;
	case AST_EXPRESSION_TYPE_UNARY:
		free_unary_expression(expression->unary);
		break;
	default:
		break;
	}

	g_free(expression);
}

static void
free_assign_expression(struct ast_assign_expression *assign_expression);

static struct ast_assign_expression *
parse_assign_expression(struct parser *parser)
{
	struct ast_assign_expression *assign =
		g_new0(struct ast_assign_expression, 1);

	if ((assign->assign_operator = parse_assign_operator(parser))) {
		assign->type = AST_ASSIGN_EXPRESSION_TYPE_ASSIGNMENT;
		assign->expression = parse_expression(parser);
		if (assign->expression == NULL)
			parse_error(parser,
				    "Expected expression in assignment");
	} else if ((assign->increment_operator =
			    parse_increment_operator(parser))) {
		assign->type = AST_ASSIGN_EXPRESSION_TYPE_INCREMENT;
	} else {
		free_assign_expression(assign);
		return NULL;
	}

	return assign;
}

static void free_assign_expression(struct ast_assign_expression *assign)
{
	if (assign == NULL)
		return;

	switch (assign->type) {
	case AST_ASSIGN_EXPRESSION_TYPE_ASSIGNMENT:
		free_assign_operator(assign->assign_operator);
		free_expression(assign->expression);
		break;
	case AST_ASSIGN_EXPRESSION_TYPE_INCREMENT:
		free_increment_operator(assign->increment_operator);
		break;
	default:
		break;
	}

	g_free(assign);
}

static struct ast_location *parse_location(struct parser *parser)
{
	struct ast_identifier *identifier = parse_identifier(parser);
	if (identifier == NULL)
		return NULL;

	struct ast_location *location = g_new0(struct ast_location, 1);
	location->identifier = identifier;

	if (next_token(parser, TOKEN_TYPE_OPEN_SQUARE_BRACKET, NULL)) {
		location->index_expression = parse_expression(parser);
		if (location->index_expression == NULL)
			parse_error(parser,
				    "Expected expression in identifier index");

		if (!next_token(parser, TOKEN_TYPE_CLOSE_SQUARE_BRACKET, NULL))
			parse_error(
				parser,
				"Expected closing square bracket in index expression");
	}

	return location;
}

static void free_location(struct ast_location *location)
{
	if (location == NULL)
		return;

	free_expression(location->index_expression);

	g_free(location);
}

static void free_for_update(struct ast_for_update *for_update);

static struct ast_for_update *parse_for_update(struct parser *parser)
{
	struct ast_for_update *update = g_new0(struct ast_for_update, 1);

	if ((update->method_call = parse_method_call(parser))) {
		update->type = AST_FOR_UPDATE_TYPE_METHOD_CALL;
	} else if ((update->assign_statement.location =
			    parse_location(parser))) {
		update->type = AST_FOR_UPDATE_TYPE_ASSIGN;

		update->assign_statement.assign_expression =
			parse_assign_expression(parser);
		if (update->assign_statement.assign_expression == NULL)
			parse_error(parser,
				    "Expected assignment in for update");
	} else {
		free_for_update(update);
		return NULL;
	}

	return update;
}

static void free_for_update(struct ast_for_update *for_update)
{
	if (for_update == NULL)
		return;

	switch (for_update->type) {
	case AST_FOR_UPDATE_TYPE_METHOD_CALL:
		free_method_call(for_update->method_call);
		break;
	case AST_FOR_UPDATE_TYPE_ASSIGN:
		free_location(for_update->assign_statement.location);
		free_assign_expression(
			for_update->assign_statement.assign_expression);
		break;
	default:
		break;
	}

	g_free(for_update);
}

static bool parse_continue_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_CONTINUE, NULL))
		return false;

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser, "Expected semicolon in continue statement");

	return true;
}

static bool parse_break_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_BREAK, NULL))
		return false;

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser, "Expected semicolon in break statement");

	return true;
}

static struct ast_expression *parse_return_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_RETURN, NULL))
		return NULL;

	struct ast_expression *expression = parse_expression(parser);
	if (expression == NULL)
		parse_error(parser, "Expected expression in return statement");

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser, "Expected semicolon in return statement");

	return expression;
}

static struct ast_block *parse_block(struct parser *parser);

static void free_block(struct ast_block *block);

static struct ast_while_statement *parse_while_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_WHILE, NULL))
		return NULL;

	struct ast_while_statement *statement =
		g_new0(struct ast_while_statement, 1);

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected open parenthesis in while statement");

	statement->expression = parse_expression(parser);
	if (statement->expression == NULL)
		parse_error(parser, "Expected expression in while statement");

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected closing parenthesis in while statement");

	statement->block = parse_block(parser);
	if (statement->block == NULL)
		parse_error(parser, "Expected block in while statement");

	return statement;
}

static void free_while_statement(struct ast_while_statement *statement)
{
	if (statement == NULL)
		return;

	free_expression(statement->expression);
	free_block(statement->block);

	g_free(statement);
}

static struct ast_for_statement *parse_for_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_FOR, NULL))
		return NULL;

	struct ast_for_statement *statement =
		g_new0(struct ast_for_statement, 1);

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected open parenthesis in for statement");

	statement->identifier = parse_identifier(parser);
	if (statement->identifier == NULL)
		parse_error(parser,
			    "Expected identifier in for statement assignment");

	if (!next_token(parser, TOKEN_TYPE_ASSIGN, NULL))
		parse_error(parser, "Expected assignment in for statement");

	statement->initial_expression = parse_expression(parser);
	if (statement->initial_expression == NULL)
		parse_error(parser,
			    "Expected expression in for statement assignment");

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser,
			    "Expected semicolon in for statement assignment");

	statement->break_expression = parse_expression(parser);
	if (statement->break_expression == NULL)
		parse_error(parser,
			    "Expected expression in for statement condition");

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser,
			    "Expected semicolon in for statement condition");

	statement->for_update = parse_for_update(parser);
	if (statement->for_update == NULL)
		parse_error(parser,
			    "Expected update expression in for statement");

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected closing parenthesis in for statement");

	statement->block = parse_block(parser);
	if (statement->block == NULL)
		parse_error(parser, "Expected block in for statement");

	return statement;
}

static void free_for_statement(struct ast_for_statement *statement)
{
	if (statement == NULL)
		return;

	free_identifier(statement->identifier);
	free_expression(statement->initial_expression);
	free_expression(statement->break_expression);
	free_for_update(statement->for_update);
	free_block(statement->block);

	g_free(statement);
}

static struct ast_if_statement *parse_if_statement(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_IF, NULL))
		return NULL;

	struct ast_if_statement *statement = g_new0(struct ast_if_statement, 1);

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected open parenthesis in if statement");

	statement->expression = parse_expression(parser);
	if (statement->expression == NULL)
		parse_error(parser, "Expected expression in if statement");

	if (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected close parenthesis in if statement");

	statement->if_block = parse_block(parser);
	if (statement->if_block == NULL)
		parse_error(parser, "Expected block in if statement");

	if (next_token(parser, TOKEN_TYPE_KEYWORD_ELSE, NULL)) {
		statement->else_block = parse_block(parser);
		if (statement->else_block == NULL)
			parse_error(parser, "Expected block in else statement");
	}

	return statement;
}

static void free_if_statement(struct ast_if_statement *statement)
{
	if (statement == NULL)
		return;

	free_expression(statement->expression);
	free_block(statement->if_block);
	free_block(statement->else_block);

	g_free(statement);
}

static struct ast_method_call *
parse_method_call_statement(struct parser *parser)
{
	struct ast_method_call *call = parse_method_call(parser);
	if (call == NULL)
		return NULL;

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser,
			    "Expected semicolon in method call statement");

	return call;
}

static struct ast_assign_statement *
parse_assign_statement(struct parser *parser)
{
	struct ast_location *location = parse_location(parser);
	if (location == NULL)
		return NULL;

	struct ast_assign_statement *statement =
		g_new0(struct ast_assign_statement, 1);
	statement->location = location;

	statement->assign_expression = parse_assign_expression(parser);
	if (statement->assign_expression == NULL)
		parse_error(parser, "Expected assignment in statement");

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser,
			    "Expected semicolon in assignment statement");

	return statement;
}

static void free_assign_statement(struct ast_assign_statement *statement)
{
	if (statement == NULL)
		return;

	free_location(statement->location);
	free_assign_expression(statement->assign_expression);

	g_free(statement);
}

static void free_statement(struct ast_statement *statement);

static struct ast_statement *parse_statement(struct parser *parser)
{
	struct ast_statement *statement = g_new0(struct ast_statement, 1);

	if ((statement->if_statement = parse_if_statement(parser))) {
		statement->type = AST_STATEMENT_TYPE_IF;
	} else if ((statement->for_statement = parse_for_statement(parser))) {
		statement->type = AST_STATEMENT_TYPE_FOR;
	} else if ((statement->while_statement =
			    parse_while_statement(parser))) {
		statement->type = AST_STATEMENT_TYPE_WHILE;
	} else if ((statement->return_expression =
			    parse_return_statement(parser))) {
		statement->type = AST_STATEMENT_TYPE_RETURN;
	} else if (parse_break_statement(parser)) {
		statement->type = AST_STATEMENT_TYPE_BREAK;
	} else if (parse_continue_statement(parser)) {
		statement->type = AST_STATEMENT_TYPE_CONTINUE;
	} else if ((statement->method_call =
			    parse_method_call_statement(parser))) {
		statement->type = AST_STATEMENT_TYPE_METHOD_CALL;
	} else if ((statement->assign_statement =
			    parse_assign_statement(parser))) {
		statement->type = AST_STATEMENT_TYPE_ASSIGN;
	} else {
		free_statement(statement);
		return NULL;
	}

	return statement;
}

static void free_statement(struct ast_statement *statement)
{
	if (statement == NULL)
		return;

	switch (statement->type) {
	case AST_STATEMENT_TYPE_METHOD_CALL:
		free_method_call(statement->method_call);
		break;
	case AST_STATEMENT_TYPE_ASSIGN:
		free_assign_statement(statement->assign_statement);
		break;
	case AST_STATEMENT_TYPE_IF:
		free_if_statement(statement->if_statement);
		break;
	case AST_STATEMENT_TYPE_FOR:
		free_for_statement(statement->for_statement);
		break;
	default:
		break;
	}

	g_free(statement);
}

static struct ast_field *parse_field(struct parser *parser);
static void free_field(struct ast_field *field);

static struct ast_block *parse_block(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_OPEN_CURLY_BRACKET, NULL))
		return NULL;

	struct ast_block *block = g_new0(struct ast_block, 1);
	block->fields = g_array_new(false, false, sizeof(struct ast_field *));
	block->statements =
		g_array_new(false, false, sizeof(struct ast_statement *));

	struct ast_field *field;
	while ((field = parse_field(parser)))
		g_array_append_val(block->fields, field);

	struct ast_statement *statement;
	while ((statement = parse_statement(parser)))
		g_array_append_val(block->statements, statement);

	if (!next_token(parser, TOKEN_TYPE_CLOSE_CURLY_BRACKET, NULL))
		parse_error(parser, "Expected closing curly bracket in block");

	return block;
}

static void free_block(struct ast_block *block)
{
	if (block == NULL)
		return;

	for (uint32_t i = 0; i < block->fields->len; i++)
		free_field(g_array_index(block->fields, struct ast_field *, i));
	for (uint32_t i = 0; i < block->statements->len; i++)
		free_statement(g_array_index(block->statements,
					     struct ast_statement *, i));
	g_array_free(block->fields, true);
	g_array_free(block->statements, true);

	g_free(block);
}

static struct ast_method_argument *parse_method_argument(struct parser *parser)
{
	struct ast_type *type = parse_type(parser);
	if (type == NULL)
		return NULL;

	struct ast_method_argument *argument =
		g_new0(struct ast_method_argument, 1);
	argument->type = type;

	argument->identifier = parse_identifier(parser);
	if (argument->identifier == NULL)
		parse_error(parser, "Expected identifier in method argument");

	return argument;
}

static void free_method_argument(struct ast_method_argument *argument)
{
	if (argument == NULL)
		return;

	free_type(argument->type);
	free_identifier(argument->identifier);

	g_free(argument);
}

static struct ast_method *parse_method(struct parser *parser)
{
	struct ast_type *type = NULL;
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_VOID, NULL)) {
		type = parse_type(parser);
		if (type == NULL)
			return NULL;
	}

	struct ast_method *method = g_new0(struct ast_method, 1);
	method->type = type;

	method->identifier = parse_identifier(parser);
	if (method->identifier == NULL)
		parse_error(parser,
			    "Expected identifier in method declaration");

	if (!next_token(parser, TOKEN_TYPE_OPEN_PARENTHESIS, NULL))
		parse_error(parser,
			    "Expected parenthesis in method declaraction");

	method->arguments =
		g_array_new(false, false, sizeof(struct ast_method_argument *));

	while (!next_token(parser, TOKEN_TYPE_CLOSE_PARENTHESIS, NULL)) {
		struct ast_method_argument *argument =
			parse_method_argument(parser);
		if (argument == NULL) {
			parse_error(parser,
				    "Expected argument in method declaration");
			break;
		}
		g_array_append_val(method->arguments, argument);

		if (next_token(parser, TOKEN_TYPE_COMMA, NULL) &&
		    peek_token(parser, 0, TOKEN_TYPE_CLOSE_PARENTHESIS))
			parse_error(parser,
				    "Extra comma in method declaration");
	}

	method->block = parse_block(parser);
	if (method->block == NULL)
		parse_error(parser, "Expected block after method declaration");

	return method;
}

static void free_method(struct ast_method *method)
{
	if (method == NULL)
		return;

	free_type(method->type);
	free_identifier(method->identifier);
	for (uint32_t i = 0; i < method->arguments->len; i++)
		free_method_argument(g_array_index(
			method->arguments, struct ast_method_argument *, i));
	g_array_free(method->arguments, true);
	free_block(method->block);

	g_free(method);
}

static struct ast_field_identifier *
parse_field_identifier(struct parser *parser)
{
	struct ast_identifier *identifier = parse_identifier(parser);
	if (identifier == NULL)
		return NULL;

	struct ast_field_identifier *field_identifier =
		g_new0(struct ast_field_identifier, 1);
	field_identifier->identifier = identifier;

	if (next_token(parser, TOKEN_TYPE_OPEN_SQUARE_BRACKET, NULL)) {
		field_identifier->array_length = parse_int_literal(parser);

		if (!next_token(parser, TOKEN_TYPE_CLOSE_SQUARE_BRACKET, NULL))
			parse_error(
				parser,
				"Expected closing square bracket in field delcaration");
	}

	if (next_token(parser, TOKEN_TYPE_ASSIGN, NULL)) {
		field_identifier->initializer = parse_initializer(parser);
		if (field_identifier->initializer == NULL)
			parse_error(
				parser,
				"Expected initializer in field declaration");
	}

	return field_identifier;
}

void free_field_identifier(struct ast_field_identifier *field_identifier)
{
	if (field_identifier == NULL)
		return;

	free_identifier(field_identifier->identifier);
	free_int_literal(field_identifier->array_length);
	free_initializer(field_identifier->initializer);

	g_free(field_identifier);
}

static struct ast_field *parse_field(struct parser *parser)
{
	bool constant = next_token(parser, TOKEN_TYPE_KEYWORD_CONST, NULL);

	struct ast_type *type = parse_type(parser);
	if (type == NULL) {
		if (constant)
			parse_error(parser,
				    "Expected type in field declaration");
		else
			return NULL;
	}

	struct ast_field *field = g_new0(struct ast_field, 1);
	field->constant = constant;
	field->type = type;

	field->field_identifiers = g_array_new(
		false, false, sizeof(struct ast_field_identifier *));

	while (true) {
		struct ast_field_identifier *field_identifier =
			parse_field_identifier(parser);
		if (field_identifier == NULL)
			parse_error(parser,
				    "Expected identifier in field declaration");

		g_array_append_val(field->field_identifiers, field_identifier);

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

static void free_field(struct ast_field *field)
{
	if (field == NULL)
		return;

	free_type(field->type);
	for (uint32_t i = 0; i < field->field_identifiers->len; i++)
		free_field_identifier(
			g_array_index(field->field_identifiers,
				      struct ast_field_identifier *, i));
	g_array_free(field->field_identifiers, true);

	g_free(field);
}

static struct ast_import *parse_import(struct parser *parser)
{
	if (!next_token(parser, TOKEN_TYPE_KEYWORD_IMPORT, NULL))
		return NULL;

	struct ast_import *import = g_new0(struct ast_import, 1);

	import->identifier = parse_identifier(parser);
	if (import->identifier == NULL)
		parse_error(parser,
			    "Expected identifier in import declaration");

	if (!next_token(parser, TOKEN_TYPE_SEMICOLON, NULL))
		parse_error(parser, "Expected semicolon in import declaration");

	return import;
}

static void free_import(struct ast_import *import)
{
	if (import == NULL)
		return;

	free_identifier(import->identifier);

	g_free(import);
}

static struct ast_program *parse_program(struct parser *parser)
{
	struct ast_program *program = g_new0(struct ast_program, 1);
	program->imports =
		g_array_new(false, false, sizeof(struct ast_import *));
	program->fields = g_array_new(false, false, sizeof(struct ast_field *));
	program->methods =
		g_array_new(false, false, sizeof(struct ast_method *));

	struct ast_import *import;
	while ((import = parse_import(parser)))
		g_array_append_val(program->imports, import);

	while (peek_field(parser)) {
		struct ast_field *field = parse_field(parser);
		g_array_append_val(program->fields, field);
	}

	struct ast_method *method;
	while ((method = parse_method(parser)))
		g_array_append_val(program->methods, method);

	if (parser->position != parser->token_count)
		parse_error(
			parser,
			"Unrecognized or unexpected declaration in program");

	return program;
}

static void free_program(struct ast_program *program)
{
	if (program == NULL)
		return;

	for (uint32_t i = 0; i < program->imports->len; i++)
		free_import(g_array_index(program->imports, struct ast_import *,
					  i));
	for (uint32_t i = 0; i < program->fields->len; i++)
		free_field(
			g_array_index(program->fields, struct ast_field *, i));
	for (uint32_t i = 0; i < program->methods->len; i++)
		free_method(g_array_index(program->methods, struct ast_method *,
					  i));
	g_array_free(program->imports, true);
	g_array_free(program->fields, true);
	g_array_free(program->methods, true);

	g_free(program);
}

struct parser *parser_new(void)
{
	struct parser *parser = g_new0(struct parser, 1);
	return parser;
}

int parser_parse(struct parser *parser, const char *source,
		 struct scanner *scanner, struct ast **ast)
{
	GArray *tokens;
	if (scanner_tokenize(scanner, source, false, &tokens) != 0)
		return -1;

	*parser = (struct parser){
		.tokens = &g_array_index(tokens, struct token, 0),
		.token_count = tokens->len,
		.source = source,
		.position = 0,
		.parse_error = false,
	};
	struct ast_program *program = parse_program(parser);

	g_array_free(tokens, true);

	if (ast != NULL && !parser->parse_error) {
		*ast = g_new0(struct ast, 1);
		(*ast)->program = program;
	} else {
		free_program(program);
	}

	return parser->parse_error ? -1 : 0;
}

void parser_free(struct parser *parser)
{
	g_free(parser);
}
