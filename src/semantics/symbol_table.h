#pragma once

#include <glib.h>
#include <stdbool.h>

typedef struct symbol_table symbol_table_t;

struct ir_method;
struct ir_field;

symbol_table_t *symbol_table_new(struct symbol_table *parent_table);

symbol_table_t *symbol_table_get_parent(symbol_table_t *symbols);

void symbol_table_add_method(symbol_table_t *symbols, char *method_identifier,
			     struct ir_method *method);

struct ir_method *symbol_table_get_method(symbol_table_t *symbols,
					  char *method_identifier);

void symbol_table_add_field(symbol_table_t *symbols, char *field_identifier,
			    struct ir_field *field);

struct ir_field *symbol_table_get_field(symbol_table_t *symbols,
					char *field_identifier);

void symbol_table_free(symbol_table_t *symbols);
