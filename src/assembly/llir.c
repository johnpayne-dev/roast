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

struct llir_node *llir_node_new_method(struct ir_method *method)
{
	g_assert(!"TODO");
	return NULL;
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
