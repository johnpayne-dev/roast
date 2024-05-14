#pragma once
#include "assembly/llir.h"
#include "assembly/symbol_table.h"

struct code_generator {
	struct llir *llir;
	GHashTable *strings;
	uint64_t string_counter;
	GHashTable *offsets;
	GHashTable *variable_to_register;
	GHashTable *register_to_variable;
	const char *least_recently_used[5];
	bool pinhole_optimize;
};

struct code_generator *code_generator_new(bool pinhole_optimize);

void code_generator_generate(struct code_generator *generator,
			     struct llir *llir);

void code_generator_free(struct code_generator *generator);
