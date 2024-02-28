#pragma once

#include <glib.h>
#include <stdbool.h>

struct symbol_table {
	struct symbol_table *parent;
	GHashTable *method_table;
	GHashTable *fields_table;
};

enum data_type { DATA_TYPE_INT, DATA_TYPE_BOOL, DATA_TYPE_VOID };

struct method_descriptor {
	bool imported;
	enum data_type return_type;
	GArray *arguments;
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
	GArray *initializers;
};