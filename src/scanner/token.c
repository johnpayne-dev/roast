#include "scanner/token.h"

#define VALID_CHAR_PATTERN "(?:[ -!#-&\\(-\\[\\]-~]|\\\\['\"\\\\tn])"

#define EXTENDED_CHAR_PATTERN "(?:[ -\\[\\]-~]|\\\\[ -~])"

static const char *REGEX_PATTERNS[] = {
	[TOKEN_TYPE_WHITESPACE] = "\\s+",

	[TOKEN_TYPE_KEYWORD_BOOL] = "bool\\b",
	[TOKEN_TYPE_KEYWORD_BREAK] = "break\\b",
	[TOKEN_TYPE_KEYWORD_CONST] = "const\\b",
	[TOKEN_TYPE_KEYWORD_CONTINUE] = "continue\\b",
	[TOKEN_TYPE_KEYWORD_ELSE] = "else\\b",
	[TOKEN_TYPE_KEYWORD_FALSE] = "false\\b",
	[TOKEN_TYPE_KEYWORD_FOR] = "for\\b",
	[TOKEN_TYPE_KEYWORD_IF] = "if\\b",
	[TOKEN_TYPE_KEYWORD_IMPORT] = "import\\b",
	[TOKEN_TYPE_KEYWORD_INT] = "int\\b",
	[TOKEN_TYPE_KEYWORD_LEN] = "len\\b",
	[TOKEN_TYPE_KEYWORD_RETURN] = "return\\b",
	[TOKEN_TYPE_KEYWORD_TRUE] = "true\\b",
	[TOKEN_TYPE_KEYWORD_VOID] = "void\\b",
	[TOKEN_TYPE_KEYWORD_WHILE] = "while\\b",

	[TOKEN_TYPE_HEX_LITERAL] = "0x[0-9a-fA-F]+",
	[TOKEN_TYPE_HEX_LITERAL_INCOMPLETE] = "0x\\b",
	[TOKEN_TYPE_DECIMAL_LITERAL] = "[0-9]+",
	[TOKEN_TYPE_CHAR_LITERAL] = "'" VALID_CHAR_PATTERN "'",
	[TOKEN_TYPE_CHAR_LITERAL_EMPTY] = "''",
	[TOKEN_TYPE_CHAR_LITERAL_INVALID_CHAR] = "'" EXTENDED_CHAR_PATTERN "'",
	[TOKEN_TYPE_CHAR_LITERAL_INVALID_LENGTH] = "'" EXTENDED_CHAR_PATTERN
						   "*?'",
	[TOKEN_TYPE_CHAR_LITERAL_UNTERMINATED] = "'" EXTENDED_CHAR_PATTERN "*",
	[TOKEN_TYPE_STRING_LITERAL] = "\"" VALID_CHAR_PATTERN "*?\"",
	[TOKEN_TYPE_STRING_LITERAL_INVALID_CHAR] = "\"" EXTENDED_CHAR_PATTERN
						   "*?\"",
	[TOKEN_TYPE_STRING_LITERAL_UNTERMINATED] = "\"" EXTENDED_CHAR_PATTERN
						   "*",

	[TOKEN_TYPE_IDENTIFIER] = "[a-zA-Z_][a-zA-Z0-9_]*",

	[TOKEN_TYPE_LINE_COMMENT] = "\\/\\/.*",
	[TOKEN_TYPE_MULTILINE_COMMENT] = "\\/\\*[\\s\\S]*?\\*\\/",
	[TOKEN_TYPE_MULTILINE_COMMENT_UNTERMINATED] = "\\/\\*[\\s\\S]*",

	[TOKEN_TYPE_ADD_ASSIGN] = "\\+=",
	[TOKEN_TYPE_SUB_ASSIGN] = "-=",
	[TOKEN_TYPE_MUL_ASSIGN] = "\\*=",
	[TOKEN_TYPE_DIV_ASSIGN] = "\\/=",
	[TOKEN_TYPE_MOD_ASSIGN] = "%=",
	[TOKEN_TYPE_INCREMENT] = "\\+\\+",
	[TOKEN_TYPE_DECREMENT] = "--",
	[TOKEN_TYPE_EQUAL] = "==",
	[TOKEN_TYPE_NOT_EQUAL] = "!=",
	[TOKEN_TYPE_LESS_EQUAL] = "<=",
	[TOKEN_TYPE_GREATER_EQUAL] = ">=",
	[TOKEN_TYPE_AND] = "&&",
	[TOKEN_TYPE_OR] = "\\|\\|",

	[TOKEN_TYPE_OPEN_PARENTHESIS] = "\\(",
	[TOKEN_TYPE_CLOSE_PARENTHESIS] = "\\)",
	[TOKEN_TYPE_OPEN_SQUARE_BRACKET] = "\\[",
	[TOKEN_TYPE_CLOSE_SQUARE_BRACKET] = "\\]",
	[TOKEN_TYPE_OPEN_CURLY_BRACKET] = "{",
	[TOKEN_TYPE_CLOSE_CURLY_BRACKET] = "}",
	[TOKEN_TYPE_ASSIGN] = "=",
	[TOKEN_TYPE_ADD] = "\\+",
	[TOKEN_TYPE_SUB] = "-",
	[TOKEN_TYPE_MUL] = "\\*",
	[TOKEN_TYPE_DIV] = "\\/",
	[TOKEN_TYPE_MOD] = "%",
	[TOKEN_TYPE_LESS] = "<",
	[TOKEN_TYPE_GREATER] = ">",
	[TOKEN_TYPE_NOT] = "!",
	[TOKEN_TYPE_COMMA] = ",",
	[TOKEN_TYPE_SEMICOLON] = ";",
};

