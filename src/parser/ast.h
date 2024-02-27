#pragma once

#include <glib.h>

#include "scanner/token.h"

struct ast {
	struct ast_program *program;
};

struct ast_program {
	GArray *imports;
	GArray *fields;
	GArray *methods;
};

struct ast_import {
	struct ast_identifier *identifier;
};

struct ast_field {
	bool constant;
	struct ast_type *type;
	GArray *field_identifiers;
};

struct ast_field_identifier {
	struct ast_identifier *identifier;
	struct ast_int_literal *array_length;
	struct ast_initializer *initializer;
};

struct ast_method {
	struct ast_type *type;
	struct ast_identifier *identifier;
	GArray *arguments;
	struct ast_block *block;
};

struct ast_method_argument {
	struct ast_type *type;
	struct ast_identifier *identifier;
};

struct ast_type {
	enum ast_type_type {
		AST_TYPE_INT,
		AST_TYPE_BOOL,
	} type;

	struct token token;
};

struct ast_block {
	GArray *fields;
	GArray *statements;
};

struct ast_initializer {
	enum ast_initializer_type {
		AST_INITIALIZER_TYPE_LITERAL,
		AST_INITIALIZER_TYPE_ARRAY_LITERAL,
	} type;

	union {
		struct ast_literal *literal;
		struct ast_array_literal *array_literal;
	};
};

struct ast_statement {
	enum ast_statement_type {
		AST_STATEMENT_TYPE_ASSIGN,
		AST_STATEMENT_TYPE_METHOD_CALL,
		AST_STATEMENT_TYPE_IF,
		AST_STATEMENT_TYPE_FOR,
		AST_STATEMENT_TYPE_WHILE,
		AST_STATEMENT_TYPE_RETURN,
		AST_STATEMENT_TYPE_BREAK,
		AST_STATEMENT_TYPE_CONTINUE,
	} type;

	union {
		struct ast_assign_statement *assign_statement;
		struct ast_method_call *method_call;
		struct ast_if_statement *if_statement;
		struct ast_for_statement *for_statement;
		struct ast_while_statement *while_statement;
		struct ast_expression *return_expression;
	};
};

struct ast_assign_statement {
	struct ast_location *location;
	struct ast_assign_expression *assign_expression;
};

struct ast_if_statement {
	struct ast_expression *expression;
	struct ast_block *if_block;
	struct ast_block *else_block;
};

struct ast_for_statement {
	struct ast_identifier *identifier;
	struct ast_expression *initial_expression;
	struct ast_expression *break_expression;
	struct ast_for_update *for_update;
	struct ast_block *block;
};

struct ast_while_statement {
	struct ast_expression *expression;
	struct ast_block *block;
};

struct ast_for_update {
	enum ast_for_update_type {
		AST_FOR_UPDATE_TYPE_ASSIGN,
		AST_FOR_UPDATE_TYPE_METHOD_CALL,
	} type;

	union {
		struct ast_assign_statement assign_statement;
		struct ast_method_call *method_call;
	};
};

struct ast_assign_expression {
	enum ast_assign_expression_type {
		AST_ASSIGN_EXPRESSION_TYPE_INCREMENT,
		AST_ASSIGN_EXPRESSION_TYPE_ASSIGNMENT,
	} type;

	union {
		struct ast_increment_operator *increment_operator;
		struct ast_assign_operator *assign_operator;
	};

	struct ast_expression *expression;
};

struct ast_assign_operator {
	enum ast_assign_operator_type {
		AST_ASSIGN_OPERATOR_TYPE_ASSIGN,
		AST_ASSIGN_OPERATOR_TYPE_ADD,
		AST_ASSIGN_OPERATOR_TYPE_SUB,
		AST_ASSIGN_OPERATOR_TYPE_MUL,
		AST_ASSIGN_OPERATOR_TYPE_DIV,
		AST_ASSIGN_OPERATOR_TYPE_MOD,
	} type;

	struct token token;
};

