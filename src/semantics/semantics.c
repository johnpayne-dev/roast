#include "semantics/semantics.h"
#include "semantics/ir.h"

#define semantic_error(semantics, token, ...)                               \
	do {                                                                \
		semantics->error = true;                                    \
		uint32_t line_number = token_get_line_number(&token);       \
		uint32_t column_number = token_get_column_number(&token);   \
		g_printerr("SEMANTIC-ERROR at %s:%i:%i: ", token.file_name, \
			   line_number, column_number);                     \
		g_printerr(__VA_ARGS__);                                    \
		g_printerr("\n");                                           \
	} while (0)

static void push_scope(struct semantics *semantics,
		       fields_table_t *fields_table)
{
	g_array_append_val(semantics->fields_table_stack, fields_table);
}

static fields_table_t *current_scope(struct semantics *semantics)
{
	return g_array_index(semantics->fields_table_stack, fields_table_t *,
			     semantics->fields_table_stack->len - 1);
}

static void pop_scope(struct semantics *semantics)
{
	g_array_remove_index(semantics->fields_table_stack,
			     semantics->fields_table_stack->len - 1);
}

static void declare_method(struct semantics *semantics,
			   struct ir_method *method)
{
	fields_table_t *fields_table = current_scope(semantics);

	g_assert(fields_table_get_parent(fields_table) == NULL);

	if (methods_table_get(semantics->methods_table, method->identifier))
		semantic_error(semantics, method->token,
			       "Redeclaration of method '%s'",
			       method->identifier);
	else if (fields_table_get(fields_table, method->identifier, false))
		semantic_error(semantics, method->token,
			       "Identifier '%s' already declared as field",
			       method->identifier);
	else
		methods_table_add(semantics->methods_table, method->identifier,
				  method);
}

static void declare_field(struct semantics *semantics, struct ir_field *field)
{
	fields_table_t *fields_table = current_scope(semantics);
	bool global = fields_table_get_parent(fields_table) == NULL;

	if (fields_table_get(fields_table, field->identifier, false))
		semantic_error(semantics, field->token,
			       "Redeclaration of field '%s'",
			       field->identifier);
	else if (global &&
		 methods_table_get(semantics->methods_table, field->identifier))
		semantic_error(semantics, field->token,
			       "Identifier '%s' already declared as method",
			       field->identifier);
	else
		fields_table_add(fields_table, field->identifier, field);
}

static struct ir_method *get_method_declaration(struct semantics *semantics,
						char *identifier,
						struct token token)
{
	fields_table_t *fields_table = current_scope(semantics);
	struct ir_field *field =
		fields_table_get(fields_table, identifier, true);
	if (field != NULL) {
		semantic_error(semantics, token,
			       "Identifier '%s' is not a method", identifier);
		return NULL;
	}

	struct ir_method *method =
		methods_table_get(semantics->methods_table, identifier);
	if (method == NULL)
		semantic_error(semantics, token, "Undeclared method '%s'",
			       identifier);

	return method;
}

static struct ir_field *get_field_declaration(struct semantics *semantics,
					      char *identifier,
					      struct token token)
{
	fields_table_t *fields_table = current_scope(semantics);
	struct ir_field *field =
		fields_table_get(fields_table, identifier, true);
	if (field == NULL)
		semantic_error(semantics, token, "Undeclared field '%s'",
			       identifier);

	return field;
}

static enum ir_data_type analyze_literal(struct semantics *semantics,
					 struct ir_literal *literal)
{
	if (literal->type == IR_LITERAL_TYPE_INT) {
		if (literal->value == (uint64_t)-1)
			semantic_error(semantics, literal->token,
				       "Overflow in int literal '%.*s'",
				       literal->token.length,
				       literal->token.source);
	}

