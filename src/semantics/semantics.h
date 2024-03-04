#pragma once

#include "semantics/ir.h"
#include "parser/ast.h"

struct semantics {
	bool error;
	const char *source;
};

struct semantics *semantics_new(void);

int semantics_analyze(struct semantics *semantics, const char *source,
		      struct ast_node *ast, struct ir_program **ir);

void semantics_free(struct semantics *semantics);
