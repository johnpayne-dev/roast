#include "assembly/llir_generator.h"

static void add_move(struct llir_generator *assembly,
		     struct llir_operand source, char *destination)
{
	struct llir_assignment *assignment = llir_assignment_new_unary(
		LLIR_ASSIGNMENT_TYPE_MOVE, source, destination);
	llir_block_add_assignment(assembly->current_block, assignment);
}

static void add_array_update(struct llir_generator *assembly,
			     struct llir_operand index,
			     struct llir_operand value, char *destination)
{
	struct llir_assignment *assignment =
		llir_assignment_new_array_update(index, value, destination);
	llir_block_add_assignment(assembly->current_block, assignment);
}

static void add_array_access(struct llir_generator *assembly,
			     struct llir_operand index, char *array,
			     char *destination)
{
	struct llir_assignment *assignment =
		llir_assignment_new_array_access(index, array, destination);
	llir_block_add_assignment(assembly->current_block, assignment);
}

static void add_unary_assignment(struct llir_generator *assembly,
				 enum llir_assignment_type type,
				 struct llir_operand source, char *destination)
{
	struct llir_assignment *assignment =
		llir_assignment_new_unary(type, source, destination);
	llir_block_add_assignment(assembly->current_block, assignment);
}

static void add_binary_assignment(struct llir_generator *assembly,
				  enum llir_assignment_type type,
				  struct llir_operand left,
				  struct llir_operand right, char *destination)
{
	struct llir_assignment *assignment =
		llir_assignment_new_binary(type, left, right, destination);
	llir_block_add_assignment(assembly->current_block, assignment);
}

static char *new_temporary(struct llir_generator *assembly)
{
	char *identifier =
		g_strdup_printf("$%u", assembly->temporary_counter++);

	struct llir_field *field = llir_field_new(identifier, 0, false, 1);
	llir_block_add_field(assembly->current_block, field);

	symbol_table_set(assembly->symbol_table, identifier, field);

	g_free(identifier);
	return field->identifier;
}

static struct llir_block *new_block(struct llir_generator *assembly)
{
	return llir_block_new(assembly->block_counter++);
}

static void next_block(struct llir_generator *assembly,
		       struct llir_block *block)
{
	llir_method_add_block(assembly->current_method,
			      assembly->current_block);
	assembly->current_block = block;
}

static void push_loop(struct llir_generator *assembly,
		      struct llir_block *break_block,
		      struct llir_block *continue_block)
{
	g_array_append_val(assembly->break_blocks, break_block);
	g_array_append_val(assembly->continue_blocks, continue_block);
}

static struct llir_block *get_break_block(struct llir_generator *assembly)
{
	g_assert(assembly->break_blocks->len > 0);
	return g_array_index(assembly->break_blocks, struct llir_block *,
			     assembly->break_blocks->len - 1);
}

static struct llir_block *get_continue_block(struct llir_generator *assembly)
{
	g_assert(assembly->continue_blocks->len > 0);
	return g_array_index(assembly->continue_blocks, struct llir_block *,
			     assembly->continue_blocks->len - 1);
}

static void pop_loop(struct llir_generator *assembly)
{
	g_assert(assembly->break_blocks->len > 0);
	g_assert(assembly->continue_blocks->len > 0);
	g_array_remove_index(assembly->break_blocks,
			     assembly->break_blocks->len - 1);
	g_array_remove_index(assembly->continue_blocks,
			     assembly->continue_blocks->len - 1);
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

static struct llir_field *generate_field(struct llir_generator *assembly,
					 struct ir_field *ir_field);
static void generate_block(struct llir_generator *assembly,
			   struct ir_block *ir_block, bool new_scope);
static struct llir_operand
generate_expression(struct llir_generator *assembly,
		    struct ir_expression *ir_expression);

static void generate_bounds_check(struct llir_generator *assembly,
				  struct llir_operand index, int64_t length)
{
	struct llir_block *true_block = new_block(assembly);
	struct llir_block *false_block = new_block(assembly);

	struct llir_operand length_operand = llir_operand_from_literal(length);

	struct llir_branch *branch =
		llir_branch_new(LLIR_BRANCH_TYPE_LESS, true, index,
				length_operand, true_block, false_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_BRANCH, branch);
	next_block(assembly, true_block);

	struct llir_shit_yourself *exit = llir_shit_yourself_new(-1);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_SHIT_YOURSELF, exit);
	next_block(assembly, false_block);
}