	switch (literal->type) {
	case IR_LITERAL_TYPE_INT:
		return IR_DATA_TYPE_INT;
	case IR_LITERAL_TYPE_CHAR:
		return IR_DATA_TYPE_INT;
	case IR_LITERAL_TYPE_BOOL:
		return IR_DATA_TYPE_BOOL;
	default:
		g_assert(!"Invalid literal type");
		return -1;
	}
}

static enum ir_data_type analyze_expression(struct semantics *semantics,
					    struct ir_expression *expression);

static enum ir_data_type analyze_location(struct semantics *semantics,
					  struct ir_location *location,
					  bool *constant)
{
	if (constant != NULL)
		*constant = false;

	struct ir_field *field = get_field_declaration(
		semantics, location->identifier, location->token);
	if (field == NULL)
		return IR_DATA_TYPE_VOID;

	if (constant != NULL)
		*constant = field->constant;

	enum ir_data_type type = field->type;

	if (location->index != NULL) {
		if (ir_data_type_is_array(type))
			type -= 1;
		else
			semantic_error(
				semantics, location->token,
				"Cannot index into a non-array field '%s'",
				location->identifier);

		enum ir_data_type index_type =
			analyze_expression(semantics, location->index);
		if (index_type != IR_DATA_TYPE_INT)
			semantic_error(
				semantics, location->token,
				"Indexing expression must be of type int");
	}

	return type;
}

static enum ir_data_type
analyze_binary_expression(struct semantics *semantics,
			  struct ir_binary_expression *expression)
{
	enum ir_data_type left_type =
		analyze_expression(semantics, expression->left);
	enum ir_data_type right_type =
		analyze_expression(semantics, expression->right);

	if (expression->binary_operator == IR_BINARY_OPERATOR_ADD ||
	    expression->binary_operator == IR_BINARY_OPERATOR_SUB ||
	    expression->binary_operator == IR_BINARY_OPERATOR_MUL ||
	    expression->binary_operator == IR_BINARY_OPERATOR_DIV ||
	    expression->binary_operator == IR_BINARY_OPERATOR_MOD ||
	    expression->binary_operator == IR_BINARY_OPERATOR_LESS ||
	    expression->binary_operator == IR_BINARY_OPERATOR_LESS_EQUAL ||
	    expression->binary_operator == IR_BINARY_OPERATOR_GREATER_EQUAL ||
	    expression->binary_operator == IR_BINARY_OPERATOR_GREATER) {
		if (left_type != IR_DATA_TYPE_INT ||
		    right_type != IR_DATA_TYPE_INT)
			semantic_error(
				semantics, expression->token,
				"arithmetic operators can only operate on expressions of type int");
	} else if (expression->binary_operator == IR_BINARY_OPERATOR_OR ||
		   expression->binary_operator == IR_BINARY_OPERATOR_AND) {
		if (left_type != IR_DATA_TYPE_BOOL ||
		    right_type != IR_DATA_TYPE_BOOL)
			semantic_error(
				semantics, expression->token,
				"boolean operators can only operate on expressions of type bool");
	} else {
		if (ir_data_type_is_array(left_type) ||
		    ir_data_type_is_array(right_type))
			semantic_error(
				semantics, expression->token,
				"Cannot perform operations on array type");
		if (left_type != right_type)
			semantic_error(
				semantics, expression->token,
				"Mismatching types in binary expression");
	}

