#include "assembly/llir.h"

struct llir_node *llir_node_new(enum llir_node_type type, void *data)
{
	struct llir_node *node = g_new(struct llir_node, 1);
	node->type = type;
	node->next = NULL;

	switch (type) {
	case LLIR_NODE_TYPE_IMPORT:
		node->import = data;
		break;
	case LLIR_NODE_TYPE_FIELD:
		node->field = data;
		break;
	case LLIR_NODE_TYPE_METHOD:
		node->method = data;
		break;
	case LLIR_NODE_TYPE_ASSIGNMENT:
		node->assignment = data;
		break;
	case LLIR_NODE_TYPE_LITERAL_ASSIGNMENT:
		node->literal_assignment = data;
		break;
	case LLIR_NODE_TYPE_INDEXED_ASSIGNMENT:
		node->indexed_assignment = data;
		break;
	case LLIR_NODE_TYPE_INCREMENT:
		node->increment = data;
		break;
	case LLIR_NODE_TYPE_DECREMENT:
		node->decrement = data;
		break;
	case LLIR_NODE_TYPE_BINARY_OPERATION:
		node->binary_operation = data;
		break;
	case LLIR_NODE_TYPE_UNARY_OPERATION:
		node->unary_operation = data;
		break;
	case LLIR_NODE_TYPE_METHOD_CALL:
		node->method_call = data;
		break;
	case LLIR_NODE_TYPE_ARRAY_INDEX:
		node->array_index = data;
		break;
	case LLIR_NODE_TYPE_BRANCH:
		node->branch = data;
		break;
	default:
		node->import = NULL;
		break;
	}

	return node;
}

static void append_nodes(struct llir_node **node, struct llir_node *appended)
{
	while ((*node)->next != NULL)
		*node = (*node)->next;

	(*node)->next = appended;

	while ((*node)->next != NULL)
		*node = (*node)->next;
}

struct llir_node *llir_node_new_program(struct ir_program *ir_program)
{
	struct llir_node *program = llir_node_new(LLIR_NODE_TYPE_PROGRAM, NULL);
	struct llir_node *node = program;

	for (uint32_t i = 0; i < ir_program->imports->len; i++) {
		struct ir_method *ir_method = g_array_index(
			ir_program->imports, struct ir_method *, i);
		append_nodes(&node, llir_node_new_import(ir_method));
	}

	for (uint32_t i = 0; i < ir_program->fields->len; i++) {
		struct ir_field *ir_field =
			g_array_index(ir_program->fields, struct ir_field *, i);
		append_nodes(&node, llir_node_new_field(ir_field));
	}

	for (uint32_t i = 0; i < ir_program->methods->len; i++) {
		struct ir_method *ir_method = g_array_index(
			ir_program->methods, struct ir_method *, i);
		append_nodes(&node, llir_node_new_method(ir_method));
	}

	append_nodes(&node, llir_node_new(LLIR_NODE_TYPE_PROGRAM_END, NULL));
	return program;
}

struct llir_node *llir_node_new_import(struct ir_method *ir_method)
{
	struct llir_import *import = llir_import_new(ir_method);
	return llir_node_new(LLIR_NODE_TYPE_IMPORT, import);
}

struct llir_node *llir_node_new_method(struct ir_method *ir_method)
{
	g_assert(ir_method->imported == false);

	struct llir_node *node = llir_node_new(LLIR_NODE_TYPE_METHOD,
					       llir_method_new(ir_method));

	node->next = llir_node_new_block(ir_method->block);

	return node;
}

struct llir_node *llir_node_new_field(struct ir_field *ir_field)
{
	struct llir_field *field = llir_field_new(ir_field);
	struct llir_node *node = llir_node_new(LLIR_NODE_TYPE_FIELD, field);
	return node;
}

struct llir_node *llir_node_new_block(struct ir_block *ir_block)
{
	struct llir_node *head_node =
		llir_node_new(LLIR_NODE_TYPE_BLOCK_START, NULL);

	struct llir_node *node = head_node;

	for (uint32_t i = 0; i < ir_block->fields->len; i++) {
		append_nodes(&node,
			     llir_node_new_field(g_array_index(
				     ir_block->fields, struct ir_field *, i)));
	}

	for (uint32_t i = 0; i < ir_block->statements->len; i++) {
		append_nodes(&node, llir_node_new_instructions(g_array_index(
					    ir_block->statements,
					    struct ir_statement *, i)));
	}

