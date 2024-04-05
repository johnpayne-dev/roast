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
	g_assert(!"TODO");
	return NULL;
}

struct llir_node *llir_node_new_block(struct ir_block *ir_block)
{
	g_assert(!"TODO");
	return NULL;
}

struct llir_node *llir_node_new_instructions(struct ir_statement *ir_statement)
{
	g_assert(!"TODO");
	return NULL;
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
