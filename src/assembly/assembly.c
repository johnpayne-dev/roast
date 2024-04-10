#include "assembly/assembly.h"

static void add_node(struct assembly *assembly, enum llir_node_type type,
		     void *data)
{
	struct llir_node *node = llir_node_new(type, data);

	if (assembly->head == NULL) {
		assembly->head = node;
		assembly->current = node;
		return;
	}

	assembly->current->next = node;
	assembly->current = node;
}

static void add_operation(struct assembly *assembly,
			  enum llir_operation_type type,
			  struct llir_operand source,
			  struct llir_operand destination)
{
	struct llir_operation *operation =
		llir_operation_new(type, source, destination);
	add_node(assembly, LLIR_NODE_TYPE_OPERATION, operation);
}

static struct llir_operand new_temporary(struct assembly *assembly)
{
	char *identifier =
		g_strdup_printf("$%u", assembly->temporary_variable_counter++);

	struct llir_field *field = llir_field_new(identifier, 0, false, 1);
	add_node(assembly, LLIR_NODE_TYPE_FIELD, field);

	symbol_table_set(assembly->symbol_table, field->identifier, field);

	g_free(identifier);
	return llir_operand_from_identifier(field->identifier);
}

static struct llir_label *new_label(struct assembly *assembly)
{
	return llir_label_new(assembly->label_counter++);
}

static void push_loop(struct assembly *assembly, struct llir_label *break_label,
		      struct llir_label *continue_label)
{
	g_array_append_val(assembly->break_labels, break_label);
	g_array_append_val(assembly->continue_labels, continue_label);
}

static struct llir_label *get_break_label(struct assembly *assembly)
{
	g_assert(assembly->break_labels->len > 0);
	return g_array_index(assembly->break_labels, struct llir_label *,
			     assembly->break_labels->len - 1);
}

static struct llir_label *get_continue_label(struct assembly *assembly)
{
	g_assert(assembly->continue_labels->len > 0);
	return g_array_index(assembly->continue_labels, struct llir_label *,
			     assembly->continue_labels->len - 1);
}

static void pop_loop(struct assembly *assembly)
{
	g_assert(assembly->break_labels->len > 0);
	g_assert(assembly->continue_labels->len > 0);
	g_array_remove_index(assembly->break_labels,
			     assembly->break_labels->len - 1);
	g_array_remove_index(assembly->continue_labels,
			     assembly->continue_labels->len - 1);
}

static void nodes_from_field(struct assembly *assembly,
			     struct ir_field *ir_field);
static struct llir_operand
nodes_from_expression(struct assembly *assembly,
		      struct ir_expression *ir_expression);
static void nodes_from_block(struct assembly *assembly,
			     struct ir_block *ir_block);

static void nodes_from_bounds_check(struct assembly *assembly,
				    struct llir_operand index, int64_t length)
{
	struct llir_label *label = new_label(assembly);

	struct llir_operand length_operand = llir_operand_from_literal(length);
	struct llir_branch *branch = llir_branch_new(
		LLIR_BRANCH_TYPE_LESS, true, index, length_operand, label);
	add_node(assembly, LLIR_NODE_TYPE_BRANCH, branch);

	struct llir_shit_yourself *exit = llir_shit_yourself_new(-1);
	add_node(assembly, LLIR_NODE_TYPE_SHIT_YOURSELF, exit);

	add_node(assembly, LLIR_NODE_TYPE_LABEL, label);
}

static struct llir_operand nodes_from_location(struct assembly *assembly,
					       struct ir_location *ir_location)
{
	struct llir_field *source_field = symbol_table_get(
		assembly->symbol_table, ir_location->identifier);
	g_assert(source_field != NULL);

	struct llir_operand source =
		llir_operand_from_identifier(source_field->identifier);
	struct llir_operand destination = new_temporary(assembly);

	if (ir_location->index == NULL) {
		add_operation(assembly, LLIR_OPERATION_TYPE_MOVE, source,
			      destination);
		return destination;
	}

	struct llir_operand array = new_temporary(assembly);
	add_operation(assembly, LLIR_OPERATION_TYPE_MOVE, source, array);

	struct llir_operand index =
		nodes_from_expression(assembly, ir_location->index);

	nodes_from_bounds_check(assembly, index, source_field->value_count);