	append_nodes(&node, llir_node_new(LLIR_NODE_TYPE_BLOCK_END, NULL));

	return head_node;
}

static struct llir_node *
nodes_from_expression(struct ir_expression *ir_expression);

static const enum llir_binary_operation_type IR_TO_LLIR_BIN_OPS[] = {
	[IR_BINARY_OPERATOR_OR] = LLIR_BINARY_OPERATION_TYPE_OR,
	[IR_BINARY_OPERATOR_AND] = LLIR_BINARY_OPERATION_TYPE_AND,
	[IR_BINARY_OPERATOR_EQUAL] = LLIR_BINARY_OPERATION_TYPE_EQUAL,
	[IR_BINARY_OPERATOR_NOT_EQUAL] = LLIR_BINARY_OPERATION_TYPE_NOT_EQUAL,
	[IR_BINARY_OPERATOR_LESS] = LLIR_BINARY_OPERATION_TYPE_LESS,
	[IR_BINARY_OPERATOR_LESS_EQUAL] = LLIR_BINARY_OPERATION_TYPE_LESS_EQUAL,
	[IR_BINARY_OPERATOR_GREATER_EQUAL] =
		LLIR_BINARY_OPERATION_TYPE_GREATER_EQUAL,
	[IR_BINARY_OPERATOR_GREATER] = LLIR_BINARY_OPERATION_TYPE_GREATER,
	[IR_BINARY_OPERATOR_ADD] = LLIR_BINARY_OPERATION_TYPE_ADD,
	[IR_BINARY_OPERATOR_SUB] = LLIR_BINARY_OPERATION_TYPE_SUB,
	[IR_BINARY_OPERATOR_MUL] = LLIR_BINARY_OPERATION_TYPE_MUL,
	[IR_BINARY_OPERATOR_DIV] = LLIR_BINARY_OPERATION_TYPE_DIV,
	[IR_BINARY_OPERATOR_MOD] = LLIR_BINARY_OPERATION_TYPE_MOD,
};

static struct llir_node *
nodes_from_binary_expression(struct ir_binary_expression *binary_expression)
{
	struct llir_node *head_node =
		nodes_from_expression(binary_expression->left);
	char *left_operand = last_temporary_variable();

	struct llir_node *node = head_node;

	append_nodes(&node, nodes_from_expression(binary_expression->right));
	char *right_operand = last_temporary_variable();

	char *destination = next_temporary_variable();

	struct llir_node *binary_operation_node = llir_node_new(
		LLIR_NODE_TYPE_BINARY_OPERATION,
		llir_binary_operation_new(
			destination,
			IR_TO_LLIR_BIN_OPS[binary_expression->binary_operator],
			left_operand, right_operand));

	g_free(left_operand);
	g_free(right_operand);
	g_free(destination);

	node->next = binary_operation_node;

	return head_node;
}

static struct llir_node *
nodes_from_not_expression(struct ir_expression *not_expression)
{
	g_assert(not_expression->type == IR_EXPRESSION_TYPE_NOT);

	struct llir_node *head_node =
		nodes_from_expression(not_expression->not_expression);
	char *source = last_temporary_variable();

	struct llir_node *node = head_node;

	char *destination = next_temporary_variable();

	struct llir_node *not_node = llir_node_new(
		LLIR_NODE_TYPE_UNARY_OPERATION,
		llir_unary_operation_new(
			destination, LLIR_UNARY_OPERATION_TYPE_NOT, source));

	g_free(source);
	g_free(destination);

	node->next = not_node;

	return head_node;
}

static struct llir_node *
nodes_from_negate_expression(struct ir_expression *negate_expression)
{
	g_assert(negate_expression->type == LLIR_UNARY_OPERATION_TYPE_NEGATE);

	struct llir_node *head_node =
		nodes_from_expression(negate_expression->negate_expression);
	char *source = last_temporary_variable();

	struct llir_node *node = head_node;

	char *destination = next_temporary_variable();

	struct llir_node *negate_node = llir_node_new(
		LLIR_NODE_TYPE_UNARY_OPERATION,
		llir_unary_operation_new(
			destination, LLIR_UNARY_OPERATION_TYPE_NEGATE, source));

	g_free(source);
	g_free(destination);

	node->next = negate_node;

	return head_node;
}

static struct llir_node *
nodes_from_len_identifier(struct ir_length_expression *length_expression)
{
	char *destination = next_temporary_variable();

	struct llir_node *length_node =
		llir_node_new(LLIR_NODE_TYPE_LITERAL_ASSIGNMENT,
			      llir_literal_assignment_new(
				      destination, length_expression->length));

	g_free(destination);

	return length_node;
}

