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

static void add_move(struct assembly *assembly, struct llir_operand source,
		     char *destination)
{
	struct llir_assignment *assignment = llir_assignment_new_unary(
		LLIR_ASSIGNMENT_TYPE_MOVE, source, destination);
	add_node(assembly, LLIR_NODE_TYPE_ASSIGNMENT, assignment);
}

static void add_array_update(struct assembly *assembly,
			     struct llir_operand index,
			     struct llir_operand value, char *destination)
{
	struct llir_assignment *assignment =
		llir_assignment_new_array_update(index, value, destination);
	add_node(assembly, LLIR_NODE_TYPE_ASSIGNMENT, assignment);
}

static void add_array_access(struct assembly *assembly,
			     struct llir_operand index, char *array,
			     char *destination)
{
	struct llir_assignment *assignment =
		llir_assignment_new_array_access(index, array, destination);
	add_node(assembly, LLIR_NODE_TYPE_ASSIGNMENT, assignment);
}

static void add_unary_assignment(struct assembly *assembly,
				 enum llir_assignment_type type,
				 struct llir_operand source, char *destination)
{
	struct llir_assignment *assignment =
		llir_assignment_new_unary(type, source, destination);
	add_node(assembly, LLIR_NODE_TYPE_ASSIGNMENT, assignment);
}

static void add_binary_assignment(struct assembly *assembly,
				  enum llir_assignment_type type,
				  struct llir_operand left,
				  struct llir_operand right, char *destination)
{
	struct llir_assignment *assignment =
		llir_assignment_new_binary(type, left, right, destination);
	add_node(assembly, LLIR_NODE_TYPE_ASSIGNMENT, assignment);
}

static char *new_temporary(struct assembly *assembly)
{
	char *identifier =
		g_strdup_printf("$%u", assembly->temporary_counter++);

	struct llir_field *field = llir_field_new(identifier, 0, false, 1);
	add_node(assembly, LLIR_NODE_TYPE_FIELD, field);

	symbol_table_set(assembly->symbol_table, identifier, field);

	g_free(identifier);
	return field->identifier;
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

static void get_field_initializer(struct ir_initializer *initializer,
				  int64_t *values)
{
	if (initializer == NULL)
		return;

	for (uint32_t i = 0; i < initializer->literals->len; i++) {
		struct ir_literal *ir_literal = g_array_index(
			initializer->literals, struct ir_literal *, i);

		values[i] = literal_to_int64(ir_literal);
	}
}

static char *nodes_from_field(struct assembly *assembly,
			      struct ir_field *ir_field, bool initialize);
static struct llir_operand
nodes_from_expression(struct assembly *assembly,
		      struct ir_expression *ir_expression);
static void nodes_from_block(struct assembly *assembly,
			     struct ir_block *ir_block, bool new_scope);

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
	struct llir_field *field = symbol_table_get(assembly->symbol_table,
						    ir_location->identifier);
	g_assert(field != NULL);

	struct llir_operand source = llir_operand_from_field(field->identifier);
	char *destination = new_temporary(assembly);

	if (ir_location->index == NULL) {
		add_move(assembly, source, destination);
	} else {
		struct llir_operand index =
			nodes_from_expression(assembly, ir_location->index);
		add_array_access(assembly, index, field->identifier,
				 destination);
	}

	return llir_operand_from_field(destination);
}

static struct llir_operand nodes_from_literal(struct assembly *assembly,
					      struct ir_literal *literal)
{
	int64_t value = literal_to_int64(literal);
	struct llir_operand source = llir_operand_from_literal(value);
	char *destination = new_temporary(assembly);

	add_move(assembly, source, destination);
	return llir_operand_from_field(destination);
}

static struct llir_operand
nodes_from_method_call(struct assembly *assembly,
		       struct ir_method_call *ir_method_call)
{
	char *destination = new_temporary(assembly);

	struct llir_assignment *call = llir_assignment_new_method_call(
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

		call->arguments[i] = argument;
	}

	add_node(assembly, LLIR_NODE_TYPE_ASSIGNMENT, call);
	return llir_operand_from_field(destination);
}

static struct llir_operand
nodes_from_len_expression(struct assembly *assembly,
			  struct ir_length_expression *length_expression)
{
	struct llir_operand source =
		llir_operand_from_literal(length_expression->length);
	char *destination = new_temporary(assembly);

	add_move(assembly, source, destination);
	return llir_operand_from_field(destination);
}

static struct llir_operand
nodes_from_not_expression(struct assembly *assembly,
			  struct ir_expression *ir_expression)
{
	struct llir_operand source =
		nodes_from_expression(assembly, ir_expression);
	char *destination = new_temporary(assembly);

	add_unary_assignment(assembly, LLIR_ASSIGNMENT_TYPE_NOT, source,
			     destination);
	return llir_operand_from_field(destination);
}

