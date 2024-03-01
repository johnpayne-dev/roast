#include "semantics/symbol_table.h"

struct symbol_table {
	struct symbol_table *parent;
	GHashTable *methods_table;
	GHashTable *fields_table;
};

symbol_table_t *symbol_table_new(symbol_table_t *parent_table)
{
	struct symbol_table *symbols = g_new(struct symbol_table, 1);
	symbols->parent = parent_table;
	symbols->methods_table = g_hash_table_new(g_str_hash, g_str_equal);
	symbols->fields_table = g_hash_table_new(g_str_hash, g_str_equal);
	return symbols;
}

symbol_table_t *symbol_table_get_parent(symbol_table_t *symbols)
{
	return symbols->parent;
}

void symbol_table_add_method(symbol_table_t *symbols, char *method_identifier,
			     struct method_descriptor *method)
{
	g_assert(method_identifier != NULL || method_identifier[0] != '\0');
	g_assert(method != NULL);
	g_assert(g_hash_table_insert(symbols->methods_table, method_identifier,
				     method));
	return 0;
}

struct method_descriptor *symbol_table_get_method(symbol_table_t *symbols,
						  char *method_identifier)
{
	g_assert(method_identifier != NULL || method_identifier[0] != '\0');
	return g_hash_table_lookup(symbols->methods_table, method_identifier);
}

void symbol_table_add_field(symbol_table_t *symbols, char *field_identifier,
			    struct field_descriptor *field)
{
	g_assert(field_identifier != NULL || field_identifier[0] != '\0');
	g_assert(field != NULL);
	g_assert(g_hash_table_insert(symbols->methods_table, field_identifier,
				     field));
}

struct field_descriptor *symbol_table_get_field(symbol_table_t *symbols,
						char *field_identifier)
{
	g_assert(field_identifier != NULL || field_identifier[0] != '\0');
	return g_hash_table_lookup(symbols->fields_table, field_identifier);
}

void symbol_table_free(symbol_table_t *symbols)
{
	g_hash_table_destroy(symbols->methods_table);
	g_hash_table_destroy(symbols->fields_table);
	g_free(symbols);
}