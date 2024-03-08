#pragma once

#include <glib.h>

#include "scanner/token.h"

struct scanner {
	GRegex *regex;
	const char *file_name;
	const char *source;
	uint32_t position;
};

struct scanner *scanner_new(void);

void scanner_start(struct scanner *scanner, const char *file_name,
		   const char *source);

bool scanner_next_token(struct scanner *scanner, struct token *token);

int scanner_tokenize(struct scanner *scanner, const char *file_name,
		     const char *source, bool print_output, GArray **tokens);

void scanner_free(struct scanner *scanner);