	switch (expression->binary_operator) {
	case IR_BINARY_OPERATOR_ADD:
		return IR_DATA_TYPE_INT;
	case IR_BINARY_OPERATOR_MUL:
		return IR_DATA_TYPE_INT;
	case IR_BINARY_OPERATOR_SUB:
		return IR_DATA_TYPE_INT;
	case IR_BINARY_OPERATOR_DIV:
		return IR_DATA_TYPE_INT;
	case IR_BINARY_OPERATOR_MOD:
		return IR_DATA_TYPE_INT;
	case IR_BINARY_OPERATOR_LESS:
		return IR_DATA_TYPE_BOOL;
	case IR_BINARY_OPERATOR_LESS_EQUAL:
		return IR_DATA_TYPE_BOOL;
	case IR_BINARY_OPERATOR_GREATER_EQUAL:
		return IR_DATA_TYPE_BOOL;
	case IR_BINARY_OPERATOR_GREATER:
		return IR_DATA_TYPE_BOOL;
	case IR_BINARY_OPERATOR_OR:
		return IR_DATA_TYPE_BOOL;
	case IR_BINARY_OPERATOR_AND:
		return IR_DATA_TYPE_BOOL;
	case IR_BINARY_OPERATOR_EQUAL:
		return IR_DATA_TYPE_BOOL;
	case IR_BINARY_OPERATOR_NOT_EQUAL:
		return IR_DATA_TYPE_BOOL;
	default:
		g_assert(!"Invalid binary operator");
		return IR_DATA_TYPE_VOID;
	}
}

static enum ir_data_type
analyze_not_expression(struct semantics *semantics,
		       struct ir_expression *expression)
{
	enum ir_data_type type = analyze_expression(semantics, expression);
	if (type != IR_DATA_TYPE_BOOL)
		semantic_error(
			semantics, expression->token,
			"Cannot perform boolean operation on an expression that is not of type bool");

	return IR_DATA_TYPE_BOOL;
}

static enum ir_data_type
analyze_negate_expression(struct semantics *semantics,
			  struct ir_expression *expression)
{
	enum ir_data_type type = analyze_expression(semantics, expression);
	if (type != IR_DATA_TYPE_INT)
		semantic_error(
			semantics, expression->token,
			"Cannot negate an expression that is not of type int");

	return IR_DATA_TYPE_INT;
}

static enum ir_data_type
analyze_len_expression(struct semantics *semantics,
		       struct ir_length_expression *length_expression,
		       struct token token)
{
	struct ir_field *field = get_field_declaration(
		semantics, length_expression->identifier, token);
	if (field == NULL)
		return IR_DATA_TYPE_INT;

	if (!ir_data_type_is_array(field->type))
		semantic_error(semantics, token,
			       "Cannot take len of non-array field '%s'",
			       length_expression->identifier);

	else
		length_expression->length = field->array_length;

	return IR_DATA_TYPE_INT;
}

static void
analyze_imported_method_call_argument(struct semantics *semantics,
				      struct ir_method_call_argument *argument)
{
	if (argument->type == IR_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION)
		analyze_expression(semantics, argument->expression);
}

static void
analyze_method_call_argument(struct semantics *semantics,
			     struct ir_method_call_argument *argument,
			     struct ir_field *field)
{
	if (argument->type == IR_METHOD_CALL_ARGUMENT_TYPE_STRING)
		semantic_error(semantics, field->token,
			       "String not allowed in non-imported methods");

	if (argument->type == IR_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION) {
		enum ir_data_type type =
			analyze_expression(semantics, argument->expression);
		if (type != field->type)
			semantic_error(
				semantics, field->token,
				"Incorrect type passed into method call");
	}
}

static enum ir_data_type analyze_method_call(struct semantics *semantics,
					     struct ir_method_call *call)
{
	struct ir_method *method = get_method_declaration(
		semantics, call->identifier, call->token);
	if (method == NULL)
		return IR_DATA_TYPE_VOID;

	if (method->imported) {
		for (uint32_t i = 0; i < call->arguments->len; i++) {
			struct ir_method_call_argument *argument =
				g_array_index(call->arguments,
					      struct ir_method_call_argument *,
					      i);
			analyze_imported_method_call_argument(semantics,
							      argument);
		}
	} else {
		if (method->arguments->len != call->arguments->len) {
			semantic_error(
				semantics, call->token,
				"Incorrect number of arguments passed into method call '%s'",
				call->identifier);
			return method->return_type;
		}

		for (uint32_t i = 0; i < call->arguments->len; i++) {
			struct ir_method_call_argument *argument =
				g_array_index(call->arguments,
					      struct ir_method_call_argument *,
					      i);
			struct ir_field *field = g_array_index(
				method->arguments, struct ir_field *, i);
			analyze_method_call_argument(semantics, argument,
						     field);
		}
	}