static struct llir_operand generate_location(struct llir_generator *assembly,
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
			generate_expression(assembly, ir_location->index);
		add_array_access(assembly, index, field->identifier,
				 destination);
	}

	return llir_operand_from_field(destination);
}

static struct llir_operand generate_literal(struct llir_generator *assembly,
					    struct ir_literal *literal)
{
	int64_t value = literal_to_int64(literal);
	struct llir_operand source = llir_operand_from_literal(value);
	char *destination = new_temporary(assembly);

	add_move(assembly, source, destination);
	return llir_operand_from_field(destination);
}

static struct llir_operand
generate_method_call(struct llir_generator *assembly,
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
			argument = generate_expression(
				assembly, ir_method_call_argument->expression);

		call->arguments[i] = argument;
	}

	llir_block_add_assignment(assembly->current_block, call);
	return llir_operand_from_field(destination);
}

static struct llir_operand
generate_len_expression(struct llir_generator *assembly,
			struct ir_length_expression *length_expression)
{
	struct llir_operand source =
		llir_operand_from_literal(length_expression->length);
	char *destination = new_temporary(assembly);

	add_move(assembly, source, destination);
	return llir_operand_from_field(destination);
}

static struct llir_operand
generate_not_expression(struct llir_generator *assembly,
			struct ir_expression *ir_expression)
{
	struct llir_operand source =
		generate_expression(assembly, ir_expression);
	char *destination = new_temporary(assembly);

	add_unary_assignment(assembly, LLIR_ASSIGNMENT_TYPE_NOT, source,
			     destination);
	return llir_operand_from_field(destination);
}

static struct llir_operand
generate_negate_expression(struct llir_generator *assembly,
			   struct ir_expression *ir_expression)
{
	struct llir_operand source =
		generate_expression(assembly, ir_expression);
	char *destination = new_temporary(assembly);

	add_unary_assignment(assembly, LLIR_ASSIGNMENT_TYPE_NEGATE, source,
			     destination);
	return llir_operand_from_field(destination);
}

static struct llir_operand
generate_short_circuit(struct llir_generator *assembly,
		       struct ir_binary_expression *ir_binary_expression)
{
	g_assert(ir_binary_expression->binary_operator ==
			 IR_BINARY_OPERATOR_AND ||
		 ir_binary_expression->binary_operator ==
			 IR_BINARY_OPERATOR_OR);

	char *destination = new_temporary(assembly);

	struct llir_operand left =
		generate_expression(assembly, ir_binary_expression->left);
	add_move(assembly, left, destination);

	enum llir_branch_type comparison =
		ir_binary_expression->binary_operator == IR_BINARY_OPERATOR_OR ?
			LLIR_BRANCH_TYPE_EQUAL :
			LLIR_BRANCH_TYPE_NOT_EQUAL;
	struct llir_operand literal = llir_operand_from_literal(1);

	struct llir_block *true_block = new_block(assembly);
	struct llir_block *false_block = new_block(assembly);

	struct llir_branch *branch = llir_branch_new(
		comparison, false, left, literal, true_block, false_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_BRANCH, branch);
	next_block(assembly, true_block);

	struct llir_operand right =
		generate_expression(assembly, ir_binary_expression->right);
	add_move(assembly, right, destination);

	struct llir_jump *jump = llir_jump_new(false_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
	next_block(assembly, false_block);

	return llir_operand_from_field(destination);
}

static struct llir_operand
generate_binary_expression(struct llir_generator *assembly,
			   struct ir_binary_expression *ir_binary_expression)
{
	if (ir_binary_expression->binary_operator == IR_BINARY_OPERATOR_OR ||
	    ir_binary_expression->binary_operator == IR_BINARY_OPERATOR_AND)
		return generate_short_circuit(assembly, ir_binary_expression);

