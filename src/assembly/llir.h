#pragma once
#include "semantics/ir.h"
#include "assembly/temporary_variables.h"

enum llir_node_type {
	LLIR_NODE_TYPE_PROGRAM,
	LLIR_NODE_TYPE_IMPORT,
	LLIR_NODE_TYPE_FIELD,
	LLIR_NODE_TYPE_METHOD,
	LLIR_NODE_TYPE_BLOCK_START,
	LLIR_NODE_TYPE_ASSIGNMENT,
	LLIR_NODE_TYPE_LITERAL_ASSIGNMENT,
	LLIR_NODE_TYPE_INDEXED_ASSIGNMENT,
	LLIR_NODE_TYPE_BINARY_OPERATION,
	LLIR_NODE_TYPE_UNARY_OPERATION,
	LLIR_NODE_TYPE_METHOD_CALL,
	LLIR_NODE_TYPE_ARRAY_INDEX,
	LLIR_NODE_TYPE_BRANCH,
	LLIR_NODE_TYPE_JUMP,
	LLIR_NODE_TYPE_RETURN,
	LLIR_NODE_TYPE_BLOCK_END,
	LLIR_NODE_TYPE_PROGRAM_END,
};

struct llir_node {
	enum llir_node_type type;

	union {
		struct llir_import *import;
		struct llir_field *field;
		struct llir_method *method;
		struct llir_assignment *assignment;
		struct llir_literal_assignment *literal_assignment;
		struct llir_indexed_assignment *indexed_assignment;
		struct llir_increment *increment;
		struct llir_decrement *decrement;
		struct llir_binary_operation *binary_operation;
		struct llir_unary_operation *unary_operation;
		struct llir_method_call *method_call;
		struct llir_array_index *array_index;
		struct llir_branch *branch;
		struct llir_jump *jump;
	};

	struct llir_node *next;
};

struct llir_node *llir_node_new(enum llir_node_type type, void *data);
struct llir_node *llir_node_new_program(struct ir_program *ir_program);
struct llir_node *llir_node_new_import(struct ir_method *ir_method);
struct llir_node *llir_node_new_method(struct ir_method *ir_method);
struct llir_node *llir_node_new_field(struct ir_field *ir_field);
struct llir_node *llir_node_new_block(struct ir_block *ir_block);
struct llir_node *llir_node_new_instructions(struct ir_statement *ir_statement);
void llir_node_free(struct llir_node *node);

struct llir_import {
	char *identifier;
};

struct llir_import *llir_import_new(struct ir_method *ir_method);
void llir_import_free(struct llir_import *import);

struct llir_field {
	char *identifier;
	bool array;
	GArray *values;
};

struct llir_field *llir_field_new(struct ir_field *ir_field);
void llir_field_free(struct llir_field *field);

struct llir_method {
	char *identifier;
	GArray *arguments;
};

struct llir_method *llir_method_new(struct ir_method *ir_method);
void llir_method_free(struct llir_method *method);

struct llir_assignment {
	char *destination;
	char *source;
};

struct llir_assignment *llir_assignment_new(char *destination, char *source);
void llir_assignment_free(struct llir_assignment *assignment);

struct llir_literal_assignment {
	char *destination;
	int64_t literal;
};

struct llir_literal_assignment *llir_literal_assignment_new(char *destination,
							    int64_t literal);
void llir_literal_assignment_free(
	struct llir_literal_assignment *literal_assignment);

struct llir_indexed_assignment {
	char *destination;
	char *index;
	char *source;
};

struct llir_indexed_assignment *
llir_indexed_assignment_new(char *destination, char *index, char *source);
void llir_indexed_assignment_free(
	struct llir_indexed_assignment *indexed_assignment);

struct llir_binary_operation {
	char *destination;

	enum llir_binary_operation_type {
		LLIR_BINARY_OPERATION_TYPE_OR,
		LLIR_BINARY_OPERATION_TYPE_AND,
		LLIR_BINARY_OPERATION_TYPE_EQUAL,
		LLIR_BINARY_OPERATION_TYPE_NOT_EQUAL,
		LLIR_BINARY_OPERATION_TYPE_LESS,
		LLIR_BINARY_OPERATION_TYPE_LESS_EQUAL,
		LLIR_BINARY_OPERATION_TYPE_GREATER_EQUAL,
		LLIR_BINARY_OPERATION_TYPE_GREATER,
		LLIR_BINARY_OPERATION_TYPE_ADD,
		LLIR_BINARY_OPERATION_TYPE_SUB,
		LLIR_BINARY_OPERATION_TYPE_MUL,
		LLIR_BINARY_OPERATION_TYPE_DIV,
		LLIR_BINARY_OPERATION_TYPE_MOD,
	} operation;

	char *left_operand;
	char *right_operand;
};

struct llir_binary_operation *
llir_binary_operation_new(char *destination,
			  enum llir_binary_operation_type operation, char *left,
			  char *right);
void llir_binary_operation_free(struct llir_binary_operation *binary_operation);

struct llir_unary_operation {
	char *destination;

	enum llir_unary_operation_type {
		LLIR_UNARY_OPERATION_TYPE_NEGATE,
		LLIR_UNARY_OPERATION_TYPE_NOT,
	} operation;

	char *source;
};

struct llir_unary_operation *
llir_unary_operation_new(char *destination,
			 enum llir_unary_operation_type operation,
			 char *source);
void llir_unary_operation_free(struct llir_unary_operation *unary_operation);

struct llir_method_call {
	char *destination;
	char *identifier;
	GArray *arguments;
};

struct llir_method_call_argument {
	enum llir_method_call_argument_type {
		LLIR_METHOD_CALL_ARGUMENT_TYPE_IDENTIFIER,
		LLIR_METHOD_CALL_ARGUMENT_TYPE_STRING,
	} type;

	union {
		char *identifier;
		char *string;
	};
};

struct llir_method_call *
llir_method_call_new(char *destination, char *indetifier, GArray *arguments);
void llir_method_call_free(struct llir_method_call *method_call);

struct llir_array_index {
	char *destination;
	char *source;
	char *index;
};

struct llir_array_index *llir_array_index_new(char *destination, char *source,
					      char *index);
void llir_array_index_free(struct llir_array_index *array_index);

struct llir_branch {
	char *condition;
	struct llir_node *branch;
};

struct llir_branch *llir_branch_new(char *condition);
void llir_branch_free(struct llir_branch *branch);

struct llir_jump {
	struct llir_node *branch;
};

struct llir_jump *llir_jump_new(struct llir_node *branch);
void llir_jump_free(struct llir_jump *jump);
