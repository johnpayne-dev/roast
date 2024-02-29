#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "parser/ast.h"
#include "scanner/scanner.h"

struct parser {
	struct token *tokens;
	uint32_t token_count;
	const char *source;
	uint32_t position;
	bool parse_error;
};

struct parser *parser_new(void);

int parser_parse(struct parser *parser, const char *source, GArray *token_list,
		 struct ast_node **ast);

void parser_free(struct parser *parser);
