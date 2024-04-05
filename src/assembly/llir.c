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

struct llir_node *llir_node_new_program(struct ir_program *program)
{
	g_assert(!"TODO");
	return NULL;
}

struct llir_node *llir_node_new_method(struct ir_method *ir_method)
{
	g_assert(ir_method->imported == false);

	struct llir_node *node = g_new(struct llir_node, 1);

	node->type = LLIR_NODE_TYPE_METHOD;

	node->method = llir_method_new(ir_method);

	node->next = llir_node_new_block(ir_method->block);

	return node;
}

struct llir_node *llir_node_new_field(struct ir_field *field)
{
	g_assert(!"TODO");
	return NULL;
}

struct llir_node *llir_node_new_block(struct ir_block *block)
{
	g_assert(!"TODO");
	return NULL;
}

struct llir_node *llir_node_new_instructions(struct ir_statement *statement)
{
	g_assert(!"TODO");
	return NULL;
}

void llir_node_free(struct llir_node *node)
{
	g_assert(!"TODO");
}

struct llir_method *llir_method_new(struct ir_method *ir_method)
{
	struct llir_method *method = g_new(struct llir_method, 1);

	method->identifier = g_strdup(ir_method->identifier);

	method->arguments =
		g_array_new(false, false, sizeof(struct llir_field *));

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