static const char *ERROR_MESSAGES[] = {
	[TOKEN_TYPE_HEX_LITERAL_INCOMPLETE] = "Incomplete hex literal",
	[TOKEN_TYPE_CHAR_LITERAL_EMPTY] = "Empty char literal",
	[TOKEN_TYPE_CHAR_LITERAL_INVALID_CHAR] =
		"Invalid character in char literal",
	[TOKEN_TYPE_CHAR_LITERAL_INVALID_LENGTH] =
		"Invalid char literal length (must only contain 1 char)",
	[TOKEN_TYPE_CHAR_LITERAL_UNTERMINATED] = "Unterminated char literal",
	[TOKEN_TYPE_STRING_LITERAL_INVALID_CHAR] =
		"Invalid character in string literal",
	[TOKEN_TYPE_STRING_LITERAL_UNTERMINATED] =
		"Unterminated string literal",
	[TOKEN_TYPE_MULTILINE_COMMENT_UNTERMINATED] = "Unterminated comment",
	[TOKEN_TYPE_UNKNOWN] = "Unrecognized token",
};

bool token_type_is_ignored(enum token_type token_type)
{
	return token_type == TOKEN_TYPE_WHITESPACE ||
	       token_type == TOKEN_TYPE_LINE_COMMENT ||
	       token_type == TOKEN_TYPE_MULTILINE_COMMENT;
}

bool token_type_is_error(enum token_type token_type)
{
	g_assert(token_type < G_N_ELEMENTS(ERROR_MESSAGES));
	return ERROR_MESSAGES[token_type] != NULL;
}

const char *token_type_error_message(enum token_type token_type)
{
	g_assert(token_type < G_N_ELEMENTS(ERROR_MESSAGES));
	return ERROR_MESSAGES[token_type];
}

const char *token_type_regex_pattern(enum token_type token_type)
{
	g_assert(token_type < TOKEN_TYPE_COUNT);
	return REGEX_PATTERNS[token_type];
}

uint32_t token_get_line_number(struct token *token, const char *source)
{
	uint32_t line_number = 1;

	for (uint32_t i = 0; i < token->offset; i++) {
		if (source[i] == '\n')
			line_number++;

		if (source[i] == '\0')
			return 0;
	}

	return line_number;
}

uint32_t token_get_column_number(struct token *token, const char *source)
{
	for (int32_t i = token->offset; i >= 0; i--) {
		if (i == 0 || source[i - 1] == '\n')
			return token->offset - i;
	}

	return token->offset;
}

void token_print(struct token *token, const char *source)
{
	uint32_t line_number = token_get_line_number(token, source);

	g_print("%i ", line_number);
	if (token->type == TOKEN_TYPE_CHAR_LITERAL)
		g_print("CHARLITERAL ");
	if (token->type == TOKEN_TYPE_HEX_LITERAL ||
	    token->type == TOKEN_TYPE_DECIMAL_LITERAL)
		g_print("INTLITERAL ");
	if (token->type == TOKEN_TYPE_KEYWORD_TRUE ||
	    token->type == TOKEN_TYPE_KEYWORD_FALSE)
		g_print("BOOLEANLITERAL ");
	if (token->type == TOKEN_TYPE_STRING_LITERAL)
		g_print("STRINGLITERAL ");
	if (token->type == TOKEN_TYPE_IDENTIFIER)
		g_print("IDENTIFIER ");
	g_print("%.*s\n", token->length, &source[token->offset]);
}

void token_print_error(struct token *token, const char *source)
{
	uint32_t line_number = token_get_line_number(token, source);
	uint32_t column_number = token_get_column_number(token, source);
	const char *error_message = token_type_error_message(token->type);
	g_printerr("ERROR: %s at %i:%i: %.*s\n", error_message, line_number,
		   column_number, token->length, &source[token->offset]);
}
