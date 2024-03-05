#include "semantics/semantics.h"
#include "semantics/ir.h"

struct semantics *semantics_new(void)
{
	return g_new(struct semantics, 1);
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

	struct ir_program *program = ir_program_new(semantics);

	if (semantics->error || ir == NULL)
		ir_program_free(program);
	else
		*ir = program;

	g_array_free(semantics->nodes, true);
	return semantics->error ? -1 : 0;
}

void semantics_free(struct semantics *semantics)
{
	g_free(semantics);
}
