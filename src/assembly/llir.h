#pragma once
#include "semantics/ir.h"

struct llir_node {
	enum llir_node_type {
		LLIR_NODE_TYPE_FIELD,
		LLIR_NODE_TYPE_METHOD,
		LLIR_NODE_TYPE_OPERATION,
		LLIR_NODE_TYPE_METHOD_CALL,
		LLIR_NODE_TYPE_LABEL,
		LLIR_NODE_TYPE_BRANCH,
		LLIR_NODE_TYPE_JUMP,
		LLIR_NODE_TYPE_RETURN,
		LLIR_NODE_TYPE_SHIT_YOURSELF,
	} type;

	union {
		void *data;
		struct llir_field *field;
		struct llir_method *method;
		struct llir_operation *operation;
		struct llir_method_call *method_call;
		struct llir_label *label;
		struct llir_branch *branch;
		struct llir_jump *jump;
		struct llir_return *llir_return;
		struct llir_shit_yourself *shit_yourself;
	};

	struct llir_node *next;
};

struct llir_node *llir_node_new(enum llir_node_type type, void *data);
struct llir_node *llir_node_from_program(struct ir_program *program);
void llir_node_print(struct llir_node *node);
void llir_node_free(struct llir_node *node);

struct llir_field {
	char *identifier;
	bool is_array;
	int64_t value_count;
	int64_t *values;
};

struct llir_field *llir_field_new(char *identifier, bool is_array,
				  int64_t length);
void llir_field_set_value(struct llir_field *field, uint32_t index,
			  int64_t value);
int64_t llir_field_get_value(struct llir_field *field, uint32_t index);
void llir_field_free(struct llir_field *field);

struct llir_method {
	char *identifier;
	uint32_t argument_count;
	struct llir_field **arguments;
};

struct llir_method *llir_method_new(char *identifier, uint32_t argument_count);
void llir_method_set_argument(struct llir_method *method, uint32_t index,
			      struct llir_field *field);
struct llir_field *llir_method_get_argument(struct llir_method *method,
					    uint32_t index);
void llir_method_free(struct llir_method *method);

struct llir_operand {
	enum llir_operand_type {
		LLIR_OPERAND_TYPE_VARIABLE,
		LLIR_OPERAND_TYPE_DEREFERENCE,
		LLIR_OPERAND_TYPE_LITERAL,
	} type;

	union {
		char *identifier;
		int64_t literal;
		struct llir_dereference {
			char *identifier;
			int64_t offset;
		} dereference;
	};
};

struct llir_operand llir_operand_variable(char *identifier);
struct llir_operand llir_operand_dereference(char *identifier, int64_t offset);
struct llir_operand llir_operand_literal(int64_t literal);

struct llir_operation {
	enum llir_operation_type {
		LLIR_OPERATION_TYPE_MOVE,
		LLIR_OPERATION_TYPE_ADD,
		LLIR_OPERATION_TYPE_SUBTRACT,
		LLIR_OPERATION_TYPE_MULTIPLY,
		LLIR_OPERATION_TYPE_DIVIDE,
		LLIR_OPERATION_TYPE_MODULO,
		LLIR_OPERATION_TYPE_GREATER,
		LLIR_OPERATION_TYPE_GREATER_EQUAL,
		LLIR_OPERATION_TYPE_LESS,
		LLIR_OPERATION_TYPE_LESS_EQUAL,
		LLIR_OPERATION_TYPE_EQUAL,
		LLIR_OPERATION_TYPE_NOT_EQUAL,
	} type;

	struct llir_operand source;
	struct llir_operand destination;
};

struct llir_operation *llir_operation_new(enum llir_operation_type type,
					  struct llir_operand source,
					  struct llir_operand destination);
void llir_operation_free(struct llir_operation *operation);

struct llir_method_call {
	char *method;
	struct llir_operand *arguments;
	uint32_t argument_count;
	struct llir_operand destination;
};

struct llir_method_call *llir_method_call_new(char *method,
					      uint32_t argument_count,
					      struct llir_operand destination);
void llir_method_call_set_argument(struct llir_method_call *call,
				   uint32_t index,
				   struct llir_operand argument);
struct llir_operand llir_method_call_get_argument(struct llir_method_call *call,
						  uint32_t index);
void llir_method_call_free(struct llir_method_call *call);

struct llir_label {
	char *name;
};

struct llir_label *llir_label_new(uint32_t id);
void llir_label_free(struct llir_label *label);

struct llir_branch {
	enum llir_branch_type {
		LLIR_BRANCH_EQUAL,
		LLIR_BRANCH_NOT_EQUAL,
		LLIR_BRANCH_LESS,
		LLIR_BRANCH_LESS_EQUAL,
		LLIR_BRANCH_GREATER,
		LLIR_BRANCH_GREATER_EQUAL,
	} type;

	struct llir_operand left;
	struct llir_operand right;

	struct llir_label *label;
};

struct llir_branch *llir_branch_new(enum llir_branch_type type,
				    struct llir_operand left,
				    struct llir_operand right,
				    struct llir_label *label);
void llir_branch_free(struct llir_branch *branch);

struct llir_jump {
	struct llir_label *label;
};

struct llir_jump *llir_jump_new(struct llir_label *label);
void llir_jump_free(struct llir_jump *jump);

struct llir_return {
	struct llir_operand source;
};

struct llir_return *llir_return_new(struct llir_operand source);
void llir_return_free(struct llir_return *llir_return);

struct llir_shit_yourself {
	int64_t return_value;
};

struct llir_shit_yourself *llir_shit_yourself_new(int64_t return_value);
void llir_shit_yourself_free(struct llir_shit_yourself *shit_yourself);