	return method->return_type;
}

static enum ir_data_type analyze_expression(struct semantics *semantics,
					    struct ir_expression *expression)
{
	enum ir_data_type type;

	switch (expression->type) {
	case IR_EXPRESSION_TYPE_LEN:
		return analyze_len_expression(semantics,
					      expression->length_expression,
					      expression->token);
	case IR_EXPRESSION_TYPE_NEGATE:
		return analyze_negate_expression(semantics,
						 expression->negate_expression);
	case IR_EXPRESSION_TYPE_NOT:
		return analyze_not_expression(semantics,
					      expression->not_expression);
	case IR_EXPRESSION_TYPE_LITERAL:
		return analyze_literal(semantics, expression->literal);
	case IR_EXPRESSION_TYPE_LOCATION:
		return analyze_location(semantics, expression->location, NULL);
	case IR_EXPRESSION_TYPE_METHOD_CALL:
		type = analyze_method_call(semantics, expression->method_call);
		if (type == IR_DATA_TYPE_VOID)
			semantic_error(semantics, expression->token,
				       "method call does not return a value");
		return type;
	case IR_EXPRESSION_TYPE_BINARY:
		return analyze_binary_expression(semantics,
						 expression->binary_expression);
	default:
		g_assert(!"Invalid expression type");
		return IR_DATA_TYPE_VOID;
	}
}

static void analyze_block(struct semantics *semantics, struct ir_block *block,
			  GArray *initial_fields);

static void analyze_assignment(struct semantics *semantics,
			       struct ir_assignment *assignment)
{
	bool constant;
	enum ir_data_type location_type =
		analyze_location(semantics, assignment->location, &constant);

	if (constant)
		semantic_error(semantics, assignment->token,
			       "Cannot assign to constant field '%s'",
			       assignment->location->identifier);
	if (ir_data_type_is_array(location_type))
		semantic_error(semantics, assignment->token,
			       "Cannot assign to array typed field '%s'",
			       assignment->location->identifier);

	if (assignment->assign_operator != IR_ASSIGN_OPERATOR_SET &&
	    location_type != IR_DATA_TYPE_INT)
		semantic_error(
			semantics, assignment->token,
			"Arithmetic assignment cannot be applied to field that is not of type int");

	if (assignment->expression != NULL) {
		enum ir_data_type expression_type =
			analyze_expression(semantics, assignment->expression);
		if (expression_type != location_type)
			semantic_error(
				semantics, assignment->token,
				"Expression is not the same type as field");
	}
}

static void analyze_while_statement(struct semantics *semantics,
				    struct ir_while_statement *statement)
{
	semantics->loop_depth++;
	enum ir_data_type type =
		analyze_expression(semantics, statement->condition);
	if (type != IR_DATA_TYPE_BOOL)
		semantic_error(
			semantics, statement->token,
			"Condition expression in while statement must be of type bool");

	analyze_block(semantics, statement->block, NULL);
	semantics->loop_depth--;
}

static void analyze_for_update(struct semantics *semantics,
			       struct ir_for_update *update)
{
	if (update->type == IR_FOR_UPDATE_TYPE_ASSIGNMENT)
		analyze_assignment(semantics, update->assignment);
	else if (update->type == IR_FOR_UPDATE_TYPE_METHOD_CALL)
		analyze_method_call(semantics, update->method_call);
}

static void analyze_for_statement(struct semantics *semantics,
				  struct ir_for_statement *statement)
{
	semantics->loop_depth++;
	analyze_assignment(semantics, statement->initial);
	enum ir_data_type type =
		analyze_expression(semantics, statement->condition);
	if (type != IR_DATA_TYPE_BOOL)
		semantic_error(
			semantics, statement->token,
			"Condition expression in for statement must be of type bool");