	struct llir_operand left =
		generate_expression(assembly, ir_binary_expression->left);
	struct llir_operand right =
		generate_expression(assembly, ir_binary_expression->right);
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
generate_expression(struct llir_generator *assembly,
		    struct ir_expression *ir_expression)
{
	switch (ir_expression->type) {
	case IR_EXPRESSION_TYPE_BINARY:
		return generate_binary_expression(
			assembly, ir_expression->binary_expression);
	case IR_EXPRESSION_TYPE_NOT:
		return generate_not_expression(assembly,
					       ir_expression->not_expression);
	case IR_EXPRESSION_TYPE_NEGATE:
		return generate_negate_expression(
			assembly, ir_expression->negate_expression);
	case IR_EXPRESSION_TYPE_LEN:
		return generate_len_expression(
			assembly, ir_expression->length_expression);
	case IR_EXPRESSION_TYPE_METHOD_CALL:
		return generate_method_call(assembly,
					    ir_expression->method_call);
	case IR_EXPRESSION_TYPE_LITERAL:
		return generate_literal(assembly, ir_expression->literal);
	case IR_EXPRESSION_TYPE_LOCATION:
		return generate_location(assembly, ir_expression->location);
	default:
		g_assert(!"you fucked up");
		return (struct llir_operand){ 0 };
	}
}

static void generate_assignment(struct llir_generator *assembly,
				struct ir_assignment *ir_assignment)
{
	struct llir_field *field = symbol_table_get(
		assembly->symbol_table, ir_assignment->location->identifier);
	char *destination = field->identifier;

	struct llir_operand index;
	if (ir_assignment->location->index != NULL) {
		index = generate_expression(assembly,
					    ir_assignment->location->index);
		generate_bounds_check(assembly, index, field->value_count);
	}