static struct llir_node *
nodes_from_method_call(struct ir_method_call *method_call)
{
	struct llir_node *head_node = NULL, *node = NULL;

	GArray *arguments = g_array_new(
		false, false, sizeof(struct llir_method_call_argument));

	for (uint32_t i = 0; i < method_call->arguments->len; i++) {
		struct ir_method_call_argument *ir_method_call_argument =
			g_array_index(method_call->arguments,
				      struct ir_method_call_argument *, i);

		struct llir_method_call_argument method_call_argument;

		switch (ir_method_call_argument->type) {
		case IR_METHOD_CALL_ARGUMENT_TYPE_STRING:
			method_call_argument.type =
				LLIR_METHOD_CALL_ARGUMENT_TYPE_STRING;
			method_call_argument.string =
				g_strdup(ir_method_call_argument->string);
			break;
		case IR_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION:
			method_call_argument.type =
				LLIR_METHOD_CALL_ARGUMENT_TYPE_IDENTIFIER;

			if (head_node == NULL) {
				head_node = nodes_from_expression(
					ir_method_call_argument->expression);
				node = head_node;
			} else {
				append_nodes(&node,
					     nodes_from_expression(
						     ir_method_call_argument
							     ->expression));
			}

			method_call_argument.identifier =
				last_temporary_variable();
			break;
		default:
			g_assert(!"you fucked up");
			break;
		}
	}

	char *destination = next_temporary_variable();

	struct llir_node *method_call_node = llir_node_new(
		LLIR_NODE_TYPE_METHOD_CALL,
		llir_method_call_new(destination, method_call->identifier,
				     arguments));

	g_free(destination);

	if (head_node == NULL) {
		head_node = method_call_node;
	} else {
		append_nodes(&node, method_call_node);
	}

	return head_node;
}

static struct llir_node *nodes_from_literal(struct ir_literal *literal)
{
	char *destination = next_temporary_variable();

	struct llir_node *literal_node = llir_node_new(
		LLIR_NODE_TYPE_LITERAL_ASSIGNMENT,
		llir_literal_assignment_new(
			destination,
			literal->value)); // TODO: NEGATIVE VALUES OR SMTH SHIT

	g_free(destination);

	return literal_node;
}

static struct llir_node *nodes_from_location(struct ir_location *location)
{
	struct llir_node *head_node = nodes_from_expression(location->index);

	char *index = last_temporary_variable();

	char *destination = next_temporary_variable();

	struct llir_node *location_node = llir_node_new(
		LLIR_NODE_TYPE_ARRAY_INDEX,
		llir_array_index_new(destination, location->identifier, index));

	g_free(index);
	g_free(destination);

	return location_node;
}

static struct llir_node *
nodes_from_expression(struct ir_expression *ir_expression)
{
	struct llir_node *node = NULL;

	switch (ir_expression->type) {
	case IR_EXPRESSION_TYPE_BINARY:
		node = nodes_from_binary_expression(
			ir_expression->binary_expression);
		break;
	case IR_EXPRESSION_TYPE_NOT:
		node = nodes_from_not_expression(ir_expression->not_expression);
		break;
	case IR_EXPRESSION_TYPE_NEGATE:
		node = nodes_from_negate_expression(
			ir_expression->negate_expression);
		break;
	case IR_EXPRESSION_TYPE_LEN:
		node = nodes_from_len_identifier(
			ir_expression->length_expression);
		break;
	case IR_EXPRESSION_TYPE_METHOD_CALL:
		node = nodes_from_method_call(ir_expression->method_call);
		break;
	case IR_EXPRESSION_TYPE_LITERAL:
		node = nodes_from_literal(ir_expression->literal);
		break;
	case IR_EXPRESSION_TYPE_LOCATION:
		node = nodes_from_location(ir_expression->location);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}

	return node;
}

static struct llir_node *
nodes_from_assignment(struct ir_assignment *ir_assignment)
{
	g_assert(!"TODO");
	return NULL;
}

static struct llir_node *
nodes_from_method_call_statement(struct ir_method_call *ir_method_call)
{
	g_assert(!"TODO");
	return NULL;
}

static struct llir_node *
nodes_from_if_statement(struct ir_if_statement *ir_if_statement)
{
	g_assert(!"TODO");
	return NULL;
}

static struct llir_node *
nodes_from_for_statement(struct ir_for_statement *ir_for_statement)
{
	g_assert(!"TODO");
	return NULL;
}

