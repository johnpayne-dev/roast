#pragma once

#include <glib.h>
#include <stdbool.h>

typedef struct symbol_table symbol_table_t;

enum data_type { DATA_TYPE_INT, DATA_TYPE_BOOL, DATA_TYPE_VOID };

struct method_descriptor {
	bool imported;
	enum data_type return_type;
	struct argument_descriptor *arguments;
};

struct argument_descriptor {
	enum data_type type;
	char *identifier;
};

struct field_descriptor {
	enum data_type type;
	bool constant;
	bool array;
	size_t array_length;
	int *initializers;
};

symbol_table_t *symbol_table_new(struct symbol_table *parent_table);

symbol_table_t *symbol_table_get_parent(symbol_table_t *symbols);

void symbol_table_add_method(symbol_table_t *symbols, char *method_identifier,
			     struct method_descriptor *method);

struct method_descriptor *symbol_table_get_method(symbol_table_t *symbols,
						  char *method_identifier);

void symbol_table_add_field(symbol_table_t *symbols, char *field_identifier,
			    struct field_descriptor *field);

struct field_descriptor *symbol_table_get_field(symbol_table_t *symbols,
						char *field_identifier);

void symbol_table_free(symbol_table_t *symbols);