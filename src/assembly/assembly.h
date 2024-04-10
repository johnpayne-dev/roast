#pragma once
#include "assembly/llir.h"
#include "assembly/symbol_table.h"

struct assembly {
	struct llir_node *head;
	struct llir_node *current;
	uint32_t temporary_variable_counter;
	uint32_t label_counter;
	struct symbol_table *symbol_table;
	GArray *break_labels;
	GArray *continue_labels;
};

struct assembly *assembly_new(void);

struct llir_node *assembly_generate_llir(struct assembly *assembly,
					 struct ir_program *ir);

void assembly_free(struct assembly *assembly);