static struct llir_operand
nodes_from_negate_expression(struct assembly *assembly,
			     struct ir_expression *ir_expression)
{
	struct llir_operand source =
		nodes_from_expression(assembly, ir_expression);
	char *destination = new_temporary(assembly);

	add_unary_assignment(assembly, LLIR_ASSIGNMENT_TYPE_NEGATE, source,
			     destination);
	return llir_operand_from_field(destination);
}

static struct llir_operand
nodes_from_short_circuit(struct assembly *assembly,
			 struct ir_binary_expression *ir_binary_expression)
{
	g_assert(ir_binary_expression->binary_operator ==
			 IR_BINARY_OPERATOR_AND ||
		 ir_binary_expression->binary_operator ==
			 IR_BINARY_OPERATOR_OR);

	char *destination = new_temporary(assembly);

	struct llir_operand left =
		nodes_from_expression(assembly, ir_binary_expression->left);
	add_move(assembly, left, destination);

	enum llir_branch_type comparison =
		ir_binary_expression->binary_operator == IR_BINARY_OPERATOR_OR ?
			LLIR_BRANCH_TYPE_EQUAL :
			LLIR_BRANCH_TYPE_NOT_EQUAL;
	struct llir_operand literal = llir_operand_from_literal(1);
	struct llir_label *label = new_label(assembly);

	struct llir_branch *branch =
		llir_branch_new(comparison, false, left, literal, label);
	add_node(assembly, LLIR_NODE_TYPE_BRANCH, branch);

	struct llir_operand right =
		nodes_from_expression(assembly, ir_binary_expression->right);
	add_move(assembly, right, destination);

	add_node(assembly, LLIR_NODE_TYPE_LABEL, label);

	return llir_operand_from_field(destination);
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
	char *destination = new_temporary(assembly);

	static enum llir_assignment_type IR_OPERATOR_TO_ASSIGNMENT_TYPE[] = {
		[IR_BINARY_OPERATOR_EQUAL] = LLIR_ASSIGNMENT_TYPE_EQUAL,
		[IR_BINARY_OPERATOR_NOT_EQUAL] = LLIR_ASSIGNMENT_TYPE_NOT_EQUAL,
		[IR_BINARY_OPERATOR_LESS] = LLIR_ASSIGNMENT_TYPE_LESS,
		[IR_BINARY_OPERATOR_LESS_EQUAL] =
			LLIR_ASSIGNMENT_TYPE_LESS_EQUAL,
		[IR_BINARY_OPERATOR_GREATER_EQUAL] =
			LLIR_ASSIGNMENT_TYPE_GREATER_EQUAL,
		[IR_BINARY_OPERATOR_GREATER] = LLIR_ASSIGNMENT_TYPE_GREATER,
		[IR_BINARY_OPERATOR_ADD] = LLIR_ASSIGNMENT_TYPE_ADD,
		[IR_BINARY_OPERATOR_SUB] = LLIR_ASSIGNMENT_TYPE_SUBTRACT,
		[IR_BINARY_OPERATOR_MUL] = LLIR_ASSIGNMENT_TYPE_MULTIPLY,
		[IR_BINARY_OPERATOR_DIV] = LLIR_ASSIGNMENT_TYPE_DIVIDE,
		[IR_BINARY_OPERATOR_MOD] = LLIR_ASSIGNMENT_TYPE_MODULO,
	};
	enum llir_assignment_type operation =
		IR_OPERATOR_TO_ASSIGNMENT_TYPE[ir_binary_expression
						       ->binary_operator];

	add_binary_assignment(assembly, operation, left, right, destination);
	return llir_operand_from_field(destination);
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
	char *destination = field->identifier;

	struct llir_operand index;
	if (ir_assignment->location->index != NULL) {
		index = nodes_from_expression(assembly,
					      ir_assignment->location->index);
		nodes_from_bounds_check(assembly, index, field->value_count);
	}

	struct llir_operand source;
	if (ir_assignment->expression == NULL)
		source = llir_operand_from_literal(1);
	else
		source = nodes_from_expression(assembly,
					       ir_assignment->expression);

	if (ir_assignment->assign_operator == IR_ASSIGN_OPERATOR_SET) {
		if (ir_assignment->location->index != NULL)
			add_array_update(assembly, index, source, destination);
		else
			add_move(assembly, source, destination);
		return;
	}

	char *left_destination = new_temporary(assembly);
	if (ir_assignment->location->index != NULL)
		add_array_access(assembly, index, destination,
				 left_destination);
	else
		add_move(assembly, llir_operand_from_field(destination),
			 left_destination);

