#pragma once

#include <glib.h>
#include <stdbool.h>

struct ir_field;

typedef struct fields_table fields_table_t;

fields_table_t *fields_table_new(void);

void fields_table_free(fields_table_t *fields_table);

void fields_table_set_parent(fields_table_t *fields, fields_table_t *parent);

fields_table_t *fields_table_get_parent(fields_table_t *fields_table);

void fields_table_add(fields_table_t *fields_table, char *identifier,
		      struct ir_field *field);

struct ir_field *fields_table_get(fields_table_t *fields_table,
				  char *identifier, bool climb);

struct ir_method;

typedef struct methods_table methods_table_t;

methods_table_t *methods_table_new(void);

void methods_table_free(methods_table_t *methods_table);

void methods_table_add(methods_table_t *methods_table, char *identifier,
		       struct ir_method *descriptor);

struct ir_method *methods_table_get(methods_table_t *methods_table,
				    char *method);