static struct llir_node *
nodes_from_while_statement(struct ir_while_statement *ir_while_statement)
{
	g_assert(!"TODO");
	return NULL;
}

static struct llir_node *nodes_from_break_statement(void)
{
	g_assert(!"TODO");
	return NULL;
}

static struct llir_node *nodes_from_continue_statement(void)
{
	g_assert(!"TODO");
	return NULL;
}

struct llir_node *llir_node_new_instructions(struct ir_statement *ir_statement)
{
	struct llir_node *node = NULL;

	switch (ir_statement->type) {
	case IR_STATEMENT_TYPE_ASSIGNMENT:
		node = nodes_from_assignment(ir_statement->assignment);
		break;
	case IR_STATEMENT_TYPE_METHOD_CALL:
		node = nodes_from_method_call(ir_statement->method_call);
		break;
	case IR_STATEMENT_TYPE_IF:
		node = nodes_from_if_statement(ir_statement->if_statement);
		break;
	case IR_STATEMENT_TYPE_FOR:
		node = nodes_from_for_statement(ir_statement->for_statement);
		break;
	case IR_STATEMENT_TYPE_WHILE:
		node = nodes_from_while_statement(
			ir_statement->while_statement);
		break;
	case IR_STATEMENT_TYPE_RETURN:
		node = nodes_from_expression(ir_statement->return_expression);
		break;
	case IR_STATEMENT_TYPE_BREAK:
		node = nodes_from_break_statement();
		break;
	case IR_STATEMENT_TYPE_CONTINUE:
		node = nodes_from_continue_statement();
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}

	return node;
}

void llir_node_free(struct llir_node *node)
{
	g_assert(!"TODO");
}

struct llir_import *llir_import_new(struct ir_method *ir_method)
{
	g_assert(ir_method->imported);

	struct llir_import *import = g_new(struct llir_import, 1);
	import->identifier = g_strdup(ir_method->identifier);
	return import;
}

void llir_import_free(struct llir_import *import)
{
	g_free(import->identifier);
	g_free(import);
}

struct llir_field *llir_field_new(struct ir_field *ir_field)
{
	struct llir_field *field = g_new(struct llir_field, 1);

	field->identifier = g_strdup(ir_field->identifier);
	field->array = ir_data_type_is_array(ir_field->type);
	field->values = g_array_new(false, false, sizeof(int64_t));

	for (int64_t i = 0; i < ir_field->array_length; i++) {
		int64_t value = 0;
		if (ir_field->initializer != NULL) {
			struct ir_literal *literal =
				g_array_index(ir_field->initializer->literals,
					      struct ir_literal *, i);
			value = literal->value;
		}

		g_array_append_val(field->values, value);
	}

	return field;
}

void llir_field_free(struct llir_field *field)
{
	g_array_free(field->values, true);
	g_free(field);
}

struct llir_method *llir_method_new(struct ir_method *ir_method)
{
	struct llir_method *method = g_new(struct llir_method, 1);

	method->identifier = g_strdup(ir_method->identifier);

	method->arguments =
		g_array_new(false, false, sizeof(struct llir_field *));

	for (uint32_t i = 0; i < ir_method->arguments->len; i++) {
		struct ir_field *ir_field = g_array_index(ir_method->arguments,
							  struct ir_field *, i);
		struct llir_field *field = llir_field_new(ir_field);
		g_array_append_val(method->arguments, field);
	}

	return method;
}

void llir_method_free(struct llir_method *method)
{
	g_free(method->identifier);

	for (uint32_t i = 0; i < method->arguments->len; i++) {
		struct llir_field *field = g_array_index(
			method->arguments, struct llir_field *, i);
		llir_field_free(field);
	}
	g_array_free(method->arguments, true);

	g_free(method);
}

struct llir_assignment *llir_assignment_new(char *destination, char *source)
{
	struct llir_assignment *assignment = g_new(struct llir_assignment, 1);
	assignment->destination = g_strdup(destination);
	assignment->source = g_strdup(source);
	return assignment;
}

void llir_assignment_free(struct llir_assignment *assignment)
{
	g_free(assignment->destination);
	g_free(assignment->source);
	g_free(assignment);
}

struct llir_literal_assignment *llir_literal_assignment_new(char *destination,
							    int64_t literal)
{
	struct llir_literal_assignment *assignment =
		g_new(struct llir_literal_assignment, 1);
	assignment->destination = g_strdup(destination);
	assignment->literal = literal;
	return assignment;
}

