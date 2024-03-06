#pragma once

#include <glib.h>
#include <stdbool.h>

struct ir_method;

typedef struct methods_table methods_table_t;

methods_table_t *methods_table_new(void);

void methods_table_free(methods_table_t *methods);

void methods_table_add(methods_table_t *methods, char *identifier,
		       struct ir_method *descriptor);

struct ir_method *methods_table_get(methods_table_t *methods, char *identifier);

struct ir_field;

typedef struct fields_table fields_table_t;

fields_table_t *fields_table_new(fields_table_t *parent);

void fields_table_free(fields_table_t *fields);

fields_table_t *fields_table_get_parent(fields_table_t *fields);

void fields_table_add(fields_table_t *fields, char *identifier,
		      struct ir_field *descriptor);

struct ir_field *fields_table_get(fields_table_t *fields, char *identifier,
				  bool climb);
