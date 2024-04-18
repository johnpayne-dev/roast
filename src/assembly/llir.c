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

void llir_node_print(struct llir_node *node)
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
	case LLIR_NODE_TYPE_ASSIGNMENT:
		g_print("\t%s", node->assignment->destination);
		if (node->assignment->type == LLIR_ASSIGNMENT_TYPE_MOVE) {
			g_print(" = ");
			print_operand(node->assignment->source);
		} else if (node->assignment->type == LLIR_ASSIGNMENT_TYPE_NOT ||
			   node->assignment->type ==
				   LLIR_ASSIGNMENT_TYPE_NEGATE) {
			g_print(" = %s",
				UNARY_OPERATOR_TO_STRING[node->assignment->type]);
			print_operand(node->assignment->source);
		} else if (node->assignment->type ==
			   LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE) {
			g_print("[");
			print_operand(node->assignment->update_index);
			g_print("] = ");
			print_operand(node->assignment->update_value);
		} else if (node->assignment->type ==
			   LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS) {
			g_print(" = %s[", node->assignment->access_array);
			print_operand(node->assignment->access_index);
			g_print("]");
		} else if (node->assignment->type ==
			   LLIR_ASSIGNMENT_TYPE_METHOD_CALL) {
			g_print(" = %s(...)", node->assignment->method);
		} else {
			g_print(" = ");
			print_operand(node->assignment->left);
			g_print(" %s ",
				BINARY_OPERATOR_TO_STRING[node->assignment
								  ->type]);
			print_operand(node->assignment->right);
		}
		g_print("\n");
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
	case LLIR_NODE_TYPE_ASSIGNMENT:
		llir_assignment_free(node->assignment);
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

struct llir_field *llir_field_new(char *identifier, bool is_array,
				  int64_t length)
{
	struct llir_field *field = g_new(struct llir_field, 1);

	field->identifier = g_strdup(identifier);
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

struct llir_method *llir_method_new(char *identifier, uint32_t argument_count)
{
	struct llir_method *method = g_new(struct llir_method, 1);

	method->identifier = g_strdup(identifier);
	method->argument_count = argument_count;
	method->arguments = g_new(char *, argument_count);

	return method;
}

void llir_method_free(struct llir_method *method)
{
	g_free(method->identifier);
	g_free(method->arguments);
	g_free(method);
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

void llir_assignment_free(struct llir_assignment *assignment)
{
	if (assignment->type == LLIR_ASSIGNMENT_TYPE_METHOD_CALL)
		g_free(assignment->arguments);

	g_free(assignment);
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