struct ast_increment_operator {
	enum ast_increment_operator_type {
		AST_INCREMENT_OPERATOR_TYPE_ADD,
		AST_INCREMENT_OPERATOR_TYPE_SUB,
	} type;

	struct token token;
};

struct ast_method_call {
	struct ast_identifier *identifer;
	GArray *arguments;
};

struct ast_method_call_argument {
	enum ast_method_call_argument_type {
		AST_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION,
		AST_METHOD_CALL_ARGUMENT_TYPE_STRING,
	} type;

	union {
		struct ast_expression *expression;
		struct ast_string_literal *string_literal;
	};
};

struct ast_location {
	struct ast_identifier *identifier;
	struct ast_expression *index_expression;
};

struct ast_expression {
	enum ast_expression_type {
		AST_EXPRESSION_TYPE_BINARY,
		AST_EXPRESSION_TYPE_UNARY,
	} type;

	union {
		struct ast_unary_expression *unary;
		struct ast_binary_expression *binary;
	};
};

struct ast_unary_expression {
	enum ast_unary_expression_type {
		AST_UNARY_EXPRESSION_TYPE_LOCATION,
		AST_UNARY_EXPRESSION_TYPE_METHOD_CALL,
		AST_UNARY_EXPRESSION_TYPE_LITERAL,
		AST_UNARY_EXPRESSION_TYPE_LEN,
		AST_UNARY_EXPRESSION_TYPE_NEGATE,
		AST_UNARY_EXPRESSION_TYPE_NOT,
		AST_UNARY_EXPRESSION_TYPE_PARENTHESIS,
	} type;

	union {
		struct ast_location *location;
		struct ast_method_call *method_call;
		struct ast_literal *literal;
		struct ast_identifier *len_identifier;
		struct ast_unary_expression *negate_expression;
		struct ast_unary_expression *not_expression;
		struct ast_expression *parenthesis_expression;
	};
};

struct ast_binary_expression {
	struct ast_expression *left;
	struct ast_binary_operator *binary_operator;
	struct ast_expression *right;
};

struct ast_binary_operator {
	enum ast_binary_operator_type {
		AST_BINARY_OPERATOR_TYPE_ADD,
		AST_BINARY_OPERATOR_TYPE_SUB,
		AST_BINARY_OPERATOR_TYPE_MUL,
		AST_BINARY_OPERATOR_TYPE_DIV,
		AST_BINARY_OPERATOR_TYPE_MOD,
		AST_BINARY_OPERATOR_TYPE_LESS,
		AST_BINARY_OPERATOR_TYPE_GREATER,
		AST_BINARY_OPERATOR_TYPE_LESS_EQUAL,
		AST_BINARY_OPERATOR_TYPE_GREATER_EQUAL,
		AST_BINARY_OPERATOR_TYPE_EQUAL,
		AST_BINARY_OPERATOR_TYPE_NOT_EQUAL,
		AST_BINARY_OPERATOR_TYPE_AND,
		AST_BINARY_OPERATOR_TYPE_OR,
	} type;

	struct token token;
};

struct ast_literal {
	bool negate;

	enum ast_literal_type {
		AST_LITERAL_TYPE_INT,
		AST_LITERAL_TYPE_CHAR,
		AST_LITERAL_TYPE_BOOL,
	} type;

	union {
		struct ast_int_literal *int_literal;
		struct ast_char_literal *char_literal;
		struct ast_bool_literal *bool_literal;
	};
};

struct ast_array_literal {
	GArray *literals;
};

struct ast_int_literal {
	enum ast_int_literal_type {
		AST_INT_LITERAL_TYPE_DECIMAL,
		AST_INT_LITERAL_TYPE_HEX,
	} type;

	struct token token;
};

struct ast_bool_literal {
	enum ast_bool_literal_type {
		AST_BOOL_LITERAL_TYPE_TRUE,
		AST_BOOL_LITERAL_TYPE_FALSE,
	} type;

	struct token token;
};

struct ast_string_literal {
	struct token token;
};

struct ast_char_literal {
	struct token token;
};

struct ast_identifier {
	struct token token;
};
