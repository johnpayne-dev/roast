#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <glib.h>

#include "semantics/symbol_table.h"
#include "parser/ast.h"

enum ir_data_type {
	IR_DATA_TYPE_VOID,
	IR_DATA_TYPE_VOID_ARRAY,
	IR_DATA_TYPE_BOOL,
	IR_DATA_TYPE_BOOL_ARRAY,
	IR_DATA_TYPE_INT,
	IR_DATA_TYPE_INT_ARRAY,
};

bool ir_data_type_is_array(enum ir_data_type type);

struct ir_program {
	struct token token;
	fields_table_t *fields_table;
	methods_table_t *methods_table;
	GArray *imports;
	GArray *fields;
	GArray *methods;
};

struct ir_program *ir_program_new(struct ast_node **nodes);
void ir_program_free(struct ir_program *program);

struct ir_method {
	struct token token;
	bool imported;
	enum ir_data_type return_type;
	char *identifier;
	GArray *arguments;
	struct ir_block *block;
};

struct ir_method *ir_method_new(struct ast_node **nodes);
struct ir_method *ir_method_new_from_import(struct ast_node **nodes);
void ir_method_free(struct ir_method *method);

struct ir_field {
	struct token token;
	enum ir_data_type type;
	bool constant;
	char *identifier;
	int64_t array_length;
	struct ir_initializer *initializer;
};

struct ir_field *ir_field_new(struct ast_node **nodes, bool constant,
			      enum ir_data_type type);
struct ir_field *ir_field_new_from_method_argument(struct ast_node **nodes);
void ir_field_free(struct ir_field *field);

struct ir_initializer {
	struct token token;
	bool array;
	GArray *literals;
};

struct ir_initializer *ir_initializer_new(struct ast_node **nodes);
void ir_initializer_free(struct ir_initializer *initializer);

struct ir_block {
	struct token token;
	fields_table_t *fields_table;
	GArray *fields;
	GArray *statements;
};

struct ir_block *ir_block_new(struct ast_node **nodes);
void ir_block_free(struct ir_block *block);

struct ir_statement {
	struct token token;

	enum ir_statement_type {
		IR_STATEMENT_TYPE_ASSIGNMENT,
		IR_STATEMENT_TYPE_METHOD_CALL,
		IR_STATEMENT_TYPE_IF,
		IR_STATEMENT_TYPE_FOR,
		IR_STATEMENT_TYPE_WHILE,
		IR_STATEMENT_TYPE_RETURN,
		IR_STATEMENT_TYPE_BREAK,
		IR_STATEMENT_TYPE_CONTINUE,
	} type;

	union {
		struct ir_assignment *assignment;
		struct ir_method_call *method_call;
		struct ir_if_statement *if_statement;
		struct ir_for_statement *for_statement;
		struct ir_while_statement *while_statement;
		struct ir_expression *return_expression;
	};
};

struct ir_statement *ir_statement_new(struct ast_node **nodes);
void ir_statement_free(struct ir_statement *statement);

struct ir_assignment {
	struct token token;
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

struct ir_assignment *ir_assignment_new(struct ast_node **nodes);
struct ir_assignment *
ir_assignment_new_from_identifier(char *identifier,
				  struct ir_expression *expression);
void ir_assignment_free(struct ir_assignment *assignment);

struct ir_method_call {
	struct token token;
	char *identifier;
	GArray *arguments;
};

struct ir_method_call *ir_method_call_new(struct ast_node **nodes);
void ir_method_call_free(struct ir_method_call *method_call);

struct ir_method_call_argument {
	struct token token;
	enum ir_method_call_argument_type {
		IR_METHOD_CALL_ARGUMENT_TYPE_STRING,
		IR_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION,
	} type;

	union {
		char *string;
		struct ir_expression *expression;
	};
};

struct ir_method_call_argument *
ir_method_call_argument_new(struct ast_node **nodes);
void ir_method_call_argument_free(struct ir_method_call_argument *argument);

struct ir_if_statement {
	struct token token;
	struct ir_expression *condition;
	struct ir_block *if_block;
	struct ir_block *else_block;
};

struct ir_if_statement *ir_if_statement_new(struct ast_node **nodes);
void ir_if_statement_free(struct ir_if_statement *statement);

struct ir_for_statement {
	struct token token;
	struct ir_assignment *initial;
	struct ir_expression *condition;
	struct ir_for_update *update;
	struct ir_block *block;
};

struct ir_for_statement *ir_for_statement_new(struct ast_node **nodes);
void ir_for_statement_free(struct ir_for_statement *statement);

struct ir_for_update {
	struct token token;
	enum ir_for_update_type {
		IR_FOR_UPDATE_TYPE_METHOD_CALL,
		IR_FOR_UPDATE_TYPE_ASSIGNMENT,
	} type;

	union {
		struct ir_method_call *method_call;
		struct ir_assignment *assignment;
	};
};

struct ir_for_update *ir_for_update_new(struct ast_node **nodes);
void ir_for_update_free(struct ir_for_update *update);

struct ir_while_statement {
	struct token token;
	struct ir_expression *condition;
	struct ir_block *block;
};

struct ir_while_statement *ir_while_statement_new(struct ast_node **nodes);
void ir_while_statement_free(struct ir_while_statement *statement);

struct ir_location {
	struct token token;
	char *identifier;
	struct ir_expression *index;
};

struct ir_location *ir_location_new(struct ast_node **nodes);
void ir_location_free(struct ir_location *location);

struct ir_expression {
	struct token token;

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
		struct ir_length_expression *length_expression;
		struct ir_method_call *method_call;
		struct ir_literal *literal;
		struct ir_location *location;
	};
};

struct ir_expression *ir_expression_new(struct ast_node **nodes);
void ir_expression_free(struct ir_expression *expression);

struct ir_binary_expression {
	struct token token;
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

struct ir_binary_expression *ir_binary_expression_new(struct ast_node **nodes);
void ir_binary_expression_free(struct ir_binary_expression *expression);

struct ir_length_expression {
	char *identifier;
	int64_t length;
};

struct ir_length_expression *ir_length_expression_new(struct ast_node **nodes);
void ir_length_expression_free(struct ir_length_expression *length_expression);

struct ir_literal {
	struct token token;
	bool negate;

	enum ir_literal_type {
		IR_LITERAL_TYPE_BOOL,
		IR_LITERAL_TYPE_CHAR,
		IR_LITERAL_TYPE_INT,
	} type;

	uint64_t value;
};

struct ir_literal *ir_literal_new(struct ast_node **nodes);
void ir_literal_free(struct ir_literal *literal);

enum ir_data_type ir_data_type_from_ast(struct ast_node **nodes);

uint64_t ir_int_literal_from_ast(struct ast_node **nodes, bool negate);

bool ir_bool_literal_from_ast(struct ast_node **nodes);

char ir_char_literal_from_ast(struct ast_node **nodes);

char *ir_string_literal_from_ast(struct ast_node **nodes);

char *ir_identifier_from_ast(struct ast_node **nodes);
