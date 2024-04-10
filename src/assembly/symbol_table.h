#pragma once
#include <glib.h>

struct symbol_table {
	GArray *tables;
};

struct symbol_table *symbol_table_new(void);

void symbol_table_set(struct symbol_table *symbol_table, char *identifier,
		      void *data);

void *symbol_table_get(struct symbol_table *symbol_table, char *identifier);

void symbol_table_push_scope(struct symbol_table *symbol_table);

uint32_t symbol_table_get_scope_level(struct symbol_table *symbol_table);

void symbol_table_pop_scope(struct symbol_table *symbol_table);

void symbol_table_free(struct symbol_table *symbol_table);
