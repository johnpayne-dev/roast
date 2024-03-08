#pragma once

#include "semantics/ir.h"
#include "parser/ast.h"

struct semantics {
	bool error;
	GArray *fields_table_stack;
	methods_table_t *methods_table;
	struct ir_method *current_method;
	uint32_t loop_depth;
};

struct semantics *semantics_new(void);

int semantics_analyze(struct semantics *semantics, struct ast_node *ast,
		      struct ir_program **ir);

void semantics_free(struct semantics *semantics);
