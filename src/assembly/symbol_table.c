#include <stdbool.h>

#include "assembly/symbol_table.h"

struct symbol_table *symbol_table_new(void)
{
	struct symbol_table *symbol_table = g_new(struct symbol_table, 1);

	symbol_table->tables = g_array_new(false, false, sizeof(GHashTable *));
	symbol_table_push_scope(symbol_table);

	return symbol_table;
}

void symbol_table_set(struct symbol_table *symbol_table, char *identifier,
		      void *data)
{
	g_assert(symbol_table->tables->len > 0);
	GHashTable *table = g_array_index(symbol_table->tables, GHashTable *,
					  symbol_table->tables->len - 1);

	g_hash_table_insert(table, identifier, data);
}

void *symbol_table_get(struct symbol_table *symbol_table, char *identifier)
{
	g_assert(symbol_table->tables->len > 0);
	GHashTable *table = g_array_index(symbol_table->tables, GHashTable *,
					  symbol_table->tables->len - 1);

	return g_hash_table_lookup(table, identifier);
}

void symbol_table_push_scope(struct symbol_table *symbol_table)
{
	GHashTable *table = g_hash_table_new(g_str_hash, g_str_equal);
	g_array_append_val(symbol_table->tables, table);
}

void symbol_table_pop_scope(struct symbol_table *symbol_table)
{
	g_assert(symbol_table->tables->len > 0);
	GHashTable *table = g_array_index(symbol_table->tables, GHashTable *,
					  symbol_table->tables->len - 1);

	g_hash_table_unref(table);
	g_array_remove_index(symbol_table->tables,
			     symbol_table->tables->len - 1);
}

void symbol_table_free(struct symbol_table *symbol_table)
{
	for (uint32_t i = 0; i < symbol_table->tables->len; i++) {
		GHashTable *table =
			g_array_index(symbol_table->tables, GHashTable *, i);
		g_hash_table_unref(table);
	}
	g_array_free(symbol_table->tables, true);
	g_free(symbol_table);
}
