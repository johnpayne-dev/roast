#pragma once
#include "assembly/llir.h"
#include "assembly/symbol_table.h"

struct llir_generator {
	uint32_t temporary_counter;
	uint32_t block_counter;
	struct symbol_table *symbol_table;
	GArray *break_blocks;
	GArray *continue_blocks;
	struct llir_method *current_method;
	struct llir_block *current_block;
};

struct llir_generator *llir_generator_new(void);

struct llir *llir_generator_generate_llir(struct llir_generator *assembly,
					  struct ir_program *ir);

void llir_generator_free(struct llir_generator *assembly);
