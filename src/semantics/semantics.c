#include "semantics/semantics.h"
#include "semantics/ir.h"

struct semantics *semantics_new(void)
{
	struct semantics *semantics = g_new(struct semantics, 1);
	semantics->symbol_table_stack =
		g_array_new(false, false, sizeof(symbol_table_t *));
	return semantics;
}

static void linearize_ast(struct ast_node *ast, GArray *nodes)
{
	g_array_append_val(nodes, ast);
	for (uint32_t i = 0; i < ast_node_child_count(ast); i++)
		linearize_ast(ast_node_get_child(ast, i), nodes);
}

int semantics_analyze(struct semantics *semantics, const char *source,
		      struct ast_node *ast, struct ir_program **ir)
{
	semantics->error = false;
	semantics->source = source;
	semantics->nodes = g_array_new(false, false, sizeof(struct ast_node *));
	semantics->position = 0;

	linearize_ast(ast, semantics->nodes);
	struct ast_node *terminating_node = ast_node_new(-1);
	g_array_append_val(semantics->nodes, terminating_node);

	struct ir_program *program = ir_program_new(semantics);

	if (semantics->error || ir == NULL)
		ir_program_free(program);
	else
		*ir = program;

	g_array_free(semantics->nodes, true);
	ast_node_free(terminating_node);
	return semantics->error ? -1 : 0;
}

void semantics_push_scope(struct semantics *semantics, symbol_table_t *table)
{
	g_array_append_val(semantics->symbol_table_stack, table);
}

symbol_table_t *semantics_current_scope(struct semantics *semantics)
{
	return g_array_index(semantics->symbol_table_stack, symbol_table_t *,
			     semantics->symbol_table_stack->len - 1);
}

void semantics_pop_scope(struct semantics *semantics)
{
	g_array_remove_index(semantics->symbol_table_stack,
			     semantics->symbol_table_stack->len - 1);
}

void semantics_free(struct semantics *semantics)
{
	g_array_free(semantics->symbol_table_stack, true);
	g_free(semantics);
}