	analyze_for_update(semantics, statement->update);
	analyze_block(semantics, statement->block, NULL);
	semantics->loop_depth--;
}

static void analyze_if_statement(struct semantics *semantics,
				 struct ir_if_statement *statement)
{
	enum ir_data_type type =
		analyze_expression(semantics, statement->condition);
	if (type != IR_DATA_TYPE_BOOL)
		semantic_error(
			semantics, statement->token,
			"Condition expression in if statement must be of type bool");

	analyze_block(semantics, statement->if_block, NULL);
	if (statement->else_block != NULL)
		analyze_block(semantics, statement->else_block, NULL);
}

static void analyze_return_statement(struct semantics *semantics,
				     struct ir_expression *expression,
				     struct token token)
{
	enum ir_data_type type = IR_DATA_TYPE_VOID;
	if (expression != NULL)
		type = analyze_expression(semantics, expression);

	if (type != semantics->current_method->return_type)
		semantic_error(
			semantics, token,
			"return expression does not match method return type");
}

static void analyze_continue_statement(struct semantics *semantics,
				       struct token token)
{
	if (semantics->loop_depth == 0)
		semantic_error(semantics, token,
			       "continue statement must be inside a loop");
}

static void analyze_break_statement(struct semantics *semantics,
				    struct token token)
{
	if (semantics->loop_depth == 0)
		semantic_error(semantics, token,
			       "break statement must be inside a loop");
}

static void analyze_statement(struct semantics *semantics,
			      struct ir_statement *statement)
{
	switch (statement->type) {
	case IR_STATEMENT_TYPE_ASSIGNMENT:
		analyze_assignment(semantics, statement->assignment);
		break;
	case IR_STATEMENT_TYPE_METHOD_CALL:
		analyze_method_call(semantics, statement->method_call);
		break;
	case IR_STATEMENT_TYPE_IF:
		analyze_if_statement(semantics, statement->if_statement);
		break;
	case IR_STATEMENT_TYPE_FOR:
		analyze_for_statement(semantics, statement->for_statement);
		break;
	case IR_STATEMENT_TYPE_WHILE:
		analyze_while_statement(semantics, statement->while_statement);
		break;
	case IR_STATEMENT_TYPE_RETURN:
		analyze_return_statement(semantics,
					 statement->return_expression,
					 statement->token);
		break;
	case IR_STATEMENT_TYPE_CONTINUE:
		analyze_continue_statement(semantics, statement->token);
		break;
	case IR_STATEMENT_TYPE_BREAK:
		analyze_break_statement(semantics, statement->token);
		break;
	default:
		break;
	}
}

static void analyze_field(struct semantics *semantics, struct ir_field *field);

static void analyze_block(struct semantics *semantics, struct ir_block *block,
			  GArray *initial_fields)
{
	fields_table_set_parent(block->fields_table, current_scope(semantics));
	push_scope(semantics, block->fields_table);

	if (initial_fields != NULL) {
		for (uint32_t i = 0; i < initial_fields->len; i++) {
			struct ir_field *field = g_array_index(
				initial_fields, struct ir_field *, i);
			analyze_field(semantics, field);
		}
	}

	for (uint32_t i = 0; i < block->fields->len; i++) {
		struct ir_field *field =
			g_array_index(block->fields, struct ir_field *, i);
		analyze_field(semantics, field);
	}

	for (uint32_t i = 0; i < block->statements->len; i++) {
		struct ir_statement *statement = g_array_index(
			block->statements, struct ir_statement *, i);
		analyze_statement(semantics, statement);
	}

	pop_scope(semantics);
}

static void analyze_method(struct semantics *semantics,
			   struct ir_method *method)
{
	declare_method(semantics, method);
	semantics->current_method = method;
	analyze_block(semantics, method->block, method->arguments);
}