	add_operation(assembly, LLIR_OPERATION_TYPE_MULTIPLY,
		      llir_operand_from_literal(8), index);
	add_operation(assembly, LLIR_OPERATION_TYPE_ADD, index, array);

	struct llir_operand dereference =
		llir_operand_from_dereference(array.identifier, 0);
	add_operation(assembly, LLIR_OPERATION_TYPE_MOVE, dereference,
		      destination);

	return destination;
}

static int64_t literal_to_int64(struct ir_literal *literal)
{
	const uint64_t MAX_INT64_POSSIBLE_MAGNITUDE = ((uint64_t)1)
						      << 63; // what the fuck?

	g_assert(literal->value <= MAX_INT64_POSSIBLE_MAGNITUDE);

	bool actually_negate = literal->negate &&
			       (literal->value != MAX_INT64_POSSIBLE_MAGNITUDE);

	int64_t value = (int64_t)literal->value * (actually_negate ? -1 : 1);
	return value;
}

static struct llir_operand nodes_from_literal(struct assembly *assembly,
					      struct ir_literal *literal)
{
	int64_t value = literal_to_int64(literal);

	struct llir_operand source = llir_operand_from_literal(value);
	struct llir_operand destination = new_temporary(assembly);
	add_operation(assembly, LLIR_OPERATION_TYPE_MOVE, source, destination);

	return destination;
}

static struct llir_operand
nodes_from_method_call(struct assembly *assembly,
		       struct ir_method_call *ir_method_call)
{
	struct llir_operand destination = new_temporary(assembly);
	struct llir_method_call *call = llir_method_call_new(
		ir_method_call->identifier, ir_method_call->arguments->len,
		destination);

	for (uint32_t i = 0; i < call->argument_count; i++) {
		struct ir_method_call_argument *ir_method_call_argument =
			g_array_index(ir_method_call->arguments,
				      struct ir_method_call_argument *, i);

		struct llir_operand argument;
		if (ir_method_call_argument->type ==
		    IR_METHOD_CALL_ARGUMENT_TYPE_STRING)
			argument = llir_operand_from_string(
				ir_method_call_argument->string);
		else
			argument = nodes_from_expression(
				assembly, ir_method_call_argument->expression);

		llir_method_call_set_argument(call, i, argument);
	}

	add_node(assembly, LLIR_NODE_TYPE_METHOD_CALL, call);
	return destination;
}

static struct llir_operand
nodes_from_len_expression(struct assembly *assembly,
			  struct ir_length_expression *length_expression)
{
	struct llir_operand source =
		llir_operand_from_literal(length_expression->length);
	struct llir_operand destination = new_temporary(assembly);
	add_operation(assembly, LLIR_OPERATION_TYPE_MOVE, source, destination);

	return destination;
}

static struct llir_operand
nodes_from_not_expression(struct assembly *assembly,
			  struct ir_expression *ir_expression)
{
	struct llir_operand source =
		nodes_from_expression(assembly, ir_expression);
	struct llir_operand destination = new_temporary(assembly);
	add_operation(assembly, LLIR_OPERATION_TYPE_NOT, source, destination);

	return destination;
}

static struct llir_operand
nodes_from_negate_expression(struct assembly *assembly,
			     struct ir_expression *ir_expression)
{
	struct llir_operand source =
		nodes_from_expression(assembly, ir_expression);
	struct llir_operand destination = new_temporary(assembly);
	add_operation(assembly, LLIR_OPERATION_TYPE_NEGATE, source,
		      destination);

	return destination;
}

static struct llir_operand
nodes_from_short_circuit(struct assembly *assembly,
			 struct ir_binary_expression *ir_binary_expression)
{
	g_assert(ir_binary_expression->binary_operator ==
			 IR_BINARY_OPERATOR_AND ||
		 ir_binary_expression->binary_operator ==
			 IR_BINARY_OPERATOR_OR);

	struct llir_operand destination =
		nodes_from_expression(assembly, ir_binary_expression->left);

	enum llir_branch_type comparison =
		ir_binary_expression->binary_operator == IR_BINARY_OPERATOR_OR ?
			LLIR_BRANCH_TYPE_EQUAL :
			LLIR_BRANCH_TYPE_NOT_EQUAL;
	struct llir_operand literal = llir_operand_from_literal(1);
	struct llir_label *label = new_label(assembly);

