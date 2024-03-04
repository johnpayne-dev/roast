#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "parser/ast.h"
#include "semantics/symbol_table.h"

bool ir_is_error(void);

enum ir_type {
	IR_TYPE_VOID,
	IR_TYPE_BOOL,
	IR_TYPE_INT,
};

enum ir_type ir_type_from_ast(struct ast_node *node);

int64_t ir_int_literal_from_ast(struct ast_node *node);

bool ir_bool_literal_from_ast(struct ast_node *node);

char ir_char_literal_from_ast(struct ast_node *node);

char *ir_string_literal_from_ast(struct ast_node *node);

struct ir_program {
	symbol_table_t *symbol_table;

	size_t import_count;
	char **imports;

	size_t field_count;
	struct ir_field_descriptor **fields;

	size_t method_count;
	struct ir_method_descriptor **methods;
};

struct ir_program *ir_program_new(struct ast_node *node);
void ir_program_free(struct ir_program *program);

struct ir_method_descriptor {
	bool imported;
	enum ir_type return_type;
	struct ir_argument_descriptor *arguments;
	struct ir_block *block;
};

struct ir_argument_descriptor {
	enum ir_type type;
	char *identifier;
};

struct ir_method_descriptor *ir_method_descriptor_new(struct ast_node *node);
void ir_method_descriptor_free(struct ir_method_descriptor *method);

struct ir_field_descriptor {
	enum ir_type type;
	bool constant;
	bool array;
	size_t array_length;
	int64_t *initializers;
};

struct ir_field_descriptor *ir_field_descriptor_new(struct ast_node *node);
void ir_field_descriptor_free(struct ir_field_descriptor *field);

struct ir_block {
	symbol_table_t *symbol_table;

	size_t field_count;
	struct ir_field **fields;

	size_t statement_count;
	struct ir_statement **statements;
};

struct ir_block *ir_block_new(struct ast_node *node);
void ir_block_free(struct ir_block *block);

struct ir_statement {
	enum ir_statement_type {
		IR_STATEMENT_TYPE_ASSIGN,
		IR_STATEMENT_TYPE_IF,
		IR_STATEMENT_TYPE_FOR,
		IR_STATEMENT_TYPE_WHILE,
		IR_STATEMENT_TYPE_RETURN,
		IR_STATEMENT_TYPE_BREAK,
		IR_STATEMENT_TYPE_CONTINUE,
	} type;

	union {
		struct ir_assignment *assign_statement;
		struct ir_method_call *method_call_statement;
		struct ir_if_statement *if_statement;
		struct ir_for_statement *for_statement;
		struct ir_while_statement *while_statement;
		struct ir_expression *return_statement;
	};
};

struct ir_statement *ir_statement_new(struct ast_node *node);
void ir_statement_free(struct ir_statement *statement);

struct ir_assignment {
	struct ir_location *location;

	enum ir_assign_operator {
		IR_ASSIGN_OPERATOR_SET,
		IR_ASSIGN_OPERATOR_ADD,
		IR_ASSIGN_OPERATOR_SUB,
		IR_ASSIGN_OPERATOR_MUL,
		IR_ASSIGN_OPERATOR_DIV,
		IR_ASSIGN_OPERATOR_MOD,
		IR_ASSIGN_OPERATOR_INCREMENT,
		IR_ASSIGN_OPERATOR_DECREMENT,
	} assign_operator;

	struct ir_expression *expression;
};

struct ir_assignment *ir_assignment_new(struct ast_node *node);
void ir_assignment_free(struct ir_assignment *assignment);

struct ir_method_call {
	char *identifier;
	struct ir_expression **arguments;
};

struct ir_method_call *ir_method_call_new(struct ast_node *node);
void ir_method_call_free(struct ir_method_call *method_call);

struct ir_if_statement {
	struct ir_expression *condition;
	struct ir_block *if_block;
	struct ir_block *else_block;
};

struct ir_if_statement *ir_if_statement_new(struct ast_node *node);
void ir_if_statement_free(struct ir_if_statement *statement);

struct ir_for_statement {
	char *identifier;
	struct ir_expression *initializer;
	struct ir_expression *condition;
	struct ir_assignment *update;
	struct ir_block *block;
};

struct ir_for_statement *ir_for_statement_new(struct ast_node *node);
void ir_for_statement_free(struct ir_for_statement *statement);

struct ir_while_statement {
	struct ir_expression *condition;
	struct ir_block *block;
};

struct ir_while_statement *ir_while_statement_new(struct ast_node *node);
void ir_while_statement_free(struct ir_while_statement *statement);

struct ir_location {
	char *identifier;
	struct ir_expression *index;
};

struct ir_location *ir_location_new(struct ast_node *node);
void ir_location_free(struct ir_location *location);

struct ir_expression {
	enum ir_expression_type {
		IR_EXPRESSION_TYPE_BINARY,
		IR_EXPRESSION_TYPE_NOT,
		IR_EXPRESSION_TYPE_NEGATE,
		IR_EXPRESSION_TYPE_LEN,
		IR_EXPRESSION_TYPE_METHOD_CALL,
		IR_EXPRESSION_TYPE_LITERAL,
		IR_EXPRESSION_TYPE_LOCATION,
	} type;

	union {
		struct ir_binary_expression *binary_expression;
		struct ir_expression *not_expression;
		struct ir_expression *negate_expression;
		char *len_identifier;
		struct ir_method_call *method_call;
		struct ir_literal *literal;
		struct ir_location *location;
	};
};

struct ir_expression *ir_expression_new(struct ast_node *node);
void ir_expression_free(struct ir_expression *expression);

struct ir_binary_expression {
	struct ir_expression *left;

	enum ir_binary_operator {
		IR_BINARY_OPERATOR_OR,
		IR_BINARY_OPERATOR_AND,
		IR_BINARY_OPERATOR_EQUAL,
		IR_BINARY_OPERATOR_NOT_EQUAL,
		IR_BINARY_OPERATOR_LESS,
		IR_BINARY_OPERATOR_LESS_EQUAL,
		IR_BINARY_OPERATOR_GREATER_EQUAL,
		IR_BINARY_OPERATOR_GREATER,
		IR_BINARY_OPERATOR_ADD,
		IR_BINARY_OPERATOR_SUB,
		IR_BINARY_OPERATOR_MUL,
		IR_BINARY_OPERATOR_DIV,
		IR_BINARY_OPERATOR_MOD,
	} binary_operator;

	struct ir_expression *right;
};

struct ir_binary_expression *ir_binary_expression_new(struct ast_node *node);
void ir_binary_expression_free(struct ir_binary_expression *expression);

struct ir_literal {
	enum ir_literal_type {
		IR_LITERAL_TYPE_BOOL,
		IR_LITERAL_TYPE_CHAR,
		IR_LITERAL_TYPE_INT,
		IR_LITERAL_TYPE_STRING,
	} type;

	union {
		bool bool_literal;
		char char_literal;
		int64_t int_literal;
		char *string_literal;
	};
};

struct ir_literal *ir_literal_new(struct ast_node *node);
void ir_literal_free(struct ir_literal *expression);
