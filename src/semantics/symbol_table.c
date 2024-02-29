#include "symbol_table.h"

struct symbol_table {
	struct symbol_table *parent;
	GHashTable *method_table;
	GHashTable *fields_table;
};

symbol_table_t *symbol_table_new(symbol_table_t *parent_table)
{
	struct symbol_table *symbols = g_new(struct symbol_table, 1);
	symbols->parent = parent_table;
	symbols->method_table = g_hash_table_new(g_str_hash, g_str_equal);
	symbols->fields_table = g_hash_table_new(g_str_hash, g_str_equal);
	return symbols;
}

symbol_table_t *symbol_table_get_parent(symbol_table_t *symbols)
{
	return symbols->parent;
}

int symbol_table_add_method(symbol_table_t *symbols, char *method_identifier,
			    struct method_descriptor *method)
{
	if (method_identifier == NULL || method_identifier[0] == '\0') {
		g_error("Cannot add method without its identifier");
		return -1;
	}
	if (method == NULL) {
		g_error("Cannot add method without its descriptor");
		return -1;
	}
	if (g_hash_table_insert(symbols->method_table, method_identifier,
				method) == false) {
		g_error("Illegal redeclaration of method %s",
			method_identifier);
		return -1;
	}
}

struct method_descriptor *symbol_table_get_method(symbol_table_t *symbols,
						  char *method_identifier)
{
	struct method_descriptor *method;
	if (g_hash_table_lookup_extended(symbols->method_table,
					 method_identifier, NULL,
					 method) == false) {
		g_error("Method %s does not exist", method_identifier);
		return NULL;
	}
	if (method == NULL)
		g_error("Method %s had no associated descriptor",
			method_identifier);
	return method;
}

int symbol_table_add_field(symbol_table_t *symbols, char *field_identifier,
			   struct method_descriptor *field)
{
	if (field_identifier == NULL || field_identifier[0] == '\0') {
		g_error("Cannot add field without its identifier");
		return -1;
	}
	if (field == NULL) {
		g_error("Cannot add field without its descriptor");
		return -1;
	}
	if (g_hash_table_insert(symbols->method_table, field_identifier,
				field) == false) {
		g_error("Illegal redeclaration of field %s", field_identifier);
		return -1;
	}
}

struct field_descriptor *symbol_table_get_field(symbol_table_t *symbols,
						char *field_identifier)
{
	struct field_descriptor *field;
	if (g_hash_table_lookup_extended(symbols->fields_table,
					 field_identifier, NULL,
					 field) == false) {
		g_error("Field %s does not exist", field_identifier);
		return NULL;
	}
	if (field == NULL)
		g_error("Field %s had no associated descriptor",
			field_identifier);
	return field;
}

static void free_hash_table(GHashTable *hash_table)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, hash_table);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		g_free(key);
		g_free(value);
	}

	g_hash_table_destroy(hash_table);
}

void symbol_table_free(symbol_table_t *symbols)
{
	free_hash_table(symbols->method_table);
	free_hash_table(symbols->fields_table);
	g_free(symbols);
}