	struct llir_branch *branch =
		llir_branch_new(comparison, false, destination, literal, label);
	add_node(assembly, LLIR_NODE_TYPE_BRANCH, branch);

	struct llir_operand right =
		nodes_from_expression(assembly, ir_binary_expression->right);
	add_operation(assembly, LLIR_OPERATION_TYPE_MOVE, right, destination);

	add_node(assembly, LLIR_NODE_TYPE_LABEL, label);

	return destination;
}

static struct llir_operand
nodes_from_binary_expression(struct assembly *assembly,
			     struct ir_binary_expression *ir_binary_expression)
{
	if (ir_binary_expression->binary_operator == IR_BINARY_OPERATOR_OR ||
	    ir_binary_expression->binary_operator == IR_BINARY_OPERATOR_AND)
		return nodes_from_short_circuit(assembly, ir_binary_expression);

	struct llir_operand left =
		nodes_from_expression(assembly, ir_binary_expression->left);
	struct llir_operand right =
		nodes_from_expression(assembly, ir_binary_expression->right);

	static enum llir_operation_type IR_OPERATOR_TO_LLIR_OPERATION[] = {
		[IR_BINARY_OPERATOR_EQUAL] = LLIR_OPERATION_TYPE_EQUAL,
		[IR_BINARY_OPERATOR_NOT_EQUAL] = LLIR_OPERATION_TYPE_NOT_EQUAL,
		[IR_BINARY_OPERATOR_LESS] = LLIR_OPERATION_TYPE_LESS,
		[IR_BINARY_OPERATOR_LESS_EQUAL] =
			LLIR_OPERATION_TYPE_LESS_EQUAL,
		[IR_BINARY_OPERATOR_GREATER_EQUAL] =
			LLIR_OPERATION_TYPE_GREATER_EQUAL,
		[IR_BINARY_OPERATOR_GREATER] = LLIR_OPERATION_TYPE_GREATER,
		[IR_BINARY_OPERATOR_ADD] = LLIR_OPERATION_TYPE_ADD,
		[IR_BINARY_OPERATOR_SUB] = LLIR_OPERATION_TYPE_SUBTRACT,
		[IR_BINARY_OPERATOR_MUL] = LLIR_OPERATION_TYPE_MULTIPLY,
		[IR_BINARY_OPERATOR_DIV] = LLIR_OPERATION_TYPE_DIVIDE,
		[IR_BINARY_OPERATOR_MOD] = LLIR_OPERATION_TYPE_MODULO,
	};

	enum llir_operation_type operation =
		IR_OPERATOR_TO_LLIR_OPERATION[ir_binary_expression
						      ->binary_operator];
	add_operation(assembly, operation, right, left);
	return left;
}

static struct llir_operand
nodes_from_expression(struct assembly *assembly,
		      struct ir_expression *ir_expression)
{
	switch (ir_expression->type) {
	case IR_EXPRESSION_TYPE_BINARY:
		return nodes_from_binary_expression(
			assembly, ir_expression->binary_expression);
	case IR_EXPRESSION_TYPE_NOT:
		return nodes_from_not_expression(assembly,
						 ir_expression->not_expression);
	case IR_EXPRESSION_TYPE_NEGATE:
		return nodes_from_negate_expression(
			assembly, ir_expression->negate_expression);
	case IR_EXPRESSION_TYPE_LEN:
		return nodes_from_len_expression(
			assembly, ir_expression->length_expression);
	case IR_EXPRESSION_TYPE_METHOD_CALL:
		return nodes_from_method_call(assembly,
					      ir_expression->method_call);
	case IR_EXPRESSION_TYPE_LITERAL:
		return nodes_from_literal(assembly, ir_expression->literal);
	case IR_EXPRESSION_TYPE_LOCATION:
		return nodes_from_location(assembly, ir_expression->location);
	default:
		g_assert(!"you fucked up");
		return (struct llir_operand){ 0 };
	}
}

static void nodes_from_assignment(struct assembly *assembly,
				  struct ir_assignment *ir_assignment)
{
	struct llir_field *field = symbol_table_get(
		assembly->symbol_table, ir_assignment->location->identifier);
	struct llir_operand destination =
		llir_operand_from_identifier(field->identifier);

