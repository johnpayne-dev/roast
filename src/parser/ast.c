#include "parser/ast.h"

struct ast_node *ast_node_new(enum ast_node_type type)
{
	struct ast_node *node = g_new(struct ast_node, 1);
	node->type = type;
	node->children = g_array_new(false, false, sizeof(struct ast_node *));
	node->token = NULL;
	return node;
}

struct ast_node *ast_node_new_terminal(enum ast_node_type type,
				       struct token token)
{
	struct ast_node *node = ast_node_new(type);
	node->token = g_new(struct token, 1);
	*node->token = token;
	return node;
}

void ast_node_add_child(struct ast_node *node, struct ast_node *child)
{
	g_array_append_val(node->children, child);
}

uint32_t ast_node_child_count(struct ast_node *node)
{
	return node->children->len;
}

struct ast_node *ast_node_get_child(struct ast_node *node, uint32_t i)
{
	return g_array_index(node->children, struct ast_node *, i);
}

static void append_terminating_node(GArray *nodes, enum ast_node_type type)
{
	struct ast_node terminating_node = {
		.type = type,
	};
	g_array_append_val(nodes, terminating_node);
}

static void linearize_ast(struct ast_node *ast, GArray *nodes)
{
	g_array_append_val(nodes, *ast);
	for (uint32_t i = 0; i < ast_node_child_count(ast); i++)
		linearize_ast(ast_node_get_child(ast, i), nodes);

	if (ast->type == AST_NODE_TYPE_PROGRAM)
		append_terminating_node(nodes, AST_NODE_TYPE_PROGRAM_END);
	if (ast->type == AST_NODE_TYPE_BLOCK)
		append_terminating_node(nodes, AST_NODE_TYPE_BLOCK_END);
}

struct ast_node *ast_node_linearize(struct ast_node *ast)
{
	GArray *nodes = g_array_new(false, false, sizeof(struct ast_node));
	linearize_ast(ast, nodes);

	struct ast_node *array = &g_array_index(nodes, struct ast_node, 0);
	g_array_free(nodes, false);
	return array;
}

void ast_node_free(struct ast_node *node)
{
	if (node == NULL)
		return;

	for (uint32_t i = 0; i < node->children->len; i++) {
		struct ast_node *child =
			g_array_index(node->children, struct ast_node *, i);
		ast_node_free(child);
	}
	g_array_free(node->children, true);
	g_free(node->token);

	g_free(node);
}
