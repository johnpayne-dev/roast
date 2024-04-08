#include "assembly/assembly.h"

static const char *ARGUMENT_REGISTERS[] = { "rdi", "rsi", "rdx",
					    "rcx", "r8",  "r9" };

struct symbol_info {
	int32_t offset;
	int32_t size;
};

static void push_scope(struct code_generator *generator)
{
	GHashTable *table = g_hash_table_new(g_str_hash, g_str_equal);
	g_array_append_val(generator->symbol_tables, table);
}

static struct symbol_info *push_field(struct code_generator *generator,
				      char *identifier, uint32_t length)
{
	GHashTable *table = g_array_index(generator->symbol_tables,
					  GHashTable *,
					  generator->symbol_tables->len - 1);

	struct symbol_info *info = g_new(struct symbol_info, 1);
	info->offset = generator->stack_pointer;
	info->size = 8 * (length + length % 2);
	g_hash_table_insert(table, identifier, info);

	generator->stack_pointer += info->size;
	return info;
}

static int32_t get_field_offset(struct code_generator *generator,
				char *identifier)
{
	for (int32_t i = generator->symbol_tables->len - 1; i >= 0; i--) {
		GHashTable *table = g_array_index(generator->symbol_tables,
						  GHashTable *, i);

		struct symbol_info *info =
			g_hash_table_lookup(table, identifier);
		if (info != NULL)
			return info->offset;
	}

	return -1;
}

static int32_t pop_scope(struct code_generator *generator)
{
	GHashTable *table = g_array_index(generator->symbol_tables,
					  GHashTable *,
					  generator->symbol_tables->len - 1);
	GList *list = g_hash_table_get_values(table);

	int32_t size = 0;
	while (list != NULL) {
		struct symbol_info *info = list->data;
		size += info->size;
		list = list->next;
	}

	generator->stack_pointer -= size;

	g_list_free(list);
	g_hash_table_unref(table);
	g_array_remove_index(generator->symbol_tables,
			     generator->symbol_tables->len - 1);
	return size;
}

static void generate_global_string(struct code_generator *generator,
				   char *string)
{
	if (g_hash_table_lookup(generator->strings, string) != 0)
		return;

	g_print("string_%llu:\n", generator->string_counter);
	g_print("\t.string %s\n", string);
	g_print("\t.align 16\n");

	g_hash_table_insert(generator->strings, string,
			    (gpointer)generator->string_counter);
	generator->string_counter++;
}

static void generate_global_strings(struct code_generator *generator)
{
	for (struct llir_node *node = generator->node; node != NULL;
	     node = node->next) {
		if (node->type != LLIR_NODE_TYPE_METHOD_CALL)
			continue;

		GArray *arguments = node->method_call->arguments;
		for (uint32_t i = 0; i < arguments->len; i++) {
			struct llir_method_call_argument argument =
				g_array_index(arguments,
					      struct llir_method_call_argument,
					      i);

			if (argument.type !=
			    LLIR_METHOD_CALL_ARGUMENT_TYPE_STRING)
				continue;

			generate_global_string(generator, argument.string);
		}
	}
}

static void generate_global_field(struct llir_field *field)
{
	uint64_t length = 8 * (field->values->len + field->array);
	g_print("%s:\n", field->identifier);
	g_print("\t.fill %llu\n", length);
	g_print("\t.align 16\n");
}

static void generate_global_fields(struct code_generator *generator)
{
	generator->global_fields_head_node = generator->node;

	for (; generator->node->type == LLIR_NODE_TYPE_FIELD;
	     generator->node = generator->node->next) {
		generate_global_field(generator->node->field);
	}
}

static void generate_global_field_initialization(struct llir_field *field)
{
	g_print("\t# generate_global_field_initialization\n");
	if (!field->array) {
		uint64_t value = g_array_index(field->values, uint64_t, 0);
		if (value != 0)
			g_print("\tmovq $%llu, %s(%%rip)\n", value,
				field->identifier);
		return;
	}

	g_print("\tmovq $%i, %s(%%rip)\n", field->values->len, field->identifier);

	for (uint32_t i = 0; i < field->values->len; i++) {
		uint64_t value = g_array_index(field->values, uint64_t, i);

		if (value != 0)
			g_print("\tmovq $%llu, %s+%i(%%rip)\n", value,
				field->identifier, 8 * (i + 1));
	}
}

