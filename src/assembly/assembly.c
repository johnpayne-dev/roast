#include "assembly/assembly.h"

struct code_generator *code_generator_new(void)
{
	struct code_generator *generator = g_new(struct code_generator, 1);
	generator->llir = NULL;
	return generator;
}

int code_generator_generate(struct code_generator *generator,
			    struct ir_program *ir)
{
	generator->llir = llir_node_new_program(ir);
	llir_node_free(generator->llir);

	g_assert(!"TODO");
	return -1;
}

void code_generator_free(struct code_generator *generator)
{
	g_free(generator);
}
