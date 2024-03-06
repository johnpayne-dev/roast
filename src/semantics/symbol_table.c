#include "semantics/symbol_table.h"
#include "semantics/ir.h"

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

void symbol_table_declare_method(symbol_table_t *symbols,
				 char *method_identifier,
				 struct ir_method *method)
{
	g_assert(method_identifier != NULL || method_identifier[0] != '\0');
	g_assert(method != NULL);
	g_assert(g_hash_table_insert(symbols->methods_table, method_identifier,
				     method));
}

struct ir_method *symbol_table_get_method(symbol_table_t *symbols,
					  char *method_identifier, bool climb)
{
	g_assert(method_identifier != NULL || method_identifier[0] != '\0');
	symbol_table_t *current_scope = symbols;
	struct ir_method *method = NULL;
	while (((method = g_hash_table_lookup(current_scope->methods_table,
					      method_identifier)) == NULL) &&
	       climb) {
		current_scope = symbol_table_get_parent(current_scope);
		if (current_scope == NULL)
			break;
	}
	return method;
}

void symbol_table_declare_field(symbol_table_t *symbols, char *field_identifier,
				struct ir_field *field)
{
	g_assert(field_identifier != NULL || field_identifier[0] != '\0');
	g_assert(field != NULL);
	g_assert(g_hash_table_insert(symbols->fields_table, field_identifier,
				     field));
}

struct ir_field *symbol_table_get_field(symbol_table_t *symbols,
					char *field_identifier, bool climb)
{
	g_assert(field_identifier != NULL || field_identifier[0] != '\0');
	symbol_table_t *current_scope = symbols;
	struct ir_field *field = NULL;
	while (((field = g_hash_table_lookup(current_scope->fields_table,
					     field_identifier)) == NULL) &&
	       climb) {
		current_scope = symbol_table_get_parent(current_scope);
		if (current_scope == NULL)
			break;
	}
	return field;
}

void symbol_table_free(symbol_table_t *symbols)
{
	g_hash_table_destroy(symbols->methods_table);
	g_hash_table_destroy(symbols->fields_table);
	g_free(symbols);
}