static void generate_method_arguments(struct code_generator *generator)
{
	GArray *arguments = generator->node->method->arguments;

	g_print("\t# generate_method_arguments\n");
	for (uint32_t i = 0; i < arguments->len; i++) {
		struct llir_field *field =
			g_array_index(arguments, struct llir_field *, i);
		struct symbol_info *info =
			push_field(generator, field->identifier, 1);

		g_print("\tsubq $%i, %%rsp\n", info->size);
		if (i < G_N_ELEMENTS(ARGUMENT_REGISTERS)) {
			g_print("\tmovq %%%s, %i(%%rbp)\n",
				ARGUMENT_REGISTERS[i], info->offset);
		} else {
			int32_t offset =
				(i - G_N_ELEMENTS(ARGUMENT_REGISTERS)) * 16;
			g_print("\tmovq %i(%%rbp), %%r10\n", offset);
			g_print("\tmovq %%r10, %i(%%rbp)\n", info->offset);
		}
	}
}

static void generate_method_declaration(struct code_generator *generator)
{
	push_scope(generator);

	g_print(".globl _%s\n", generator->node->method->identifier);
	g_print("_%s:\n", generator->node->method->identifier);
	g_print("\tpushq %%rbp\n");
	g_print("\tmovq %%rsp, %%rbp\n");

	generate_method_arguments(generator);

	if (g_strcmp0(generator->node->method->identifier, "main") != 0)
		return;

	for (struct llir_node *node = generator->global_fields_head_node;
	     node->type == LLIR_NODE_TYPE_FIELD; node = node->next)
		generate_global_field_initialization(node->field);
}

static void generate_block_start(struct code_generator *generator)
{
	push_scope(generator);
}

static void generate_field(struct code_generator *generator)
{
	g_assert(generator->node->type == LLIR_NODE_TYPE_FIELD);

	struct llir_field *field = generator->node->field;

	struct symbol_info *symbol_info =
		push_field(generator, field->identifier,
			   field->values->len + field->array);

	g_print("\t# generate_field\n");
	g_print("\tsubq $%u, %%rsp\n", symbol_info->size);
	g_print("\tmovq $%lld, -%u(%%rbp)\n",
		field->array ? field->values->len :
			       g_array_index(field->values, int64_t, 0),
		symbol_info->offset);

	for (uint32_t i = 0; field->array && i < field->values->len; i++) {
		g_print("\tmovq $%lld, -%u(%%rbp)\n",
			g_array_index(field->values, int64_t, i),
			(uint32_t)(8 * (i + 1)) + symbol_info->offset);
	}
}

static int32_t get_destination_offset(struct code_generator *generator,
				      char *destination)
{
	g_assert(destination != NULL);

	if (destination[0] == '$') {
		struct symbol_info *info =
			push_field(generator, destination, 1);

		g_print("\tsubq $%u, %%rsp\n", info->size);

		return info->offset;
	}

	return get_field_offset(generator, destination);
}

static void generate_assignment(struct code_generator *generator)
{
	g_assert(generator->node->type == LLIR_NODE_TYPE_ASSIGNMENT);

	struct llir_assignment *assignment = generator->node->assignment;

	int32_t source_offset = get_field_offset(generator, assignment->source);

	int32_t destination_offset =
		get_destination_offset(generator, assignment->destination);

	g_print("\t# generate_assignment\n");

	if (source_offset == -1)
		g_print("\tmovq %s(%%rip), %%r10\n", assignment->source);
	else
		g_print("\tmovq -%d(%%rbp), %%r10\n", source_offset);

	if (destination_offset == -1)
		g_print("\tmovq %%r10, %s(%%rip)\n", assignment->destination);
	else
		g_print("\tmovq %%r10, -%d(%%rbp)\n", destination_offset);
}

static void generate_literal_assignment(struct code_generator *generator)
{
	g_assert(generator->node->type == LLIR_NODE_TYPE_LITERAL_ASSIGNMENT);

	struct llir_literal_assignment *literal_assignment =
		generator->node->literal_assignment;

	int32_t destination_offset = get_destination_offset(
		generator, literal_assignment->destination);

	g_print("\t# generate_literal_assignment\n");

	if (destination_offset == -1)
		g_print("\tmovq $%lld, %s(%%rip)\n", literal_assignment->literal,
			literal_assignment->destination);
	else
		g_print("\tmovq $%lld, -%d(%%rbp)\n",
			literal_assignment->literal, destination_offset);
}

