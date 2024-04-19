#include "assembly/llir.h"

struct llir *llir_new(void)
{
	struct llir *llir = g_new(struct llir, 1);

	llir->fields = g_array_new(false, false, sizeof(struct llir_field *));
	llir->methods = g_array_new(false, false, sizeof(struct llir_method *));

	return llir;
}

void llir_add_field(struct llir *llir, struct llir_field *field)
{
	g_array_append_val(llir->fields, field);
}

void llir_add_method(struct llir *llir, struct llir_method *method)
{
	g_array_append_val(llir->methods, method);
}

void llir_print(struct llir *llir)
{
	for (uint32_t i = 0; i < llir->methods->len; i++) {
		struct llir_method *method =
			g_array_index(llir->methods, struct llir_method *, i);
		llir_method_print(method);
	}
}

void llir_free(struct llir *llir)
{
	for (uint32_t i = 0; i < llir->fields->len; i++) {
		struct llir_field *field =
			g_array_index(llir->fields, struct llir_field *, i);
		llir_field_free(field);
	}
	g_array_free(llir->fields, true);

	for (uint32_t i = 0; i < llir->methods->len; i++) {
		struct llir_method *method =
			g_array_index(llir->methods, struct llir_method *, i);
		llir_method_free(method);
	}
	g_array_free(llir->methods, true);

	g_free(llir);
}

struct llir_method *llir_method_new(char *identifier)
{
	struct llir_method *method = g_new(struct llir_method, 1);

	method->identifier = g_strdup(identifier);
	method->arguments =
		g_array_new(false, false, sizeof(struct llir_field *));
	method->blocks = g_array_new(false, false, sizeof(struct llir_block *));

	return method;
}

void llir_method_add_argument(struct llir_method *method,
			      struct llir_field *argument)
{
	g_array_append_val(method->arguments, argument);
}

void llir_method_add_block(struct llir_method *method, struct llir_block *block)
{
	g_array_append_val(method->blocks, block);
}

void llir_method_print(struct llir_method *method)
{
	g_print("method %s:\n", method->identifier);

	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);
		llir_block_print(block);
	}
}

void llir_method_free(struct llir_method *method)
{
	for (uint32_t i = 0; i < method->arguments->len; i++) {
		struct llir_field *argument = g_array_index(
			method->arguments, struct llir_field *, i);
		llir_field_free(argument);
	}
	g_array_free(method->arguments, true);

	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);
		llir_block_free(block);
	}
	g_array_free(method->blocks, true);

	g_free(method->identifier);
	g_free(method);
}

struct llir_block *llir_block_new(uint32_t id)
{
	struct llir_block *block = g_new(struct llir_block, 1);

	block->fields = g_array_new(false, false, sizeof(struct llir_field *));
	block->assignments =
		g_array_new(false, false, sizeof(struct llir_assignment *));
	block->terminal_type = LLIR_BLOCK_TERMINAL_TYPE_UNKNOWN;
	block->terminal = NULL;
	block->id = id;
	block->predecessor_count = 0;

	return block;
}

void llir_block_add_field(struct llir_block *block, struct llir_field *field)
{
	g_array_append_val(block->fields, field);
}

void llir_block_add_assignment(struct llir_block *block,
			       struct llir_assignment *assignment)
{
	g_array_append_val(block->assignments, assignment);
}

void llir_block_prepend_assignment(struct llir_block *block,
				   struct llir_assignment *assignment)
{
	g_array_prepend_val(block->assignments, assignment);
}

void llir_block_set_terminal(struct llir_block *block,
			     enum llir_block_terminal_type type, void *terminal)
{
	block->terminal_type = type;
	block->terminal = terminal;

	if (type == LLIR_BLOCK_TERMINAL_TYPE_JUMP) {
		block->jump->block->predecessor_count++;
	} else if (type == LLIR_BLOCK_TERMINAL_TYPE_BRANCH) {
		block->branch->true_block->predecessor_count++;
		block->branch->false_block->predecessor_count++;
	}
}