	struct llir_operand source;
	if (ir_assignment->expression == NULL)
		source = llir_operand_from_literal(1);
	else
		source = generate_expression(assembly,
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

static void generate_if_statement(struct llir_generator *assembly,
				  struct ir_if_statement *ir_if_statement)
{
	struct llir_operand expression =
		generate_expression(assembly, ir_if_statement->condition);

	struct llir_block *true_block = new_block(assembly);
	struct llir_block *false_block = new_block(assembly);

	struct llir_branch *branch = llir_branch_new(
		LLIR_BRANCH_TYPE_EQUAL, false, expression,
		llir_operand_from_literal(0), true_block, false_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_BRANCH, branch);
	next_block(assembly, true_block);

	generate_block(assembly, ir_if_statement->if_block, true);

	if (ir_if_statement->else_block == NULL) {
		struct llir_jump *jump = llir_jump_new(false_block);
		llir_block_set_terminal(assembly->current_block,
					LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
		next_block(assembly, false_block);
	} else {
		struct llir_block *end_block = new_block(assembly);

		struct llir_jump *jump = llir_jump_new(end_block);
		llir_block_set_terminal(assembly->current_block,
					LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
		next_block(assembly, false_block);

		generate_block(assembly, ir_if_statement->else_block, true);

		jump = llir_jump_new(end_block);
		llir_block_set_terminal(assembly->current_block,
					LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
		next_block(assembly, end_block);
	}
}

static void generate_for_update(struct llir_generator *assembly,
				struct ir_for_update *ir_for_update)
{
	switch (ir_for_update->type) {
	case IR_FOR_UPDATE_TYPE_ASSIGNMENT:
		generate_assignment(assembly, ir_for_update->assignment);
		break;
	case IR_FOR_UPDATE_TYPE_METHOD_CALL:
		generate_method_call(assembly, ir_for_update->method_call);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
}

static void generate_for_statement(struct llir_generator *assembly,
				   struct ir_for_statement *ir_for_statement)
{
	generate_assignment(assembly, ir_for_statement->initial);

	struct llir_block *condition_block = new_block(assembly);
	struct llir_block *loop_block = new_block(assembly);
	struct llir_block *update_block = new_block(assembly);
	struct llir_block *end_block = new_block(assembly);
	push_loop(assembly, end_block, update_block);

	struct llir_jump *jump = llir_jump_new(condition_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
	next_block(assembly, condition_block);

	struct llir_operand condition =
		generate_expression(assembly, ir_for_statement->condition);

	struct llir_branch *branch = llir_branch_new(
		LLIR_BRANCH_TYPE_EQUAL, false, condition,
		llir_operand_from_literal(0), loop_block, end_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_BRANCH, branch);
	next_block(assembly, loop_block);

	generate_block(assembly, ir_for_statement->block, true);

	jump = llir_jump_new(update_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
	next_block(assembly, update_block);

	generate_for_update(assembly, ir_for_statement->update);

	jump = llir_jump_new(condition_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
	next_block(assembly, end_block);

	pop_loop(assembly);
}

static void
generate_while_statement(struct llir_generator *assembly,
			 struct ir_while_statement *ir_while_statement)
{
	struct llir_block *condition_block = new_block(assembly);
	struct llir_block *loop_block = new_block(assembly);
	struct llir_block *end_block = new_block(assembly);
	push_loop(assembly, end_block, condition_block);

	struct llir_jump *jump = llir_jump_new(condition_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
	next_block(assembly, condition_block);

	struct llir_operand condition =
		generate_expression(assembly, ir_while_statement->condition);

	struct llir_branch *branch = llir_branch_new(
		LLIR_BRANCH_TYPE_EQUAL, false, condition,
		llir_operand_from_literal(0), loop_block, end_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_BRANCH, branch);
	next_block(assembly, loop_block);

	generate_block(assembly, ir_while_statement->block, true);

	jump = llir_jump_new(condition_block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
	next_block(assembly, end_block);

	pop_loop(assembly);
}

static void generate_return_statement(struct llir_generator *assembly,
				      struct ir_expression *ir_expression)
{
	struct llir_operand return_value;
	if (ir_expression == NULL)
		return_value = llir_operand_from_literal(0);
	else
		return_value = generate_expression(assembly, ir_expression);

	struct llir_return *llir_return = llir_return_new(return_value);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_RETURN, llir_return);
	next_block(assembly, new_block(assembly));
}

static void generate_break_statement(struct llir_generator *assembly)
{
	struct llir_block *block = get_break_block(assembly);
	struct llir_jump *jump = llir_jump_new(block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
	next_block(assembly, new_block(assembly));
}

static void generate_continue_statement(struct llir_generator *assembly)
{
	struct llir_block *block = get_continue_block(assembly);
	struct llir_jump *jump = llir_jump_new(block);
	llir_block_set_terminal(assembly->current_block,
				LLIR_BLOCK_TERMINAL_TYPE_JUMP, jump);
	next_block(assembly, new_block(assembly));
}

static void generate_statement(struct llir_generator *assembly,
			       struct ir_statement *ir_statement)
{
	switch (ir_statement->type) {
	case IR_STATEMENT_TYPE_ASSIGNMENT:
		generate_assignment(assembly, ir_statement->assignment);
		break;
	case IR_STATEMENT_TYPE_METHOD_CALL:
		generate_method_call(assembly, ir_statement->method_call);
		break;
	case IR_STATEMENT_TYPE_IF:
		generate_if_statement(assembly, ir_statement->if_statement);
		break;
	case IR_STATEMENT_TYPE_FOR:
		generate_for_statement(assembly, ir_statement->for_statement);
		break;
	case IR_STATEMENT_TYPE_WHILE:
		generate_while_statement(assembly,
					 ir_statement->while_statement);
		break;
	case IR_STATEMENT_TYPE_RETURN:
		generate_return_statement(assembly,
					  ir_statement->return_expression);
		break;
	case IR_STATEMENT_TYPE_BREAK:
		generate_break_statement(assembly);
		break;
	case IR_STATEMENT_TYPE_CONTINUE:
		generate_continue_statement(assembly);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
}

static void generate_field_initialization(struct llir_generator *assembly,
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

static void generate_block(struct llir_generator *assembly,
			   struct ir_block *ir_block, bool new_scope)
{
	if (new_scope)
		symbol_table_push_scope(assembly->symbol_table);

	for (uint32_t i = 0; i < ir_block->fields->len; i++) {
		struct ir_field *ir_field =
			g_array_index(ir_block->fields, struct ir_field *, i);

		struct llir_field *field = generate_field(assembly, ir_field);
		llir_block_add_field(assembly->current_block, field);
		generate_field_initialization(assembly, field);
	}

	for (uint32_t i = 0; i < ir_block->statements->len; i++) {
		struct ir_statement *ir_statement = g_array_index(
			ir_block->statements, struct ir_statement *, i);
		generate_statement(assembly, ir_statement);
	}

	if (new_scope)
		symbol_table_pop_scope(assembly->symbol_table);
}

static struct llir_method *generate_method(struct llir_generator *assembly,
					   struct ir_method *ir_method)
{
	symbol_table_push_scope(assembly->symbol_table);

	struct llir_method *method = llir_method_new(ir_method->identifier);

	for (uint32_t i = 0; i < ir_method->arguments->len; i++) {
		struct ir_field *ir_field = g_array_index(ir_method->arguments,
							  struct ir_field *, i);

		struct llir_field *field = generate_field(assembly, ir_field);
		llir_method_add_argument(method, field);
	}

	assembly->current_method = method;
	assembly->current_block = new_block(assembly);

	generate_block(assembly, ir_method->block, false);

	if (ir_method->return_type == IR_DATA_TYPE_VOID) {
		struct llir_operand return_value = llir_operand_from_literal(0);
		struct llir_return *llir_return = llir_return_new(return_value);
		llir_block_set_terminal(assembly->current_block,
					LLIR_BLOCK_TERMINAL_TYPE_RETURN,
					llir_return);
		llir_method_add_block(method, assembly->current_block);
	} else {
		struct llir_shit_yourself *exit = llir_shit_yourself_new(-2);
		llir_block_set_terminal(assembly->current_block,
					LLIR_BLOCK_TERMINAL_TYPE_SHIT_YOURSELF,
					exit);
		llir_method_add_block(method, assembly->current_block);
	}

	symbol_table_pop_scope(assembly->symbol_table);
	return method;
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

static struct llir_field *generate_field(struct llir_generator *assembly,
					 struct ir_field *ir_field)
{
	uint32_t scope_level =
		symbol_table_get_scope_level(assembly->symbol_table);
	struct llir_field *field = llir_field_new(
		ir_field->identifier, scope_level,
		ir_data_type_is_array(ir_field->type), ir_field->array_length);
	get_field_initializer(ir_field->initializer, field->values);

	symbol_table_set(assembly->symbol_table, ir_field->identifier, field);
	return field;
}

static struct llir *generate_llir(struct llir_generator *assembly,
				  struct ir_program *ir_program)
{
	struct llir *llir = llir_new();

	for (uint32_t i = 0; i < ir_program->fields->len; i++) {
		struct ir_field *ir_field =
			g_array_index(ir_program->fields, struct ir_field *, i);

		struct llir_field *field = generate_field(assembly, ir_field);
		llir_add_field(llir, field);
	}

	for (uint32_t i = 0; i < ir_program->methods->len; i++) {
		struct ir_method *ir_method = g_array_index(
			ir_program->methods, struct ir_method *, i);

		struct llir_method *method =
			generate_method(assembly, ir_method);
		llir_add_method(llir, method);
	}

	return llir;
}

struct llir_generator *llir_generator_new(void)
{
	struct llir_generator *assembly = g_new(struct llir_generator, 1);

	assembly->break_blocks =
		g_array_new(false, false, sizeof(struct llir_block *));
	assembly->continue_blocks =
		g_array_new(false, false, sizeof(struct llir_block *));
	assembly->symbol_table = symbol_table_new();

	return assembly;
}

struct llir *llir_generator_generate_llir(struct llir_generator *assembly,
					  struct ir_program *ir)
{
	assembly->temporary_counter = 0;
	assembly->block_counter = 0;
	return generate_llir(assembly, ir);
}

void llir_generator_free(struct llir_generator *assembly)
{
	g_array_free(assembly->break_blocks, true);
	g_array_free(assembly->continue_blocks, true);
	symbol_table_free(assembly->symbol_table);
	g_free(assembly);
}