static void generate_indexed_assignment(struct code_generator *generator)
{
	g_assert(generator->node->type == LLIR_NODE_TYPE_INDEXED_ASSIGNMENT);

	struct llir_indexed_assignment *indexed_assignment =
		generator->node->indexed_assignment;

	int32_t source_offset =
		get_field_offset(generator, indexed_assignment->source);

	int32_t index_offset =
		get_field_offset(generator, indexed_assignment->index);
	g_assert(index_offset != -1);

	int32_t destination_offset = get_destination_offset(
		generator, indexed_assignment->destination);

	g_print("\t# generate_indexed_assignment\n");
	g_print("\tmovq -%u(%%rbp), %%r10\n", index_offset);
	g_print("\taddq $1, %%r10\n");
	g_print("\timul $8, %%r10\n");

	if (source_offset != -1)
		g_print("\tmovq -%u(%%rbp), %%r11\n", source_offset);
	else
		g_print("\tmovq %s(%%rip), %%r11\n", indexed_assignment->source);

	if (destination_offset != -1) {
		g_print("\tmovq %%r11, -(%%rbp, %%r10)\n");
	} else {
		g_print("\tleaq %s(%%rip), %%rax\n", indexed_assignment->destination);
		g_print("\taddq %%rax, %%r10\n");
		g_print("\tmovq %%r11, 0(%%r10)\n");
	}
}