void llir_literal_assignment_free(
	struct llir_literal_assignment *literal_assignment)
{
	g_free(literal_assignment->destination);
	g_free(literal_assignment);
}

struct llir_indexed_assignment *
llir_indexed_assignment_new(char *destination, char *index, char *source)
{
	struct llir_indexed_assignment *assignment =
		g_new(struct llir_indexed_assignment, 1);
	assignment->destination = g_strdup(destination);
	assignment->index = g_strdup(index);
	assignment->source = g_strdup(source);
	return assignment;
}

void llir_indexed_assignment_free(
	struct llir_indexed_assignment *indexed_assignment)
{
	g_free(indexed_assignment->destination);
	g_free(indexed_assignment->index);
	g_free(indexed_assignment->source);
	g_free(indexed_assignment);
}

struct llir_increment *llir_increment_new(char *destination)
{
	struct llir_increment *increment = g_new(struct llir_increment, 1);
	increment->destination = g_strdup(destination);
	return increment;
}

void llir_increment_free(struct llir_increment *increment)
{
	g_free(increment->destination);
	g_free(increment);
}

struct llir_decrement *llir_decrement_new(char *destination)
{
	struct llir_decrement *decrement = g_new(struct llir_decrement, 1);
	decrement->destination = g_strdup(destination);
	return decrement;
}

void llir_decrement_free(struct llir_decrement *decrement)
{
	g_free(decrement->destination);
	g_free(decrement);
}

struct llir_binary_operation *
llir_binary_operation_new(char *destination,
			  enum llir_binary_operation_type operation, char *left,
			  char *right)
{
	struct llir_binary_operation *binary_operation =
		g_new(struct llir_binary_operation, 1);

	binary_operation->destination = g_strdup(destination);
	binary_operation->operation = operation;
	binary_operation->left_operand = g_strdup(left);
	binary_operation->right_operand = g_strdup(right);

	return binary_operation;
}

void llir_binary_operation_free(struct llir_binary_operation *binary_operation)
{
	g_free(binary_operation->destination);
	g_free(binary_operation->left_operand);
	g_free(binary_operation->right_operand);
	g_free(binary_operation);
}

struct llir_unary_operation *
llir_unary_operation_new(char *destination,
			 enum llir_unary_operation_type operation, char *source)
{
	struct llir_unary_operation *unary_operation =
		g_new(struct llir_unary_operation, 1);

	unary_operation->destination = g_strdup(destination);
	unary_operation->operation = operation;
	unary_operation->source = g_strdup(source);

	return unary_operation;
}

void llir_unary_operation_free(struct llir_unary_operation *unary_operation)
{
	g_free(unary_operation->destination);
	g_free(unary_operation->source);
	g_free(unary_operation);
}

struct llir_method_call *
llir_method_call_new(char *destination, char *identifier, GArray *arguments)
{
	struct llir_method_call *method_call =
		g_new(struct llir_method_call, 1);

	method_call->destination = g_strdup(destination);
	method_call->identifier = g_strdup(identifier);
	method_call->arguments = arguments;

	return method_call;
}

void llir_method_call_free(struct llir_method_call *method_call)
{
	g_free(method_call->destination);
	g_free(method_call->identifier);

	for (uint32_t i = 0; i < method_call->arguments->len; i++) {
		struct llir_method_call_argument *method_call_argument =
			g_array_index(method_call->arguments,
				      struct llir_method_call_argument *, i);
		g_free(method_call_argument->identifier);
		g_free(method_call_argument->string);
	}
	g_array_free(method_call->arguments, true);

	g_free(method_call);
}

struct llir_array_index *llir_array_index_new(char *destination, char *source,
					      char *index)
{
	struct llir_array_index *array_index =
		g_new(struct llir_array_index, 1);

	array_index->destination = g_strdup(destination);
	array_index->source = g_strdup(source);
	array_index->index = g_strdup(index);

	return array_index;
}

void llir_array_index_free(struct llir_array_index *array_index)
{
	g_free(array_index->destination);
	g_free(array_index->source);
	g_free(array_index->index);
	g_free(array_index);
}

struct llir_branch *llir_branch_new(char *condition)
{
	struct llir_branch *branch = g_new(struct llir_branch, 1);

	branch->condition = g_strdup(condition);
	branch->branch = NULL;

	return branch;
}

void llir_branch_free(struct llir_branch *branch)
{
	g_free(branch->condition);
	g_free(branch);
}