	if (ir_assignment->location->index != NULL) {
		struct llir_operand index = nodes_from_expression(
			assembly, ir_assignment->location->index);
		nodes_from_bounds_check(assembly, index, field->value_count);

		struct llir_operand array = new_temporary(assembly);
		add_operation(assembly, LLIR_OPERATION_TYPE_MOVE, destination,
			      array);
		add_operation(assembly, LLIR_OPERATION_TYPE_MULTIPLY,
			      llir_operand_from_literal(8), index);
		add_operation(assembly, LLIR_OPERATION_TYPE_ADD, index, array);

		destination =
			llir_operand_from_dereference(array.identifier, 0);
	}

	struct llir_operand source;
	if (ir_assignment->expression == NULL)
		source = llir_operand_from_literal(1);
	else
		source = nodes_from_expression(assembly,
					       ir_assignment->expression);

	static enum llir_operation_type OPERATION_TYPE[] = {
		[IR_ASSIGN_OPERATOR_SET] = LLIR_OPERATION_TYPE_MOVE,
		[IR_ASSIGN_OPERATOR_ADD] = LLIR_OPERATION_TYPE_ADD,
		[IR_ASSIGN_OPERATOR_SUB] = LLIR_OPERATION_TYPE_SUBTRACT,
		[IR_ASSIGN_OPERATOR_MUL] = LLIR_OPERATION_TYPE_MULTIPLY,
		[IR_ASSIGN_OPERATOR_DIV] = LLIR_OPERATION_TYPE_DIVIDE,
		[IR_ASSIGN_OPERATOR_MOD] = LLIR_OPERATION_TYPE_MODULO,
		[IR_ASSIGN_OPERATOR_INCREMENT] = LLIR_OPERATION_TYPE_ADD,
		[IR_ASSIGN_OPERATOR_DECREMENT] = LLIR_OPERATION_TYPE_SUBTRACT,
	};

	add_operation(assembly, OPERATION_TYPE[ir_assignment->assign_operator],
		      source, destination);
}

static void nodes_from_if_statement(struct assembly *assembly,
				    struct ir_if_statement *ir_if_statement)
{
	struct llir_operand expression =
		nodes_from_expression(assembly, ir_if_statement->condition);

	struct llir_label *end_if = new_label(assembly);
	struct llir_branch *branch =
		llir_branch_new(LLIR_BRANCH_TYPE_EQUAL, false, expression,
				llir_operand_from_literal(0), end_if);
	add_node(assembly, LLIR_NODE_TYPE_BRANCH, branch);
	nodes_from_block(assembly, ir_if_statement->if_block);

	if (ir_if_statement->else_block != NULL) {
		struct llir_label *end_else = new_label(assembly);
		struct llir_jump *jump = llir_jump_new(end_else);

		add_node(assembly, LLIR_NODE_TYPE_JUMP, jump);
		add_node(assembly, LLIR_NODE_TYPE_LABEL, end_if);
		nodes_from_block(assembly, ir_if_statement->else_block);
		add_node(assembly, LLIR_NODE_TYPE_LABEL, end_else);
	} else {
		add_node(assembly, LLIR_NODE_TYPE_LABEL, end_if);
	}
}

static void nodes_from_for_update(struct assembly *assembly,
				  struct ir_for_update *ir_for_update)
{
	switch (ir_for_update->type) {
	case IR_FOR_UPDATE_TYPE_ASSIGNMENT:
		nodes_from_assignment(assembly, ir_for_update->assignment);
		break;
	case IR_FOR_UPDATE_TYPE_METHOD_CALL:
		nodes_from_method_call(assembly, ir_for_update->method_call);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
}

static void nodes_from_for_statement(struct assembly *assembly,
				     struct ir_for_statement *ir_for_statement)
{
	nodes_from_assignment(assembly, ir_for_statement->initial);

	struct llir_label *start_label = new_label(assembly);
	struct llir_label *break_label = new_label(assembly);
	struct llir_label *continue_label = new_label(assembly);
	push_loop(assembly, break_label, continue_label);

	add_node(assembly, LLIR_NODE_TYPE_LABEL, start_label);
	struct llir_operand condition =
		nodes_from_expression(assembly, ir_for_statement->condition);