static void generate_binary_operation(struct code_generator *generator)
{
	g_assert(generator->node->type == LLIR_NODE_TYPE_BINARY_OPERATION);

	struct llir_binary_operation *binary_operation =
		generator->node->binary_operation;

	int32_t left_operand_offset =
		get_field_offset(generator, binary_operation->left_operand);

	int32_t right_operand_offset =
		get_field_offset(generator, binary_operation->right_operand);

	int32_t destination_offset = get_destination_offset(
		generator, binary_operation->destination);

	g_print("\t# generate_binary_operation\n");

	if (left_operand_offset == -1)
		g_print("\tmovq %s(%%rip), %%r10\n", binary_operation->left_operand);
	else
		g_print("\tmovq -%d(%%rbp), %%r10\n", left_operand_offset);

	if (right_operand_offset == -1)
		g_print("\tmovq %s, %%r11\n", binary_operation->right_operand);
	else
		g_print("\tmovq -%d(%%rbp), %%r11\n", right_operand_offset);

	switch (binary_operation->operation) {
	case LLIR_BINARY_OPERATION_TYPE_OR:
		g_print("\torq %%r11, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_AND:
		g_print("\tandq %%r11, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_EQUAL:
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsete %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbl  %%al, %%eax\n");
		g_print("\tcltq\n");
		g_print("\tmovq %%rax, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_NOT_EQUAL:
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetne %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbl  %%al, %%eax\n");
		g_print("\tcltq\n");
		g_print("\tmovq %%rax, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_LESS:
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetl %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_LESS_EQUAL:
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetle %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_GREATER_EQUAL:
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetge %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_GREATER:
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetg %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_ADD:
		g_print("\taddq %%r11, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_SUB:
		g_print("\tsubq %%r11, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_MUL:
		g_print("\timul %%r11, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_DIV:
		g_print("\tmov %%r10, %%eax\n");
		g_print("\tidivq %%r11\n");
		g_print("\tmov %%eax, %%r10\n");
		break;
	case LLIR_BINARY_OPERATION_TYPE_MOD:
		g_print("\tmov %%r10, %%eax\n");
		g_print("\tidivq %%r11\n");
		g_print("\tmov %%edx, %%r10\n");
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}

	if (destination_offset == -1)
		g_print("\tmovq %%r10, %s(%%rip)\n", binary_operation->destination);
	else
		g_print("\tmovq %%r10, -%d(%%rbp)\n", destination_offset);
}

static void generate_unary_operation(struct code_generator *generator)
{
	g_assert(generator->node->type == LLIR_NODE_TYPE_UNARY_OPERATION);

	struct llir_unary_operation *unary_operation =
		generator->node->unary_operation;

	int32_t source_offset =
		get_field_offset(generator, unary_operation->source);

	int32_t destination_offset =
		get_destination_offset(generator, unary_operation->destination);

	g_print("\t# generate_unary_operation\n");

	if (source_offset == -1)
		g_print("\tmovq %s(%%rip), %%r10\n", unary_operation->source);
	else
		g_print("\tmovq -%d(%%rbp), %%r10\n", source_offset);

	switch (unary_operation->operation) {
	case LLIR_UNARY_OPERATION_TYPE_NEGATE:
		g_print("\tnegq %%r10");
		break;
	case LLIR_UNARY_OPERATION_TYPE_NOT:
		g_print("\tnotq %%r10");
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}

	if (destination_offset == -1)
		g_print("\tmovq %%r10, %s(%%rip)\n", unary_operation->destination);
	else
		g_print("\tmovq %%r10, -%d(%%rbp)\n", destination_offset);
}

static void
generate_method_call_argument(struct code_generator *generator,
			      struct llir_method_call_argument *argument,
			      uint32_t i)
{
	if (argument->type == LLIR_METHOD_CALL_ARGUMENT_TYPE_STRING) {
		uint64_t string_index = (uint64_t)g_hash_table_lookup(
			generator->strings, argument->string);
		g_assert(string_index != 0);

		g_print("\tleaq string_%llu(%%rip), %%r10\n", string_index);
	} else {
		int32_t offset =
			get_field_offset(generator, argument->identifier);
		g_assert(offset != -1);

		g_print("\tmovq %d(%%rbp), %%r10\n", offset);
	}

	if (i < G_N_ELEMENTS(ARGUMENT_REGISTERS)) {
		g_print("\tmovq %%r10, %%%s\n", ARGUMENT_REGISTERS[i]);
	} else {
		uint32_t offset = 8 * (i - G_N_ELEMENTS(ARGUMENT_REGISTERS));
		g_print("\tmovq %%r10, %u(%%rsp)\n", offset);
	}
}

static void generate_method_call(struct code_generator *generator)
{
	struct llir_method_call *method_call = generator->node->method_call;

	g_print("\t# generate_method_call\n");

	int32_t stack_argument_count =
		method_call->arguments->len - G_N_ELEMENTS(ARGUMENT_REGISTERS);
	int32_t extra_stack_size =
		8 * (stack_argument_count + stack_argument_count % 2);

	if (stack_argument_count > 0)
		g_print("\tsubq $%d, %%rsp\n", extra_stack_size);

	for (uint32_t i = 0; i < method_call->arguments->len; i++) {
		struct llir_method_call_argument argument =
			g_array_index(method_call->arguments,
				      struct llir_method_call_argument, i);
		generate_method_call_argument(generator, &argument, i);
	}

	g_print("\tmovq $0, %%rax\n");
	g_print("\tcall _%s\n", method_call->identifier);
	if (stack_argument_count > 0)
		g_print("\taddq $%i, %%rsp\n", extra_stack_size);

	int32_t offset =
		get_destination_offset(generator, method_call->destination);
	g_print("\tmovq %%rax, -%d(%%rbp)\n", offset);
}

static void generate_array_index(struct code_generator *generator)
{
	struct llir_array_index *array_index = generator->node->array_index;

	int32_t source_offset =
		get_field_offset(generator, array_index->source);

	int32_t index_offset = get_field_offset(generator, array_index->index);
	g_assert(index_offset != -1);

	int32_t destination_offset =
		get_destination_offset(generator, array_index->destination);

	g_print("\t# generate_array_index\n");
	g_print("\tmovq -%u(%%rbp), %%r10\n", index_offset);
	g_print("\taddq $1, %%r10\n");
	g_print("\timul $8, %%r10\n");

	if (source_offset != -1) {
		g_print("\tmovq -(%%rbp, %%r10), %%r11\n");
	} else {
		g_print("\tleaq %s(%%rip), %%r11\n", array_index->source);
		g_print("\taddq %%r11, %%r10\n");
		g_print("\tmovq 0(%%r10), %%r11\n");
	}

	if (destination_offset != -1)
		g_print("\tmovq %%r11, -%u(%%rbp)\n", destination_offset);
	else
		g_print("\tmovq %%r11, %s(%%rip)\n", array_index->destination);
}

static void generate_label(struct code_generator *generator)
{
	g_assert(generator->node->type == LLIR_NODE_TYPE_LABEL);

	struct llir_label *label = generator->node->label;

	g_print("# generate_label\n");
	g_print("%s:\n", label->name);
}

static void generate_branch(struct code_generator *generator)
{
	struct llir_branch *branch = generator->node->branch;
	struct llir_node *label_node = branch->label;

	int32_t offset = get_field_offset(generator, branch->condition);

	g_print("\t# generate_branch\n");
	g_print("\tmovq -%d(%%rbp), %%r10\n", offset);
	g_print("\tcmpq $0, %%r10\n");
	g_print(branch->negate ? "\tje %s\n" : "\tjne %s\n",
		label_node->label->name);
}

static void generate_jump(struct code_generator *generator)
{
	g_assert(generator->node->type == LLIR_NODE_TYPE_JUMP);

	struct llir_jump *jump = generator->node->jump;
	struct llir_node *label_node = jump->label;

	g_print("\t# generate_jump\n");
	g_print("\tjmp %s\n", label_node->label->name);
}

static void generate_return(struct code_generator *generator)
{
	g_print("\t# generate_return\n");

	struct llir_return *llir_return = generator->node->llir_return;
	if (llir_return->source == NULL) {
		g_print("\tmov $0, %%rax\n");
	} else {
		uint32_t offset =
			get_field_offset(generator, llir_return->source);
		g_print("\tmovq -%u(%%rbp), %%rax\n", offset);
	}

	g_print("\tmovq %%rbp, %%rsp\n");
	g_print("\tpopq %%rbp\n");
	g_print("\tret\n");
}

static void generate_block_end(struct code_generator *generator)
{
	int32_t size = pop_scope(generator);
	g_print("\t# generate_block_end\n");
	g_print("\taddq $%i, %%rsp\n", size);
}

static void generate_method_end(struct code_generator *generator)
{
	pop_scope(generator);
	g_print("\t# generate_method_end\n");
	g_print("\tmovl $0x2000001, %%eax\n");
	g_print("\tmovl $-2, %%edi\n");
	g_print("\tsyscall\n");
}

static void generate_data_section(struct code_generator *generator)
{
	g_print(".data\n");
	generate_global_strings(generator);
	generate_global_fields(generator);
}

static void generate_text_section(struct code_generator *generator)
{
	g_print(".text\n");

	for (; generator->node != NULL;
	     generator->node = generator->node->next) {
		switch (generator->node->type) {
		case LLIR_NODE_TYPE_FIELD:
			generate_field(generator);
			break;
		case LLIR_NODE_TYPE_METHOD_START:
			generate_method_declaration(generator);
			break;
		case LLIR_NODE_TYPE_BLOCK_START:
			generate_block_start(generator);
			break;
		case LLIR_NODE_TYPE_ASSIGNMENT:
			generate_assignment(generator);
			break;
		case LLIR_NODE_TYPE_LITERAL_ASSIGNMENT:
			generate_literal_assignment(generator);
			break;
		case LLIR_NODE_TYPE_INDEXED_ASSIGNMENT:
			generate_indexed_assignment(generator);
			break;
		case LLIR_NODE_TYPE_BINARY_OPERATION:
			generate_binary_operation(generator);
			break;
		case LLIR_NODE_TYPE_UNARY_OPERATION:
			generate_unary_operation(generator);
			break;
		case LLIR_NODE_TYPE_METHOD_CALL:
			generate_method_call(generator);
			break;
		case LLIR_NODE_TYPE_ARRAY_INDEX:
			generate_array_index(generator);
			break;
		case LLIR_NODE_TYPE_LABEL:
			generate_label(generator);
			break;
		case LLIR_NODE_TYPE_BRANCH:
			generate_branch(generator);
			break;
		case LLIR_NODE_TYPE_JUMP:
			generate_jump(generator);
			break;
		case LLIR_NODE_TYPE_RETURN:
			generate_return(generator);
			break;
		case LLIR_NODE_TYPE_BLOCK_END:
			generate_block_end(generator);
			break;
		case LLIR_NODE_TYPE_METHOD_END:
			generate_method_end(generator);
			break;
		default:
			g_assert(
				!"bruh the fuck kinda of bombaclot node is this arf arf");
			break;
		}
	}
}

struct code_generator *code_generator_new(void)
{
	struct code_generator *generator = g_new(struct code_generator, 1);
	generator->node = NULL;
	return generator;
}

int code_generator_generate(struct code_generator *generator,
			    struct ir_program *ir)
{
	struct llir_node *head = llir_node_new_program(ir);
	generator->node = head;
	generator->strings = g_hash_table_new(g_str_hash, g_str_equal);
	generator->string_counter = 1;
	generator->symbol_tables =
		g_array_new(false, false, sizeof(GHashTable *));
	generator->stack_pointer = 0;

	g_assert(generator->node->type == LLIR_NODE_TYPE_PROGRAM);
	generator->node = generator->node->next;

	while (generator->node->type == LLIR_NODE_TYPE_IMPORT)
		generator->node = generator->node->next;

	generate_data_section(generator);
	generate_text_section(generator);

	g_array_free(generator->symbol_tables, true);
	g_hash_table_unref(generator->strings);
	//llir_node_free(head); - something is fucked with this
	return 0;
}

void code_generator_free(struct code_generator *generator)
{
	g_free(generator);
}
