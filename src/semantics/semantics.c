#include "semantics/semantics.h"
#include "semantics/ir.h"

static void semantic_error(struct semantics *semantics, const char *message)
{
	semantics->error = true;
	g_printerr("ERROR at %i:%i: %s\n", 0, 0, message);
}

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
		semantic_error(semantics, "Redeclaration of method");
	else if (fields_table_get(fields_table, method->identifier, false))
		semantic_error(semantics,
			       "Identifier already declared as field");
	else
		methods_table_add(semantics->methods_table, method->identifier,
				  method);
}

static void declare_field(struct semantics *semantics, struct ir_field *field)
{
	fields_table_t *fields_table = current_scope(semantics);

	if (fields_table_get(fields_table, field->identifier, false))
		semantic_error(semantics, "Redeclaration of field");
	else if (methods_table_get(semantics->methods_table, field->identifier))
		semantic_error(semantics,
			       "Identifier already declared as method");
	else
		fields_table_add(fields_table, field->identifier, field);
}

static struct ir_method *get_method_declaration(struct semantics *semantics,
						char *identifier)
{
	struct ir_method *method =
		methods_table_get(semantics->methods_table, identifier);
	if (method == NULL)
		semantic_error(semantics, "Undeclared method");

	return method;
}

static struct ir_field *get_field_declaration(struct semantics *semantics,
					      char *identifier)
{
	fields_table_t *fields_table = current_scope(semantics);
	struct ir_field *field =
		fields_table_get(fields_table, identifier, true);
	if (field == NULL)
		semantic_error(semantics, "Undeclared field");

	return field;
}

static enum ir_data_type analyze_literal(struct semantics *semantics,
					 struct ir_literal *literal)
{
	if (literal->type == IR_LITERAL_TYPE_INT) {
		uint64_t max_value =
			(uint64_t)INT64_MAX + (uint64_t)literal->negate;
		if (literal->value > max_value)
			semantic_error(semantics, "Overflow in int literal");
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

static enum ir_data_type
analyze_binary_expression(struct semantics *semantics,
			  struct ir_binary_expression *expression)
{
	g_assert(!"Unimplemented");
}

static enum ir_data_type analyze_expression(struct semantics *semantics,
					    struct ir_expression *expression)
{
	g_assert(!"Unimplemented");
}

static enum ir_data_type analyze_location(struct semantics *semantics,
					  struct ir_location *location)
{
	g_assert(!"Unimplemented");
}

static void analyze_block(struct semantics *semantics, struct ir_block *block,
			  GArray *initial_fields);

static void analyze_assignment(struct semantics *semantics,
			       struct ir_assignment *assignment)
{
	enum ir_data_type location_type =
		analyze_location(semantics, assignment->location);
	if (assignment->assign_operator != IR_ASSIGN_OPERATOR_SET &&
	    location_type != IR_DATA_TYPE_INT)
		semantic_error(
			semantics,
			"Arithmetic assignment cannot be applied to field that is not of type int");

	if (assignment->expression != NULL) {
		enum ir_data_type expression_type =
			analyze_expression(semantics, assignment->expression);
		if (expression_type != location_type)
			semantic_error(
				semantics,
				"Expression is not the same type as field");
	}
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
		semantic_error(semantics,
			       "String not allowed in non-imported methods");

	if (argument->type == IR_METHOD_CALL_ARGUMENT_TYPE_EXPRESSION) {
		enum ir_data_type type =
			analyze_expression(semantics, argument->expression);
		if (type != field->type)
			semantic_error(
				semantics,
				"Incorrect type passed into method call");
	}
}

static enum ir_data_type analyze_method_call(struct semantics *semantics,
					     struct ir_method_call *call)
{
	struct ir_method *method =
		get_method_declaration(semantics, call->identifier);

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
				semantics,
				"Incorrect number of arguments passed into method call");
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

static void analyze_while_statement(struct semantics *semantics,
				    struct ir_while_statement *statement)
{
	enum ir_data_type type =
		analyze_expression(semantics, statement->condition);
	if (type != IR_DATA_TYPE_BOOL)
		semantic_error(
			semantics,
			"Condition expression in while statement must be of type bool");

	analyze_block(semantics, statement->block, NULL);
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
	analyze_assignment(semantics, statement->initial);
	enum ir_data_type type =
		analyze_expression(semantics, statement->condition);
	if (type != IR_DATA_TYPE_BOOL)
		semantic_error(
			semantics,
			"Condition expression in for statement must be of type bool");

	analyze_for_update(semantics, statement->update);
	analyze_block(semantics, statement->block, NULL);
}

static void analyze_if_statement(struct semantics *semantics,
				 struct ir_if_statement *statement)
{
	enum ir_data_type type =
		analyze_expression(semantics, statement->condition);
	if (type != IR_DATA_TYPE_BOOL)
		semantic_error(
			semantics,
			"Condition expression in if statement must be of type bool");

	analyze_block(semantics, statement->if_block, NULL);
	if (statement->else_block != NULL)
		analyze_block(semantics, statement->else_block, NULL);
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
	default:
		break;
	}
}

static void analyze_field(struct semantics *semantics, struct ir_field *field);

static void analyze_block(struct semantics *semantics, struct ir_block *block,
			  GArray *initial_fields)
{
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
				semantics,
				"Inconsistent types in array initializer");
	}

	return type;
}

static void analyze_field(struct semantics *semantics, struct ir_field *field)
{
	declare_field(semantics, field);

	if (field->initializer != NULL) {
		enum ir_data_type type =
			analyze_initializer(semantics, field->initializer);

		if (field->initializer->array && !field->array)
			semantic_error(
				semantics,
				"Cannot initialize non-array field with array initializer");
		if (!field->initializer->array && field->array)
			semantic_error(
				semantics,
				"Cannot initialize array field with non-array initializer");
		if (field->initializer->array && field->array &&
		    field->array_length != -1)
			semantic_error(
				semantics,
				"Array initializer cannot be provided when length is declared");
		if (type != field->type)
			semantic_error(
				semantics,
				"Initializer type does not match field type");

		field->array_length = field->initializer->literals->len;
	} else if (field->array) {
		if (field->array_length == -1)
			semantic_error(
				semantics,
				"Array fields without initializers must have a declared length");
		else if (field->array_length == 0)
			semantic_error(semantics,
				       "Array length must be greater than 0");
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