void llir_block_print(struct llir_block *block)
{
	g_print("\tblock %u:\n", block->id);

	for (uint32_t i = 0; i < block->assignments->len; i++) {
		struct llir_assignment *assignment = g_array_index(
			block->assignments, struct llir_assignment *, i);
		llir_assignment_print(assignment);
	}

	switch (block->terminal_type) {
	case LLIR_BLOCK_TERMINAL_TYPE_JUMP:
		llir_jump_print(block->jump);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_BRANCH:
		llir_branch_print(block->branch);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_RETURN:
		llir_return_print(block->llir_return);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_SHIT_YOURSELF:
		llir_shit_yourself_print(block->shit_yourself);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
}

void llir_block_free(struct llir_block *block)
{
	for (uint32_t i = 0; i < block->fields->len; i++) {
		struct llir_field *field =
			g_array_index(block->fields, struct llir_field *, i);
		llir_field_free(field);
	}
	g_array_free(block->fields, true);

	for (uint32_t i = 0; i < block->assignments->len; i++) {
		struct llir_assignment *assignment = g_array_index(
			block->assignments, struct llir_assignment *, i);
		llir_assignment_free(assignment);
	}
	g_array_free(block->assignments, true);

	switch (block->terminal_type) {
	case LLIR_BLOCK_TERMINAL_TYPE_UNKNOWN:
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_JUMP:
		llir_jump_free(block->jump);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_BRANCH:
		llir_branch_free(block->branch);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_RETURN:
		llir_return_free(block->llir_return);
		break;
	case LLIR_BLOCK_TERMINAL_TYPE_SHIT_YOURSELF:
		llir_shit_yourself_free(block->shit_yourself);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}

	g_free(block);
}

struct llir_field *llir_field_new(char *identifier, uint32_t scope_level,
				  bool is_array, int64_t length)
{
	struct llir_field *field = g_new(struct llir_field, 1);

	if (scope_level == 0)
		field->identifier = g_strdup(identifier);
	else
		field->identifier =
			g_strdup_printf("%s@%u", identifier, scope_level);
	field->is_array = is_array;
	field->values = g_new0(int64_t, length);
	field->value_count = length;

	return field;
}

void llir_field_free(struct llir_field *field)
{
	g_free(field->identifier);
	g_free(field->values);
	g_free(field);
}

struct llir_operand llir_operand_from_field(char *field)
{
	return (struct llir_operand){
		.type = LLIR_OPERAND_TYPE_FIELD,
		.field = field,
	};
}

struct llir_operand llir_operand_from_literal(int64_t literal)
{
	return (struct llir_operand){
		.type = LLIR_OPERAND_TYPE_LITERAL,
		.literal = literal,
	};
}

struct llir_operand llir_operand_from_string(char *string)
{
	return (struct llir_operand){
		.type = LLIR_OPERAND_TYPE_STRING,
		.string = string,
	};
}

void llir_operand_print(struct llir_operand operand)
{
	switch (operand.type) {
	case LLIR_OPERAND_TYPE_FIELD:
		g_print("%s", operand.field);
		break;
	case LLIR_OPERAND_TYPE_LITERAL:
		g_print("%lld", operand.literal);
		break;
	case LLIR_OPERAND_TYPE_STRING:
		g_print("%s", operand.string);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
}

struct llir_assignment *
llir_assignment_new_unary(enum llir_assignment_type type,
			  struct llir_operand source, char *destination)
{
	struct llir_assignment *assignment = g_new(struct llir_assignment, 1);

	assignment->type = type;
	assignment->destination = destination;
	assignment->source = source;

	return assignment;
}

struct llir_assignment *
llir_assignment_new_binary(enum llir_assignment_type type,
			   struct llir_operand left, struct llir_operand right,
			   char *destination)
{
	struct llir_assignment *assignment = g_new(struct llir_assignment, 1);

	assignment->type = type;
	assignment->destination = destination;
	assignment->left = left;
	assignment->right = right;

	return assignment;
}

struct llir_assignment *
llir_assignment_new_array_update(struct llir_operand index,
				 struct llir_operand value, char *destination)
{
	struct llir_assignment *assignment = g_new(struct llir_assignment, 1);

	assignment->type = LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE;
	assignment->destination = destination;
	assignment->update_index = index;
	assignment->update_value = value;

	return assignment;
}

struct llir_assignment *
llir_assignment_new_array_access(struct llir_operand index, char *array,
				 char *destination)
{
	struct llir_assignment *assignment = g_new(struct llir_assignment, 1);

	assignment->type = LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS;
	assignment->destination = destination;
	assignment->access_index = index;
	assignment->access_array = array;

	return assignment;
}

struct llir_assignment *llir_assignment_new_method_call(char *method,
							uint32_t argument_count,
							char *destination)
{
	struct llir_assignment *assignment = g_new(struct llir_assignment, 1);

	assignment->type = LLIR_ASSIGNMENT_TYPE_METHOD_CALL;
	assignment->destination = destination;
	assignment->method = method;
	assignment->argument_count = argument_count;
	assignment->arguments = g_new(struct llir_operand, argument_count);

	return assignment;
}

struct llir_assignment *llir_assignment_new_phi(char *destination)
{
	struct llir_assignment *assignment = g_new(struct llir_assignment, 1);

	assignment->type = LLIR_ASSIGNMENT_TYPE_PHI;
	assignment->destination = destination;
	assignment->phi_arguments =
		g_array_new(false, false, sizeof(struct llir_operand));

	return assignment;
}

void llir_assignment_add_phi_argument(struct llir_assignment *assignment,
				      struct llir_operand argument)
{
	g_array_append_val(assignment->phi_arguments, argument);
}

void llir_assignment_print(struct llir_assignment *assignment)
{
	static const char *BINARY_OPERATOR_TO_STRING[] = {
		[LLIR_ASSIGNMENT_TYPE_EQUAL] = "==",
		[LLIR_ASSIGNMENT_TYPE_NOT_EQUAL] = "!=",
		[LLIR_ASSIGNMENT_TYPE_LESS] = "<",
		[LLIR_ASSIGNMENT_TYPE_LESS_EQUAL] = "<=",
		[LLIR_ASSIGNMENT_TYPE_GREATER_EQUAL] = ">=",
		[LLIR_ASSIGNMENT_TYPE_GREATER] = ">",
		[LLIR_ASSIGNMENT_TYPE_ADD] = "+",
		[LLIR_ASSIGNMENT_TYPE_SUBTRACT] = "-",
		[LLIR_ASSIGNMENT_TYPE_MULTIPLY] = "*",
		[LLIR_ASSIGNMENT_TYPE_DIVIDE] = "/",
		[LLIR_ASSIGNMENT_TYPE_MODULO] = "%",
	};

	static const char *UNARY_OPERATOR_TO_STRING[] = {
		[LLIR_ASSIGNMENT_TYPE_NOT] = "!",
		[LLIR_ASSIGNMENT_TYPE_NEGATE] = "-",
	};

	g_print("\t\t%s", assignment->destination);
	if (assignment->type == LLIR_ASSIGNMENT_TYPE_MOVE) {
		g_print(" = ");
		llir_operand_print(assignment->source);
	} else if (assignment->type == LLIR_ASSIGNMENT_TYPE_NOT ||
		   assignment->type == LLIR_ASSIGNMENT_TYPE_NEGATE) {
		g_print(" = %s", UNARY_OPERATOR_TO_STRING[assignment->type]);
		llir_operand_print(assignment->source);
	} else if (assignment->type == LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE) {
		g_print("[");
		llir_operand_print(assignment->update_index);
		g_print("] = ");
		llir_operand_print(assignment->update_value);
	} else if (assignment->type == LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS) {
		g_print(" = %s[", assignment->access_array);
		llir_operand_print(assignment->access_index);
		g_print("]");
	} else if (assignment->type == LLIR_ASSIGNMENT_TYPE_METHOD_CALL) {
		g_print(" = %s(", assignment->method);
		for (uint32_t i = 0; i < assignment->argument_count; i++) {
			struct llir_operand operand = assignment->arguments[i];
			llir_operand_print(operand);
			if (i != assignment->argument_count - 1)
				g_print(", ");
		}
		g_print(")");
	} else if (assignment->type == LLIR_ASSIGNMENT_TYPE_PHI) {
		g_print(" = ^(");
		for (uint32_t i = 0; i < assignment->phi_arguments->len; i++) {
			struct llir_operand operand =
				g_array_index(assignment->phi_arguments,
					      struct llir_operand, i);
			llir_operand_print(operand);
			if (i != assignment->phi_arguments->len - 1)
				g_print(", ");
		}
		g_print(")");
	} else {
		g_print(" = ");
		llir_operand_print(assignment->left);
		g_print(" %s ", BINARY_OPERATOR_TO_STRING[assignment->type]);
		llir_operand_print(assignment->right);
	}
	g_print("\n");
}

void llir_assignment_free(struct llir_assignment *assignment)
{
	if (assignment->type == LLIR_ASSIGNMENT_TYPE_METHOD_CALL)
		g_free(assignment->arguments);
	else if (assignment->type == LLIR_ASSIGNMENT_TYPE_PHI)
		g_array_free(assignment->phi_arguments, true);

	g_free(assignment);
}

struct llir_branch *
llir_branch_new(enum llir_branch_type type, bool unsigned_comparison,
		struct llir_operand left, struct llir_operand right,
		struct llir_block *true_block, struct llir_block *false_block)
{
	struct llir_branch *branch = g_new(struct llir_branch, 1);

	branch->type = type;
	branch->unsigned_comparison = unsigned_comparison;
	branch->left = left;
	branch->right = right;
	branch->true_block = true_block;
	branch->false_block = false_block;

	return branch;
}

void llir_branch_print(struct llir_branch *branch)
{
	static const char *BRANCH_TYPE_TO_STRING[] = {
		[LLIR_BRANCH_TYPE_EQUAL] = "==",
		[LLIR_BRANCH_TYPE_NOT_EQUAL] = "!=",
		[LLIR_BRANCH_TYPE_LESS] = "<",
		[LLIR_BRANCH_TYPE_LESS_EQUAL] = "<=",
		[LLIR_BRANCH_TYPE_GREATER] = ">",
		[LLIR_BRANCH_TYPE_GREATER_EQUAL] = ">=",
	};

	g_print("\t\tbranch ");
	llir_operand_print(branch->left);
	g_print(" %s ", BRANCH_TYPE_TO_STRING[branch->type]);
	llir_operand_print(branch->right);
	g_print(" block %u\n", branch->false_block->id);
}

void llir_branch_free(struct llir_branch *branch)
{
	g_free(branch);
}

struct llir_jump *llir_jump_new(struct llir_block *block)
{
	struct llir_jump *jump = g_new(struct llir_jump, 1);

	jump->block = block;

	return jump;
}

void llir_jump_print(struct llir_jump *jump)
{
	g_print("\t\tjump block %u\n", jump->block->id);
}

void llir_jump_free(struct llir_jump *jump)
{
	g_free(jump);
}

struct llir_return *llir_return_new(struct llir_operand source)
{
	struct llir_return *llir_return = g_new(struct llir_return, 1);

	llir_return->source = source;

	return llir_return;
}

void llir_return_print(struct llir_return *llir_return)
{
	g_print("\t\treturn ");
	llir_operand_print(llir_return->source);
	g_print("\n");
}

void llir_return_free(struct llir_return *llir_return)
{
	g_free(llir_return);
}

struct llir_shit_yourself *llir_shit_yourself_new(int64_t return_value)
{
	struct llir_shit_yourself *shit_yourself =
		g_new(struct llir_shit_yourself, 1);

	shit_yourself->return_value = return_value;

	return shit_yourself;
}

void llir_shit_yourself_print(struct llir_shit_yourself *shit_yourself)
{
	g_print("\t\texit %lld\n", shit_yourself->return_value);
}

void llir_shit_yourself_free(struct llir_shit_yourself *shit_yourself)
{
	g_free(shit_yourself);
}
