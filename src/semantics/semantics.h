#pragma once

#include "semantics/ir.h"
#include "parser/ast.h"

struct semantics {
	bool error;
	const char *source;
	GArray *nodes;
	uint32_t position;
	GArray *symbol_table_stack;
};

struct semantics *semantics_new(void);

int semantics_analyze(struct semantics *semantics, const char *source,
		      struct ast_node *ast, struct ir_program **ir);

void semantics_push_scope(struct semantics *semantics, symbol_table_t *table);

symbol_table_t *semantics_current_scope(struct semantics *semantics);

void semantics_pop_scope(struct semantics *semantics);

void semantics_free(struct semantics *semantics);
