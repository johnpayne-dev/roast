#pragma once
#include "assembly/llir.h"

struct code_generator {
	struct llir_node *node;
	GHashTable *strings;
	uint64_t string_counter;
	struct llir_node *global_fields_head_node;
	GArray *symbol_tables;
	uint32_t stack_pointer;
};

struct code_generator *code_generator_new(void);

int code_generator_generate(struct code_generator *generator,
			    struct ir_program *ir);

void code_generator_free(struct code_generator *generator);
