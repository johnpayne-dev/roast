#pragma once
#include "assembly/llir.h"
#include "assembly/symbol_table.h"

struct llir_generator {
	struct llir_node *head;
	struct llir_node *current;
	uint32_t temporary_counter;
	uint32_t label_counter;
	struct symbol_table *symbol_table;
	GArray *break_labels;
	GArray *continue_labels;
};

struct llir_generator *llir_generator_new(void);

struct llir_node *llir_generator_generate_llir(struct llir_generator *assembly,
					       struct ir_program *ir);

void llir_generator_free(struct llir_generator *assembly);