	struct llir_branch *branch =
		llir_branch_new(LLIR_BRANCH_TYPE_EQUAL, false, condition,
				llir_operand_from_literal(0), break_label);
	add_node(assembly, LLIR_NODE_TYPE_BRANCH, branch);

	nodes_from_block(assembly, ir_for_statement->block);
	add_node(assembly, LLIR_NODE_TYPE_LABEL, continue_label);
	nodes_from_for_update(assembly, ir_for_statement->update);

	struct llir_jump *jump = llir_jump_new(start_label);
	add_node(assembly, LLIR_NODE_TYPE_JUMP, jump);
	add_node(assembly, LLIR_NODE_TYPE_LABEL, break_label);

	pop_loop(assembly);
}

static void
nodes_from_while_statement(struct assembly *assembly,
			   struct ir_while_statement *ir_while_statement)
{
	struct llir_label *break_label = new_label(assembly);
	struct llir_label *continue_label = new_label(assembly);
	push_loop(assembly, break_label, continue_label);

	add_node(assembly, LLIR_NODE_TYPE_LABEL, continue_label);
	struct llir_operand condition =
		nodes_from_expression(assembly, ir_while_statement->condition);

	struct llir_branch *branch =
		llir_branch_new(LLIR_BRANCH_TYPE_EQUAL, false, condition,
				llir_operand_from_literal(0), break_label);
	add_node(assembly, LLIR_NODE_TYPE_BRANCH, branch);

	nodes_from_block(assembly, ir_while_statement->block);

	struct llir_jump *jump = llir_jump_new(continue_label);
	add_node(assembly, LLIR_NODE_TYPE_JUMP, jump);

	add_node(assembly, LLIR_NODE_TYPE_LABEL, break_label);

	pop_loop(assembly);
}

static void nodes_from_return_statement(struct assembly *assembly,
					struct ir_expression *ir_expression)
{
	struct llir_operand return_value;
	if (ir_expression == NULL)
		return_value = llir_operand_from_literal(0);
	else
		return_value = nodes_from_expression(assembly, ir_expression);

	struct llir_return *llir_return = llir_return_new(return_value);
	add_node(assembly, LLIR_NODE_TYPE_RETURN, llir_return);
}

static void nodes_from_break_statement(struct assembly *assembly)
{
	struct llir_label *label = get_break_label(assembly);
	struct llir_jump *jump = llir_jump_new(label);
	add_node(assembly, LLIR_NODE_TYPE_JUMP, jump);
}

static void nodes_from_continue_statement(struct assembly *assembly)
{
	struct llir_label *label = get_continue_label(assembly);
	struct llir_jump *jump = llir_jump_new(label);
	add_node(assembly, LLIR_NODE_TYPE_JUMP, jump);
}

static void nodes_from_statement(struct assembly *assembly,
				 struct ir_statement *ir_statement)
{
	switch (ir_statement->type) {
	case IR_STATEMENT_TYPE_ASSIGNMENT:
		nodes_from_assignment(assembly, ir_statement->assignment);
		break;
	case IR_STATEMENT_TYPE_METHOD_CALL:
		nodes_from_method_call(assembly, ir_statement->method_call);
		break;
	case IR_STATEMENT_TYPE_IF:
		nodes_from_if_statement(assembly, ir_statement->if_statement);
		break;
	case IR_STATEMENT_TYPE_FOR:
		nodes_from_for_statement(assembly, ir_statement->for_statement);
		break;
	case IR_STATEMENT_TYPE_WHILE:
		nodes_from_while_statement(assembly,
					   ir_statement->while_statement);
		break;
	case IR_STATEMENT_TYPE_RETURN:
		nodes_from_return_statement(assembly,
					    ir_statement->return_expression);
		break;
	case IR_STATEMENT_TYPE_BREAK:
		nodes_from_break_statement(assembly);
		break;
	case IR_STATEMENT_TYPE_CONTINUE:
		nodes_from_continue_statement(assembly);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
}

static void nodes_from_block(struct assembly *assembly,
			     struct ir_block *ir_block)
{
	symbol_table_push_scope(assembly->symbol_table);

	for (uint32_t i = 0; i < ir_block->fields->len; i++) {
		struct ir_field *ir_field =
			g_array_index(ir_block->fields, struct ir_field *, i);
		nodes_from_field(assembly, ir_field);
	}

