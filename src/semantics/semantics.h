#pragma once

#include "semantics/ir.h"
#include "parser/ast.h"

struct semantics {
	bool error;
	GArray *nodes;
	uint32_t position;
	GArray *fields_table_stack;
	methods_table_t *methods_table;
};

struct semantics *semantics_new(void);

int semantics_analyze(struct semantics *semantics, struct ast_node *ast,
		      struct ir_program **ir);

void semantics_push_scope(struct semantics *semantics,
			  fields_table_t *fields_table);

fields_table_t *semantics_current_scope(struct semantics *semantics);

void semantics_pop_scope(struct semantics *semantics);

void semantics_free(struct semantics *semantics);
