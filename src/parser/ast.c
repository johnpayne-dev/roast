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
