#pragma once

#include <glib.h>

#include "scanner/token.h"

enum ast_node_type {
	AST_NODE_TYPE_PROGRAM,
	AST_NODE_TYPE_IMPORT,
	AST_NODE_TYPE_FIELD,
	AST_NODE_TYPE_CONST,
	AST_NODE_TYPE_FIELD_IDENTIFIER,
	AST_NODE_TYPE_EMPTY_ARRAY_LENGTH,
	AST_NODE_TYPE_METHOD,
	AST_NODE_TYPE_VOID,
	AST_NODE_TYPE_METHOD_ARGUMENT,
	AST_NODE_TYPE_BLOCK,
	AST_NODE_TYPE_STATEMENT,
	AST_NODE_TYPE_ASSIGNMENT,
	AST_NODE_TYPE_IF_STATEMENT,
	AST_NODE_TYPE_FOR_STATEMENT,
	AST_NODE_TYPE_WHILE_STATEMENT,
	AST_NODE_TYPE_RETURN_STATEMENT,
	AST_NODE_TYPE_BREAK_STATEMENT,
	AST_NODE_TYPE_CONTINUE_STATEMENT,
	AST_NODE_TYPE_FOR_UPDATE,
	AST_NODE_TYPE_LOCATION,
	AST_NODE_TYPE_LOCATION_INDEX,
	AST_NODE_TYPE_EXPRESSION,
	AST_NODE_TYPE_BINARY_EXPRESSION,
	AST_NODE_TYPE_NOT_EXPRESSION,
	AST_NODE_TYPE_NEGATE_EXPRESSION,
	AST_NODE_TYPE_LEN_EXPRESSION,
	AST_NODE_TYPE_METHOD_CALL,
	AST_NODE_TYPE_METHOD_CALL_ARGUMENT,
	AST_NODE_TYPE_METHOD_CALL_END,
	AST_NODE_TYPE_INITIALIZER,
	AST_NODE_TYPE_ARRAY_LITERAL,
	AST_NODE_TYPE_LITERAL,
	AST_NODE_TYPE_LITERAL_NEGATION,
	AST_NODE_TYPE_ASSIGN_OPERATOR,
	AST_NODE_TYPE_INCREMENT_OPERATOR,
	AST_NODE_TYPE_BINARY_OPERATOR,
	AST_NODE_TYPE_DATA_TYPE,
	AST_NODE_TYPE_IDENTIFIER,
	AST_NODE_TYPE_CHAR_LITERAL,
	AST_NODE_TYPE_STRING_LITERAL,
	AST_NODE_TYPE_BOOL_LITERAL,
	AST_NODE_TYPE_INT_LITERAL,

	AST_NODE_TYPE_PROGRAM_END,
	AST_NODE_TYPE_BLOCK_END,
};

struct ast_node {
	enum ast_node_type type;
	GArray *children;
	struct token token;
};

struct ast_node *ast_node_new(enum ast_node_type type, struct token token);

void ast_node_add_child(struct ast_node *node, struct ast_node *child);

uint32_t ast_node_child_count(struct ast_node *node);

struct ast_node *ast_node_get_child(struct ast_node *node, uint32_t i);

struct ast_node *ast_node_linearize(struct ast_node *ast);

void ast_node_free(struct ast_node *node);
