#pragma once
#include "semantics/ir.h"

struct llir {
	GArray *fields;
	GArray *methods;
};

struct llir_field {
	char *identifier;
	bool is_array;
	int64_t value_count;
	int64_t *values;
};

struct llir_method {
	char *identifier;
	GArray *arguments;
	GArray *blocks;
};

struct llir_block {
	GArray *fields;
	GArray *assignments;

	enum llir_block_terminal_type {
		LLIR_BLOCK_TERMINAL_TYPE_UNKNOWN,
		LLIR_BLOCK_TERMINAL_TYPE_BRANCH,
		LLIR_BLOCK_TERMINAL_TYPE_JUMP,
		LLIR_BLOCK_TERMINAL_TYPE_RETURN,
		LLIR_BLOCK_TERMINAL_TYPE_SHIT_YOURSELF,
	} terminal_type;

	union {
		void *terminal;
		struct llir_branch *branch;
		struct llir_jump *jump;
		struct llir_return *llir_return;
		struct llir_shit_yourself *shit_yourself;
	};

	uint32_t id;
	uint32_t predecessor_count;
};

struct llir_operand {
	enum llir_operand_type {
		LLIR_OPERAND_TYPE_FIELD,
		LLIR_OPERAND_TYPE_LITERAL,
		LLIR_OPERAND_TYPE_STRING,
	} type;

	union {
		char *field;
		int64_t literal;
		char *string;
	};
};

struct llir_assignment {
	enum llir_assignment_type {
		LLIR_ASSIGNMENT_TYPE_MOVE,
		LLIR_ASSIGNMENT_TYPE_ADD,
		LLIR_ASSIGNMENT_TYPE_SUBTRACT,
		LLIR_ASSIGNMENT_TYPE_MULTIPLY,
		LLIR_ASSIGNMENT_TYPE_DIVIDE,
		LLIR_ASSIGNMENT_TYPE_MODULO,
		LLIR_ASSIGNMENT_TYPE_GREATER,
		LLIR_ASSIGNMENT_TYPE_GREATER_EQUAL,
		LLIR_ASSIGNMENT_TYPE_LESS,
		LLIR_ASSIGNMENT_TYPE_LESS_EQUAL,
		LLIR_ASSIGNMENT_TYPE_EQUAL,
		LLIR_ASSIGNMENT_TYPE_NOT_EQUAL,
		LLIR_ASSIGNMENT_TYPE_NEGATE,
		LLIR_ASSIGNMENT_TYPE_NOT,
		LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE,
		LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS,
		LLIR_ASSIGNMENT_TYPE_METHOD_CALL,
		LLIR_ASSIGNMENT_TYPE_PHI,
	} type;

	char *destination;

	union {
		struct llir_operand source;
		struct {
			struct llir_operand left;
			struct llir_operand right;
		};
		struct {
			struct llir_operand update_index;
			struct llir_operand update_value;
		};
		struct {
			struct llir_operand access_index;
			char *access_array;
		};
		struct {
			char *method;
			uint32_t argument_count;
			struct llir_operand *arguments;
		};
		struct {
			GArray *phi_arguments;
			GArray *phi_blocks;
		};
	};
};

struct llir_branch {
	enum llir_branch_type {
		LLIR_BRANCH_TYPE_EQUAL,
		LLIR_BRANCH_TYPE_NOT_EQUAL,
		LLIR_BRANCH_TYPE_LESS,
		LLIR_BRANCH_TYPE_LESS_EQUAL,
		LLIR_BRANCH_TYPE_GREATER,
		LLIR_BRANCH_TYPE_GREATER_EQUAL,
	} type;

	bool unsigned_comparison;

	struct llir_operand left;
	struct llir_operand right;

	struct llir_block *true_block;
	struct llir_block *false_block;
};

struct llir_return {
	struct llir_operand source;
};

struct llir_jump {
	struct llir_block *block;
};

struct llir_shit_yourself {
	int64_t return_value;
};

struct llir *llir_new(void);
void llir_add_field(struct llir *llir, struct llir_field *field);
void llir_add_method(struct llir *llir, struct llir_method *method);
void llir_print(struct llir *llir);
void llir_free(struct llir *llir);

struct llir_method *llir_method_new(char *identifier);
void llir_method_add_argument(struct llir_method *method,
			      struct llir_field *field);
void llir_method_add_block(struct llir_method *method,
			   struct llir_block *block);
void llir_method_print(struct llir_method *method);
void llir_method_free(struct llir_method *method);

struct llir_block *llir_block_new(uint32_t id);
void llir_block_add_field(struct llir_block *block, struct llir_field *field);
void llir_block_prepend_assignment(struct llir_block *block,
				   struct llir_assignment *assignment);
void llir_block_add_assignment(struct llir_block *block,
			       struct llir_assignment *assignment);
void llir_block_set_terminal(struct llir_block *block,
			     enum llir_block_terminal_type type,
			     void *terminal);
void llir_block_print(struct llir_block *block);
void llir_block_free(struct llir_block *block);

struct llir_field *llir_field_new(char *identifier, uint32_t scope_level,
				  bool is_array, int64_t length);
void llir_field_free(struct llir_field *field);

struct llir_operand llir_operand_from_field(char *field);
struct llir_operand llir_operand_from_literal(int64_t literal);
struct llir_operand llir_operand_from_string(char *string);
void llir_operand_print(struct llir_operand operand);

struct llir_assignment *
llir_assignment_new_unary(enum llir_assignment_type type,
			  struct llir_operand source, char *destination);
struct llir_assignment *
llir_assignment_new_binary(enum llir_assignment_type type,
			   struct llir_operand left, struct llir_operand right,
			   char *destination);
struct llir_assignment *
llir_assignment_new_array_update(struct llir_operand index,
				 struct llir_operand value, char *destination);
struct llir_assignment *
llir_assignment_new_array_access(struct llir_operand index, char *array,
				 char *destination);
struct llir_assignment *llir_assignment_new_method_call(char *method,
							uint32_t argument_count,
							char *destination);
struct llir_assignment *llir_assignment_new_phi(char *destination);
void llir_assignment_add_phi_argument(struct llir_assignment *assignment,
				      struct llir_operand argument,
				      struct llir_block *block);
void llir_assignment_print(struct llir_assignment *assignment);
void llir_assignment_free(struct llir_assignment *assignment);

struct llir_branch *
llir_branch_new(enum llir_branch_type type, bool unsigned_comparison,
		struct llir_operand left, struct llir_operand right,
		struct llir_block *true_block, struct llir_block *false_block);
void llir_branch_print(struct llir_branch *branch);
void llir_branch_free(struct llir_branch *branch);

struct llir_jump *llir_jump_new(struct llir_block *block);
void llir_jump_print(struct llir_jump *jump);
void llir_jump_free(struct llir_jump *jump);

struct llir_return *llir_return_new(struct llir_operand source);
void llir_return_print(struct llir_return *llir_return);
void llir_return_free(struct llir_return *llir_return);

struct llir_shit_yourself *llir_shit_yourself_new(int64_t return_value);
void llir_shit_yourself_print(struct llir_shit_yourself *shit_yourself);
void llir_shit_yourself_free(struct llir_shit_yourself *shit_yourself);
