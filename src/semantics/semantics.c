#include "semantics/semantics.h"
#include "semantics/ir.h"

struct semantics *semantics_new(void)
{
	return g_new(struct semantics, 1);
}

int semantics_analyze(struct semantics *semantics, const char *source,
		      struct ast_node *ast, struct ir_program **ir)
{
	semantics->error = false;
	semantics->source = source;
	struct ir_program *program = ir_program_new(semantics, ast);

	if (semantics->error || ir == NULL)
		ir_program_free(program);
	else
		*ir = program;

	return semantics->error ? -1 : 0;
}

void semantics_free(struct semantics *semantics)
{
	g_free(semantics);
}
