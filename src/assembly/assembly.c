#include "assembly/assembly.h"

static void generate_global_strings(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_global_fields(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_method_declaration(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_fields(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_assignment(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_literal_assignment(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_indexed_assignment(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_binary_operation(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_unary_operation(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_method_call(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_array_index(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_label(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_branch(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_jump(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_return(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_data_section(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_text_section(struct code_generator *generator)
{
	g_assert(!"TODO");
}

struct code_generator *code_generator_new(void)
{
	struct code_generator *generator = g_new(struct code_generator, 1);
	generator->node = NULL;
	return generator;
}

int code_generator_generate(struct code_generator *generator,
			    struct ir_program *ir)
{
	struct llir_node *head = llir_node_new_program(ir);
	generator->node = head;

	generate_data_section(generator);
	generate_text_section(generator);

	llir_node_free(head);
	return 0;
}

void code_generator_free(struct code_generator *generator)
{
	g_free(generator);
}