	for (uint32_t i = 0; i < ir_block->statements->len; i++) {
		struct ir_statement *ir_statement = g_array_index(
			ir_block->statements, struct ir_statement *, i);
		nodes_from_statement(assembly, ir_statement);
	}

	symbol_table_pop_scope(assembly->symbol_table);
}

static void nodes_from_method(struct assembly *assembly,
			      struct ir_method *ir_method)
{
	g_assert(!ir_method->imported);

	symbol_table_push_scope(assembly->symbol_table);

	struct llir_method *method = llir_method_new(ir_method->identifier,
						     ir_method->arguments->len);

	for (uint32_t i = 0; i < ir_method->arguments->len; i++) {
		struct ir_field *ir_field = g_array_index(ir_method->arguments,
							  struct ir_field *, i);
		uint32_t scope_level =
			symbol_table_get_scope_level(assembly->symbol_table);
		struct llir_field *field =
			llir_field_new(ir_field->identifier, scope_level,
				       ir_data_type_is_array(ir_field->type),
				       ir_field->array_length);

		llir_method_set_argument(method, i, field);

		symbol_table_set(assembly->symbol_table, ir_field->identifier,
				 field);
	}

	add_node(assembly, LLIR_NODE_TYPE_METHOD, method);

	nodes_from_block(assembly, ir_method->block);

	if (ir_method->return_type == IR_DATA_TYPE_VOID) {
		struct llir_operand return_value = llir_operand_from_literal(0);
		struct llir_return *llir_return = llir_return_new(return_value);
		add_node(assembly, LLIR_NODE_TYPE_RETURN, llir_return);
	} else {
		struct llir_shit_yourself *exit = llir_shit_yourself_new(-2);
		add_node(assembly, LLIR_NODE_TYPE_SHIT_YOURSELF, exit);
	}

	symbol_table_pop_scope(assembly->symbol_table);
}

static void nodes_from_field(struct assembly *assembly,
			     struct ir_field *ir_field)
{
	uint32_t scope_level =
		symbol_table_get_scope_level(assembly->symbol_table);
	struct llir_field *field = llir_field_new(
		ir_field->identifier, scope_level,
		ir_data_type_is_array(ir_field->type), ir_field->array_length);

	if (ir_field->initializer != NULL) {
		for (uint32_t i = 0; i < ir_field->initializer->literals->len;
		     i++) {
			struct ir_literal *ir_literal =
				g_array_index(ir_field->initializer->literals,
					      struct ir_literal *, i);
			int64_t value = literal_to_int64(ir_literal);

			llir_field_set_value(field, i, value);
		}
	}

	add_node(assembly, LLIR_NODE_TYPE_FIELD, field);

	symbol_table_set(assembly->symbol_table, ir_field->identifier, field);
}

static void nodes_from_program(struct assembly *assembly,
			       struct ir_program *ir_program)
{
	for (uint32_t i = 0; i < ir_program->fields->len; i++) {
		struct ir_field *ir_field =
			g_array_index(ir_program->fields, struct ir_field *, i);
		nodes_from_field(assembly, ir_field);
	}

	for (uint32_t i = 0; i < ir_program->methods->len; i++) {
		struct ir_method *ir_method = g_array_index(
			ir_program->methods, struct ir_method *, i);
		nodes_from_method(assembly, ir_method);
	}
}

struct assembly *assembly_new(void)
{
	struct assembly *assembly = g_new(struct assembly, 1);

	assembly->break_labels =
		g_array_new(false, false, sizeof(struct llir_label *));
	assembly->continue_labels =
		g_array_new(false, false, sizeof(struct llir_label *));
	assembly->symbol_table = symbol_table_new();

	return assembly;
}

struct llir_node *assembly_generate_llir(struct assembly *assembly,
					 struct ir_program *ir)
{
	assembly->temporary_variable_counter = 0;
	assembly->label_counter = 0;
	assembly->current = NULL;
	assembly->head = NULL;

	nodes_from_program(assembly, ir);

	return assembly->head;
}

void assembly_free(struct assembly *assembly)
{
	g_array_free(assembly->break_labels, true);
	g_array_free(assembly->continue_labels, true);
	symbol_table_free(assembly->symbol_table);
	g_free(assembly);
}
