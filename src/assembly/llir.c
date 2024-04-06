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
	case LLIR_NODE_TYPE_INSTRUCTION:
		node->instruction = data;
		break;
	default:
		node->import = NULL;
		break;
	}

	return node;
}

static void append_nodes(struct llir_node **node, struct llir_node *appended)
{
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
nodes_from_binary_expression(struct ir_binary_expression *binary_expression)
{
}

static struct llir_node *
nodes_from_not_expression(struct ir_expression *not_expression)
{
}

static struct llir_node *
nodes_from_negate_expression(struct ir_expression *negate_expression)
{
}

static struct llir_node *nodes_from_len_identifier(char *len_identifier)
{
}

static struct llir_node *
nodes_from_method_call(struct ir_method_call *method_call)
{
}

static struct llir_node *nodes_from_literal(struct ir_literal *literal)
{
}

static struct llir_node *nodes_from_location(struct ir_location *location)
{
}

static struct llir_node *
nodes_from_expression(struct ir_expression *ir_expression)
{
	struct llir_node *node = NULL;

	switch (ir_expression->type) {
	case IR_EXPRESSION_TYPE_BINARY:
		node = node_from_binary_expression(
			ir_expression->binary_expression);
		break;
	case IR_EXPRESSION_TYPE_NOT:
		node = node_from_not_expression(ir_expression->not_expression);
		break;
	case IR_EXPRESSION_TYPE_NEGATE:
		node = node_from_negate_expression(
			ir_expression->negate_expression);
		break;
	case IR_EXPRESSION_TYPE_LEN:
		node = node_from_len_identifier(ir_expression->len_identifier);
		break;
	case IR_EXPRESSION_TYPE_METHOD_CALL:
		node = node_from_method_call(ir_expression->method_call);
		break;
	case IR_EXPRESSION_TYPE_LITERAL:
		node = node_from_literal(ir_expression->literal);
		break;
	case IR_EXPRESSION_TYPE_LOCATION:
		node = node_from_location(ir_expression->location);
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
nodes_from_method_call(struct ir_method_call *ir_method_call)
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
