#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <glib.h>

enum token_type {
	TOKEN_TYPE_WHITESPACE,

	TOKEN_TYPE_KEYWORD_BOOL,
	TOKEN_TYPE_KEYWORD_BREAK,
	TOKEN_TYPE_KEYWORD_CONST,
	TOKEN_TYPE_KEYWORD_CONTINUE,
	TOKEN_TYPE_KEYWORD_ELSE,
	TOKEN_TYPE_KEYWORD_FALSE,
	TOKEN_TYPE_KEYWORD_FOR,
	TOKEN_TYPE_KEYWORD_IF,
	TOKEN_TYPE_KEYWORD_IMPORT,
	TOKEN_TYPE_KEYWORD_INT,
	TOKEN_TYPE_KEYWORD_LEN,
	TOKEN_TYPE_KEYWORD_RETURN,
	TOKEN_TYPE_KEYWORD_TRUE,
	TOKEN_TYPE_KEYWORD_VOID,
	TOKEN_TYPE_KEYWORD_WHILE,

	TOKEN_TYPE_HEX_LITERAL,
	TOKEN_TYPE_HEX_LITERAL_INCOMPLETE,
	TOKEN_TYPE_DECIMAL_LITERAL,
	TOKEN_TYPE_CHAR_LITERAL,
	TOKEN_TYPE_CHAR_LITERAL_EMPTY,
	TOKEN_TYPE_CHAR_LITERAL_INVALID_CHAR,
	TOKEN_TYPE_CHAR_LITERAL_INVALID_LENGTH,
	TOKEN_TYPE_CHAR_LITERAL_UNTERMINATED,
	TOKEN_TYPE_STRING_LITERAL,
	TOKEN_TYPE_STRING_LITERAL_INVALID_CHAR,
	TOKEN_TYPE_STRING_LITERAL_UNTERMINATED,

	TOKEN_TYPE_IDENTIFIER,

	TOKEN_TYPE_LINE_COMMENT,
	TOKEN_TYPE_MULTILINE_COMMENT,
	TOKEN_TYPE_MULTILINE_COMMENT_UNTERMINATED,

	TOKEN_TYPE_ADD_ASSIGN,
	TOKEN_TYPE_SUB_ASSIGN,
	TOKEN_TYPE_MUL_ASSIGN,
	TOKEN_TYPE_DIV_ASSIGN,
	TOKEN_TYPE_MOD_ASSIGN,
	TOKEN_TYPE_INCREMENT,
	TOKEN_TYPE_DECREMENT,
	TOKEN_TYPE_EQUAL,
	TOKEN_TYPE_NOT_EQUAL,
	TOKEN_TYPE_LESS_EQUAL,
	TOKEN_TYPE_GREATER_EQUAL,
	TOKEN_TYPE_AND,
	TOKEN_TYPE_OR,

	TOKEN_TYPE_OPEN_PARENTHESIS,
	TOKEN_TYPE_CLOSE_PARENTHESIS,
	TOKEN_TYPE_OPEN_SQUARE_BRACKET,
	TOKEN_TYPE_CLOSE_SQUARE_BRACKET,
	TOKEN_TYPE_OPEN_CURLY_BRACKET,
	TOKEN_TYPE_CLOSE_CURLY_BRACKET,
	TOKEN_TYPE_ASSIGN,
	TOKEN_TYPE_ADD,
	TOKEN_TYPE_SUB,
	TOKEN_TYPE_MUL,
	TOKEN_TYPE_DIV,
	TOKEN_TYPE_MOD,
	TOKEN_TYPE_LESS,
	TOKEN_TYPE_GREATER,
	TOKEN_TYPE_NOT,
	TOKEN_TYPE_COMMA,
	TOKEN_TYPE_SEMICOLON,

	TOKEN_TYPE_COUNT,
	TOKEN_TYPE_UNKNOWN,
};

bool token_type_is_ignored(enum token_type token_type);

bool token_type_is_error(enum token_type token_type);

const char *token_type_error_message(enum token_type token_type);

const char *token_type_regex_pattern(enum token_type token_type);

struct token {
	enum token_type type;
	uint32_t offset;
	uint32_t length;
	const char *source;
};

uint32_t token_get_line_number(struct token *token);

uint32_t token_get_column_number(struct token *token);

char *token_get_string(struct token *token);

void token_print(struct token *token);

void token_print_error(struct token *token);