static enum ir_data_type analyze_initializer(struct semantics *semantics,
					     struct ir_initializer *initializer)
{
	enum ir_data_type type = IR_DATA_TYPE_VOID;
	for (uint32_t i = 0; i < initializer->literals->len; i++) {
		struct ir_literal *literal = g_array_index(
			initializer->literals, struct ir_literal *, i);
		enum ir_data_type literal_type =
			analyze_literal(semantics, literal);

		if (type == IR_DATA_TYPE_VOID)
			type = literal_type;
		else if (literal_type != type)
			semantic_error(
				semantics, initializer->token,
				"Inconsistent types in array initializer");
	}

	if (initializer->array)
		type += 1;

	return type;
}

static void analyze_field(struct semantics *semantics, struct ir_field *field)
{
	declare_field(semantics, field);
	bool is_array = ir_data_type_is_array(field->type);

	if (field->initializer != NULL) {
		enum ir_data_type initializer_type =
			analyze_initializer(semantics, field->initializer);

		if (initializer_type != field->type)
			semantic_error(
				semantics, field->token,
				"Initializer type does not match field type");
		if (is_array && field->initializer->array &&
		    field->array_length != -1)
			semantic_error(
				semantics, field->token,
				"Array initializer cannot be provided when length is declared");

		field->array_length = field->initializer->literals->len;
	} else {
		if (is_array && field->array_length == -1)
			semantic_error(
				semantics, field->token,
				"Array fields without initializers must have a declared length");
		else if (is_array && field->array_length == 0)
			semantic_error(semantics, field->token,
				       "Array length must be greater than 0");

		if (field->constant)
			semantic_error(
				semantics, field->token,
				"Fields declared constant must have an initializer");
	}
}

static void analyze_import(struct semantics *semantics,
			   struct ir_method *import)
{
	declare_method(semantics, import);
}

static void analyze_program(struct semantics *semantics,
			    struct ir_program *program)
{
	push_scope(semantics, program->fields_table);

	for (uint32_t i = 0; i < program->imports->len; i++) {
		struct ir_method *import =
			g_array_index(program->imports, struct ir_method *, i);
		analyze_import(semantics, import);
	}

	for (uint32_t i = 0; i < program->fields->len; i++) {
		struct ir_field *field =
			g_array_index(program->fields, struct ir_field *, i);
		analyze_field(semantics, field);
	}

	for (uint32_t i = 0; i < program->methods->len; i++) {
		struct ir_method *method =
			g_array_index(program->methods, struct ir_method *, i);
		analyze_method(semantics, method);
	}

	struct ir_method *main_method =
		methods_table_get(semantics->methods_table, "main");
	if (main_method == NULL)
		semantic_error(semantics, program->token,
			       "Program must have a definition for main");
	else if (main_method->imported)
		semantic_error(semantics, program->token,
			       "main method cannot be imported");
	else if (main_method->return_type != IR_DATA_TYPE_VOID)
		semantic_error(semantics, program->token,
			       "main method must return void");
	else if (main_method->arguments->len > 0)
		semantic_error(semantics, program->token,
			       "main method must have no arguments");

	pop_scope(semantics);
}

struct semantics *semantics_new(void)
{
	struct semantics *semantics = g_new(struct semantics, 1);
	semantics->fields_table_stack =
		g_array_new(false, false, sizeof(fields_table_t *));
	return semantics;
}

int semantics_analyze(struct semantics *semantics, struct ast_node *ast,
		      struct ir_program **ir)
{
	struct ast_node *linear_nodes = ast_node_linearize(ast);
	struct ir_program *program =
		ir_program_new(&(struct ast_node *){ linear_nodes });
	g_free(linear_nodes);

	semantics->error = false;
	semantics->methods_table = program->methods_table;
	semantics->current_method = NULL;
	semantics->loop_depth = 0;
	analyze_program(semantics, program);

	if (semantics->error || ir == NULL)
		ir_program_free(program);
	else
		*ir = program;

	return semantics->error ? -1 : 0;
}

void semantics_free(struct semantics *semantics)
{
	g_array_free(semantics->fields_table_stack, true);
	g_free(semantics);
}
