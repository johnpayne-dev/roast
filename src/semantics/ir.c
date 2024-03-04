#include "semantics/ir.h"

static bool has_error = false;

bool ir_has_error(void)
{
	if (has_error) {
		has_error = false;
		return true;
	}

	return false;
}

static void semantic_error(const char *message, struct ast_node *node)
{
	has_error = true;
	g_printerr("ERROR at %i:%i: %s\n", 0, 0, message);
}

/* karl */
enum ir_type ir_type_from_ast(struct ast_node *node);

/* john */
int64_t ir_int_literal_from_ast(struct ast_node *node);

/* karl */
bool ir_bool_literal_from_ast(struct ast_node *node);

/* john */
char ir_char_literal_from_ast(struct ast_node *node);

/* karl */
char *ir_string_literal_from_ast(struct ast_node *node);

/* john */
struct ir_program *ir_program_new(struct ast_node *node);
void ir_program_free(struct ir_program *program);

/* karl */
struct ir_method_descriptor *ir_method_descriptor_new(struct ast_node *node);
void ir_method_descriptor_free(struct ir_method_descriptor *method);

/* john */
struct ir_field_descriptor *ir_field_descriptor_new(struct ast_node *node);
void ir_field_descriptor_free(struct ir_field_descriptor *field);

/* karl */
struct ir_block *ir_block_new(struct ast_node *node);
void ir_block_free(struct ir_block *block);

/* john */
struct ir_statement *ir_statement_new(struct ast_node *node);
void ir_statement_free(struct ir_statement *statement);

/* karl */
struct ir_assignment *ir_assignment_new(struct ast_node *node);
void ir_assignment_free(struct ir_assignment *assignment);

/* john */
struct ir_method_call *ir_method_call_new(struct ast_node *node);
void ir_method_call_free(struct ir_method_call *method_call);

/* karl */
struct ir_if_statement *ir_if_statement_new(struct ast_node *node);
void ir_if_statement_free(struct ir_if_statement *statement);

/* john */
struct ir_for_statement *ir_for_statement_new(struct ast_node *node);
void ir_for_statement_free(struct ir_for_statement *statement);

/* karl */
struct ir_while_statement *ir_while_statement_new(struct ast_node *node);
void ir_while_statement_free(struct ir_while_statement *statement);

/* john */
struct ir_location *ir_location_new(struct ast_node *node);
void ir_location_free(struct ir_location *location);

/* karl */
struct ir_expression *ir_expression_new(struct ast_node *node);
void ir_expression_free(struct ir_expression *expression);

/* john */
struct ir_binary_expression *ir_binary_expression_new(struct ast_node *node);
void ir_binary_expression_free(struct ir_binary_expression *expression);

/* karl */
struct ir_literal *ir_literal_new(struct ast_node *node);
void ir_literal_free(struct ir_literal *expression);
