#pragma once
#include "assembly/llir.h"

struct code_generator {
	struct llir_node *llir;
};

struct code_generator *code_generator_new(void);

int code_generator_generate(struct code_generator *generator,
			    struct ir_program *ir);

void code_generator_free(struct code_generator *generator);
