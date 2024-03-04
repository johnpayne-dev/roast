#include "semantics/ir.h"

static bool is_error = false;

bool ir_is_error(void)
{
	return is_error;
}

/*
 * TODO: kms
 */

enum ir_type ir_type_from_ast(struct ast_node *node);
int64_t ir_int_literal_from_ast(struct ast_node *node);
bool ir_bool_literal_from_ast(struct ast_node *node);
char ir_char_literal_from_ast(struct ast_node *node);
char *ir_string_literal_from_ast(struct ast_node *node);

struct ir_program *ir_program_new(struct ast_node *node);
void ir_program_free(struct ir_program *program);

struct ir_method_descriptor *ir_method_descriptor_new(struct ast_node *node);
void ir_method_descriptor_free(struct ir_method_descriptor *method);

struct ir_field_descriptor *ir_field_descriptor_new(struct ast_node *node);
void ir_field_descriptor_free(struct ir_field_descriptor *field);

struct ir_block *ir_block_new(struct ast_node *node);
void ir_block_free(struct ir_block *block);

struct ir_statement *ir_statement_new(struct ast_node *node);
void ir_statement_free(struct ir_statement *statement);

struct ir_statement *ir_statement_new(struct ast_node *node);
void ir_statement_free(struct ir_statement *statement);

struct ir_assignment *ir_assignment_new(struct ast_node *node);
void ir_assignment_free(struct ir_assignment *assignment);

struct ir_method_call *ir_method_call_new(struct ast_node *node);
void ir_method_call_free(struct ir_method_call *method_call);

struct ir_if_statement *ir_if_statement_new(struct ast_node *node);
void ir_if_statement_free(struct ir_if_statement *statement);

struct ir_for_statement *ir_for_statement_new(struct ast_node *node);
void ir_for_statement_free(struct ir_for_statement *statement);

struct ir_while_statement *ir_while_statement_new(struct ast_node *node);
void ir_while_statement_free(struct ir_while_statement *statement);

struct ir_location *ir_location_new(struct ast_node *node);
void ir_location_free(struct ir_location *location);

struct ir_expression *ir_expression_new(struct ast_node *node);
void ir_expression_free(struct ir_expression *expression);

struct ir_binary_expression *ir_binary_expression_new(struct ast_node *node);
void ir_binary_expression_free(struct ir_binary_expression *expression);

struct ir_literal *ir_literal_new(struct ast_node *node);
void ir_literal_free(struct ir_literal *expression);
