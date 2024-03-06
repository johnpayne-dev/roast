#include "semantics/symbol_table.h"
#include "semantics/ir.h"

static inline void validate_identifier(char *identifier)
{
	g_assert(identifier != NULL);
	g_assert(identifier[0] != '\0');
}

struct methods_table {
	GHashTable *table;
};

static inline void validate_methods_table(methods_table_t *methods)
{
	g_assert(methods != NULL);
	g_assert(methods->table != NULL);
}

static inline void validate_method_descriptor(struct ir_method *descriptor)
{
	g_assert(descriptor != NULL);
}

methods_table_t *methods_table_new(void)
{
	struct methods_table *methods = g_new(struct methods_table, 1);
	methods->table = g_hash_table_new(g_str_hash, g_str_equal);
	return methods;
}

void methods_table_free(methods_table_t *methods)
{
	validate_methods_table(methods);
	g_hash_table_destroy(methods->table);
	g_free(methods);
}

void methods_table_add(methods_table_t *methods, char *identifier,
		       struct ir_method *descriptor)
{
	validate_methods_table(methods);
	validate_identifier(identifier);
	validate_method_descriptor(descriptor);
	g_assert(g_hash_table_insert(methods->table, identifier, descriptor));
}

struct ir_method *methods_table_get(methods_table_t *methods, char *identifier)
{
	validate_methods_table(methods);
	validate_identifier(identifier);
	return g_hash_table_lookup(methods->table, identifier);
}

struct fields_table {
	struct fields_table *parent;
	GHashTable *table;
};

static inline void validate_fields_table(fields_table_t *fields)
{
	g_assert(fields != NULL);
	g_assert(fields->table != NULL);
}

static inline void validate_field_descriptor(struct ir_field *descriptor)
{
	g_assert(descriptor != NULL);
}

fields_table_t *fields_table_new(fields_table_t *parent)
{
	struct fields_table *fields = g_new(struct fields_table, 1);
	fields->parent = parent;
	fields->table = g_hash_table_new(g_str_hash, g_str_equal);
	return fields;
}

void fields_table_free(fields_table_t *fields)
{
	validate_fields_table(fields);
	g_hash_table_destroy(fields->table);
	g_free(fields);
}

fields_table_t *fields_table_get_parent(fields_table_t *fields)
{
	validate_fields_table(fields);
	return fields->parent;
}

void fields_table_add(fields_table_t *fields, char *identifier,
		      struct ir_field *descriptor)
{
	validate_fields_table(fields);
	validate_identifier(identifier);
	validate_field_descriptor(descriptor);
	g_assert(g_hash_table_insert(fields->table, identifier, descriptor));
}

struct ir_field *fields_table_get(fields_table_t *fields, char *identifier,
				  bool climb)
{
	validate_fields_table(fields);
	validate_identifier(identifier);
	fields_table_t *current_scope = fields;
	struct ir_field *descriptor = NULL;
	while (((descriptor = g_hash_table_lookup(current_scope->table,
						  identifier)) == NULL) &&
	       climb) {
		current_scope = fields_table_get_parent(current_scope);
		if (current_scope == NULL)
			break;
		validate_fields_table(current_scope);
	}
	return descriptor;
}
