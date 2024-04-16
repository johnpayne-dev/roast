#include "assembly/llir.h"

struct llir_node *llir_node_new(enum llir_node_type type, void *data)
{
	struct llir_node *node = g_new(struct llir_node, 1);
	node->type = type;
	node->data = data;
	node->next = NULL;
	return node;
}

static void print_operand(struct llir_operand operand)
{
	switch (operand.type) {
	case LLIR_OPERAND_TYPE_VARIABLE:
		g_print("%s", operand.identifier);
		break;
	case LLIR_OPERAND_TYPE_DEREFERENCE:
		g_print("%s[%lld]", operand.dereference.identifier,
			operand.dereference.offset / 8);
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

void llir_node_print(struct llir_node *node)
{
	static const char *OPERATION_TYPE_TO_STRING[] = {
		[LLIR_OPERATION_TYPE_MOVE] = "=",
		[LLIR_OPERATION_TYPE_EQUAL] = "==",
		[LLIR_OPERATION_TYPE_NOT_EQUAL] = "!=",
		[LLIR_OPERATION_TYPE_LESS] = "<",
		[LLIR_OPERATION_TYPE_LESS_EQUAL] = "<=",
		[LLIR_OPERATION_TYPE_GREATER_EQUAL] = ">=",
		[LLIR_OPERATION_TYPE_GREATER] = ">",
		[LLIR_OPERATION_TYPE_ADD] = "+",
		[LLIR_OPERATION_TYPE_SUBTRACT] = "-",
		[LLIR_OPERATION_TYPE_MULTIPLY] = "*",
		[LLIR_OPERATION_TYPE_DIVIDE] = "/",
		[LLIR_OPERATION_TYPE_MODULO] = "%",
		[LLIR_OPERATION_TYPE_NEGATE] = "-",
		[LLIR_OPERATION_TYPE_NOT] = "!",
	};

	static const char *BRANCH_TYPE_TO_STRING[] = {
		[LLIR_BRANCH_TYPE_EQUAL] = "==",
		[LLIR_BRANCH_TYPE_NOT_EQUAL] = "!=",
		[LLIR_BRANCH_TYPE_LESS] = "<",
		[LLIR_BRANCH_TYPE_LESS_EQUAL] = "<=",
		[LLIR_BRANCH_TYPE_GREATER] = ">",
		[LLIR_BRANCH_TYPE_GREATER_EQUAL] = ">=",
	};

	switch (node->type) {
	case LLIR_NODE_TYPE_FIELD:
		break;
	case LLIR_NODE_TYPE_METHOD:
		g_print("\nmethod %s\n", node->method->identifier);
		break;
	case LLIR_NODE_TYPE_OPERATION:
		g_print("\t");
		print_operand(node->operation->destination);
		if (node->operation->type == LLIR_OPERATION_TYPE_MOVE) {
			g_print(" = ");
			print_operand(node->operation->source);
		} else if (node->operation->type == LLIR_OPERATION_TYPE_NOT ||
			   node->operation->type ==
				   LLIR_OPERATION_TYPE_NEGATE) {
			g_print(" = ");
			g_print("%s",
				OPERATION_TYPE_TO_STRING[node->operation->type]);
			print_operand(node->operation->source);
		} else if (node->operation->type == LLIR_OPERATION_TYPE_ADD ||
			   node->operation->type ==
				   LLIR_OPERATION_TYPE_SUBTRACT ||
			   node->operation->type ==
				   LLIR_OPERATION_TYPE_MULTIPLY ||
			   node->operation->type ==
				   LLIR_OPERATION_TYPE_DIVIDE ||
			   node->operation->type ==
				   LLIR_OPERATION_TYPE_MODULO) {
			g_print(" %s= ",
				OPERATION_TYPE_TO_STRING[node->operation->type]);
			print_operand(node->operation->source);
		} else {
			g_print(" = ");
			print_operand(node->operation->destination);
			g_print(" %s ",
				OPERATION_TYPE_TO_STRING[node->operation->type]);
			print_operand(node->operation->source);
		}
		g_print("\n");
		break;
	case LLIR_NODE_TYPE_METHOD_CALL:
		g_print("\t");
		print_operand(node->method_call->destination);
		g_print(" = %s(...)\n", node->method_call->identifier);
		break;
	case LLIR_NODE_TYPE_LABEL:
		g_print("%s:\n", node->label->name);
		break;
	case LLIR_NODE_TYPE_BRANCH:
		g_print("\tbranch ");
		print_operand(node->branch->left);
		g_print(" %s ", BRANCH_TYPE_TO_STRING[node->branch->type]);
		print_operand(node->branch->right);
		g_print(" %s\n", node->branch->label->name);
		break;
	case LLIR_NODE_TYPE_JUMP:
		g_print("\tjump %s\n", node->jump->label->name);
		break;
	case LLIR_NODE_TYPE_RETURN:
		g_print("\treturn ");
		print_operand(node->llir_return->source);
		g_print("\n");
		break;
	case LLIR_NODE_TYPE_SHIT_YOURSELF:
		g_print("\texit %lld\n", node->shit_yourself->return_value);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}

	if (node->next != NULL)
		llir_node_print(node->next);
}

void llir_node_free(struct llir_node *node)
{
	switch (node->type) {
	case LLIR_NODE_TYPE_FIELD:
		llir_field_free(node->field);
		break;
	case LLIR_NODE_TYPE_METHOD:
		llir_method_free(node->method);
		break;
	case LLIR_NODE_TYPE_OPERATION:
		llir_operation_free(node->operation);
		break;
	case LLIR_NODE_TYPE_METHOD_CALL:
		llir_method_call_free(node->method_call);
		break;
	case LLIR_NODE_TYPE_LABEL:
		llir_label_free(node->label);
		break;
	case LLIR_NODE_TYPE_BRANCH:
		llir_branch_free(node->branch);
		break;
	case LLIR_NODE_TYPE_JUMP:
		llir_jump_free(node->jump);
		break;
	case LLIR_NODE_TYPE_RETURN:
		llir_return_free(node->llir_return);
		break;
	case LLIR_NODE_TYPE_SHIT_YOURSELF:
		llir_shit_yourself_free(node->shit_yourself);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}

	if (node->next != NULL)
		llir_node_free(node->next);

	g_free(node);
}

struct llir_field *llir_field_new(char *identifier, uint32_t scope_level,
				  bool is_array, int64_t length)
{
	struct llir_field *field = g_new(struct llir_field, 1);

	if (scope_level > 0)
		field->identifier =
			g_strdup_printf("%s@%u", identifier, scope_level);
	else
		field->identifier = g_strdup(identifier);

	field->is_array = is_array;
	field->values = g_new0(int64_t, length);
	field->value_count = length;

	return field;
}

void llir_field_set_value(struct llir_field *field, uint32_t index,
			  int64_t value)
{
	g_assert(index < field->value_count);
	field->values[index] = value;
}

int64_t llir_field_get_value(struct llir_field *field, uint32_t index)
{
	g_assert(index < field->value_count);
	return field->values[index];
}

void llir_field_free(struct llir_field *field)
{
	g_free(field->identifier);
	g_free(field->values);
	g_free(field);
}

struct llir_method *llir_method_new(char *identifier, uint32_t argument_count)
{
	struct llir_method *method = g_new(struct llir_method, 1);

	method->identifier = g_strdup(identifier);
	method->argument_count = argument_count;
	method->arguments = g_new(struct llir_field *, argument_count);

	return method;
}

void llir_method_set_argument(struct llir_method *method, uint32_t index,
			      struct llir_field *field)
{
	g_assert(index < method->argument_count);
	method->arguments[index] = field;
}

struct llir_field *llir_method_get_argument(struct llir_method *method,
					    uint32_t index)
{
	g_assert(index < method->argument_count);
	return method->arguments[index];
}

void llir_method_free(struct llir_method *method)
{
	g_free(method->identifier);
	for (uint32_t i = 0; i < method->argument_count; i++)
		llir_field_free(method->arguments[i]);
	g_free(method->arguments);
	g_free(method);
}

struct llir_operand llir_operand_from_identifier(char *identifier)
{
	return (struct llir_operand){
		.type = LLIR_OPERAND_TYPE_VARIABLE,
		.identifier = identifier,
	};
}

struct llir_operand llir_operand_from_dereference(char *identifier,
						  int64_t offset)
{
	return (struct llir_operand){
		.type = LLIR_OPERAND_TYPE_DEREFERENCE,
		.dereference = {
			.identifier = identifier,
			.offset = offset,
		},
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

struct llir_operation *llir_operation_new(enum llir_operation_type type,
					  struct llir_operand source,
					  struct llir_operand destination)
{
	struct llir_operation *operation = g_new(struct llir_operation, 1);

	operation->type = type;
	operation->source = source;
	operation->destination = destination;

	return operation;
}

void llir_operation_free(struct llir_operation *operation)
{
	g_free(operation);
}

struct llir_method_call *llir_method_call_new(char *method,
					      uint32_t argument_count,
					      struct llir_operand destination)
{
	struct llir_method_call *call = g_new(struct llir_method_call, 1);

	call->identifier = g_strdup(method);
	call->argument_count = argument_count;
	call->arguments = g_new(struct llir_operand, argument_count);
	call->destination = destination;

	return call;
}

void llir_method_call_set_argument(struct llir_method_call *call,
				   uint32_t index, struct llir_operand argument)
{
	g_assert(index < call->argument_count);
	call->arguments[index] = argument;
}

struct llir_operand llir_method_call_get_argument(struct llir_method_call *call,
						  uint32_t index)
{
	g_assert(index < call->argument_count);
	return call->arguments[index];
}

void llir_method_call_free(struct llir_method_call *call)
{
	g_free(call->identifier);
	g_free(call->arguments);
	g_free(call);
}

struct llir_label *llir_label_new(uint32_t id)
{
	struct llir_label *label = g_new(struct llir_label, 1);

	label->name = g_strdup_printf("label_%u", id);

	return label;
}

void llir_label_free(struct llir_label *label)
{
	g_free(label->name);
	g_free(label);
}

struct llir_branch *llir_branch_new(enum llir_branch_type type,
				    bool unsigned_comparison,
				    struct llir_operand left,
				    struct llir_operand right,
				    struct llir_label *label)
{
	struct llir_branch *branch = g_new(struct llir_branch, 1);

	branch->type = type;
	branch->unsigned_comparison = unsigned_comparison;
	branch->left = left;
	branch->right = right;
	branch->label = label;

	return branch;
}

void llir_branch_free(struct llir_branch *branch)
{
	g_free(branch);
}

struct llir_jump *llir_jump_new(struct llir_label *label)
{
	struct llir_jump *jump = g_new(struct llir_jump, 1);

	jump->label = label;

	return jump;
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

void llir_shit_yourself_free(struct llir_shit_yourself *shit_yourself)
{
	g_free(shit_yourself);
}