	static enum llir_assignment_type ASSIGNMENT_TYPE[] = {
		[IR_ASSIGN_OPERATOR_ADD] = LLIR_ASSIGNMENT_TYPE_ADD,
		[IR_ASSIGN_OPERATOR_SUB] = LLIR_ASSIGNMENT_TYPE_SUBTRACT,
		[IR_ASSIGN_OPERATOR_MUL] = LLIR_ASSIGNMENT_TYPE_MULTIPLY,
		[IR_ASSIGN_OPERATOR_DIV] = LLIR_ASSIGNMENT_TYPE_DIVIDE,
		[IR_ASSIGN_OPERATOR_MOD] = LLIR_ASSIGNMENT_TYPE_MODULO,
		[IR_ASSIGN_OPERATOR_INCREMENT] = LLIR_ASSIGNMENT_TYPE_ADD,
		[IR_ASSIGN_OPERATOR_DECREMENT] = LLIR_ASSIGNMENT_TYPE_SUBTRACT,
	};

	char *binary_destination = new_temporary(assembly);
	add_binary_assignment(assembly,
			      ASSIGNMENT_TYPE[ir_assignment->assign_operator],
			      llir_operand_from_field(left_destination), source,
			      binary_destination);

	if (ir_assignment->location->index != NULL)
		add_array_update(assembly, index,
				 llir_operand_from_field(binary_destination),
				 destination);
	else
		add_move(assembly, llir_operand_from_field(binary_destination),
			 destination);
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
	nodes_from_block(assembly, ir_if_statement->if_block, true);

	if (ir_if_statement->else_block != NULL) {
		struct llir_label *end_else = new_label(assembly);
		struct llir_jump *jump = llir_jump_new(end_else);

		add_node(assembly, LLIR_NODE_TYPE_JUMP, jump);
		add_node(assembly, LLIR_NODE_TYPE_LABEL, end_if);
		nodes_from_block(assembly, ir_if_statement->else_block, true);
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

	nodes_from_block(assembly, ir_for_statement->block, true);
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

	nodes_from_block(assembly, ir_while_statement->block, true);

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
			     struct ir_block *ir_block, bool new_scope)
{
	if (new_scope)
		symbol_table_push_scope(assembly->symbol_table);

	for (uint32_t i = 0; i < ir_block->fields->len; i++) {
		struct ir_field *ir_field =
			g_array_index(ir_block->fields, struct ir_field *, i);
		nodes_from_field(assembly, ir_field, true);
	}

	for (uint32_t i = 0; i < ir_block->statements->len; i++) {
		struct ir_statement *ir_statement = g_array_index(
			ir_block->statements, struct ir_statement *, i);
		nodes_from_statement(assembly, ir_statement);
	}

	if (new_scope)
		symbol_table_pop_scope(assembly->symbol_table);
}

static void nodes_from_method(struct assembly *assembly,
			      struct ir_method *ir_method)
{
	symbol_table_push_scope(assembly->symbol_table);

	struct llir_method *method = llir_method_new(ir_method->identifier,
						     ir_method->arguments->len);
	add_node(assembly, LLIR_NODE_TYPE_METHOD, method);

	for (uint32_t i = 0; i < ir_method->arguments->len; i++) {
		struct ir_field *ir_field = g_array_index(ir_method->arguments,
							  struct ir_field *, i);

		char *field = nodes_from_field(assembly, ir_field, false);
		method->arguments[i] = field;
	}

	nodes_from_block(assembly, ir_method->block, false);

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

static void nodes_from_field_initialization(struct assembly *assembly,
					    struct llir_field *field)
{
	if (!field->is_array) {
		struct llir_operand source =
			llir_operand_from_literal(field->values[0]);
		add_move(assembly, source, field->identifier);
		return;
	}

	for (uint32_t i = 0; i < field->value_count; i++) {
		struct llir_operand source =
			llir_operand_from_literal(field->values[i]);
		struct llir_operand index = llir_operand_from_literal(i);
		add_array_update(assembly, index, source, field->identifier);
	}
}

static char *nodes_from_field(struct assembly *assembly,
			      struct ir_field *ir_field, bool initialize)
{
	uint32_t scope_level =
		symbol_table_get_scope_level(assembly->symbol_table);
	struct llir_field *field = llir_field_new(
		ir_field->identifier, scope_level,
		ir_data_type_is_array(ir_field->type), ir_field->array_length);
	get_field_initializer(ir_field->initializer, field->values);

	add_node(assembly, LLIR_NODE_TYPE_FIELD, field);
	symbol_table_set(assembly->symbol_table, ir_field->identifier, field);

	if (initialize)
		nodes_from_field_initialization(assembly, field);

	return field->identifier;
}

static void nodes_from_program(struct assembly *assembly,
			       struct ir_program *ir_program)
{
	for (uint32_t i = 0; i < ir_program->fields->len; i++) {
		struct ir_field *ir_field =
			g_array_index(ir_program->fields, struct ir_field *, i);
		nodes_from_field(assembly, ir_field, false);
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
	assembly->temporary_counter = 0;
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
