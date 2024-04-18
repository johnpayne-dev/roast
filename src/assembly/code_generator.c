#include "assembly/code_generator.h"

static const char *ARGUMENT_REGISTERS[] = { "rdi", "rsi", "rdx",
					    "rcx", "r8",  "r9" };

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
		if (node->type != LLIR_NODE_TYPE_ASSIGNMENT ||
		    node->assignment->type != LLIR_ASSIGNMENT_TYPE_METHOD_CALL)
			continue;

		struct llir_assignment *call = node->assignment;

		for (uint32_t i = 0; i < call->argument_count; i++) {
			struct llir_operand argument = call->arguments[i];

			if (argument.type != LLIR_OPERAND_TYPE_STRING)
				continue;

			generate_global_string(generator, argument.string);
		}
	}
}

static void generate_global_field(struct code_generator *generator)
{
	struct llir_field *field = generator->node->field;

	g_print("%s:\n", field->identifier);
	g_print("\t.fill %llu\n", 8 * field->value_count);
	g_print("\t.align 16\n");
}

static void generate_global_fields(struct code_generator *generator)
{
	generator->global_fields = generator->node;

	for (; generator->node->type == LLIR_NODE_TYPE_FIELD;
	     generator->node = generator->node->next)
		generate_global_field(generator);
}

static void generate_data_section(struct code_generator *generator)
{
	g_print(".data\n");
	generate_global_strings(generator);
	generate_global_fields(generator);
}

static void generate_stack_allocation(struct code_generator *generator)
{
	uint64_t stack_size = 0;

	for (struct llir_node *node = generator->node->next;
	     node != NULL && node->type != LLIR_NODE_TYPE_METHOD;
	     node = node->next) {
		if (node->type != LLIR_NODE_TYPE_FIELD)
			continue;

		struct llir_field *field = node->field;

		stack_size += field->value_count * 8;
		g_hash_table_insert(generator->offsets, field->identifier,
				    (gpointer)stack_size);
	}

	if (stack_size % 16 != 0)
		stack_size += 8;

	g_print("\tsubq $%llu, %%rsp\n", stack_size);
}

static void generate_method_arguments(struct code_generator *generator)
{
	struct llir_method *method = generator->node->method;

	for (uint32_t i = 0; i < method->argument_count; i++) {
		char *field = method->arguments[i];

		uint64_t offset = (uint64_t)g_hash_table_lookup(
			generator->offsets, field);
		g_assert(offset != 0);

		if (i < G_N_ELEMENTS(ARGUMENT_REGISTERS)) {
			g_print("\tmovq %%%s, -%llu(%%rbp)\n",
				ARGUMENT_REGISTERS[i], offset);
		} else {
			int32_t argument_offset =
				(i - G_N_ELEMENTS(ARGUMENT_REGISTERS)) * 8 + 16;
			g_print("\tmovq %i(%%rbp), %%r10\n", argument_offset);
			g_print("\tmovq %%r10, -%llu(%%rbp)\n", offset);
		}
	}
}

static void generate_global_field_initialization(struct llir_field *field)
{
	for (uint32_t i = 0; i < field->value_count; i++) {
		if (field->values[i] == 0)
			continue;

		g_print("\tmovq $%llu, %s+%i(%%rip)\n", field->values[i],
			field->identifier, 8 * i);
	}
}

static void generate_method_declaration(struct code_generator *generator)
{
	struct llir_method *method = generator->node->method;

#ifdef __APPLE__
	g_print(".globl _%s\n", method->identifier);
	g_print("_%s:\n", method->identifier);
#else
	g_print(".globl %s\n", method->identifier);
	g_print("%s:\n", method->identifier);
#endif
	g_print("\tpushq %%rbp\n");
	g_print("\tmovq %%rsp, %%rbp\n");

	generate_stack_allocation(generator);
	generate_method_arguments(generator);

	if (g_strcmp0(generator->node->method->identifier, "main") != 0)
		return;

	for (struct llir_node *node = generator->global_fields;
	     node->type == LLIR_NODE_TYPE_FIELD; node = node->next)
		generate_global_field_initialization(node->field);
}

static void load_to_register(struct code_generator *generator,
			     struct llir_operand operand,
			     const char *destination)
{
	uint64_t offset, string_id;

	switch (operand.type) {
	case LLIR_OPERAND_TYPE_LITERAL:
		g_print("\tmovq $%lld, %%%s\n", operand.literal, destination);
		break;
	case LLIR_OPERAND_TYPE_FIELD:
		offset = (uint64_t)g_hash_table_lookup(generator->offsets,
						       operand.field);

		if (offset != 0)
			g_print("\tmovq -%llu(%%rbp), %%%s\n", offset,
				destination);
		else
			g_print("\tmovq %s(%%rip), %%%s\n", operand.field,
				destination);
		break;
	case LLIR_OPERAND_TYPE_STRING:
		string_id = (uint64_t)g_hash_table_lookup(generator->strings,
							  operand.string);
		g_print("\tleaq string_%llu(%%rip), %%%s\n", string_id,
			destination);
		break;

	default:
		g_assert(!"you fucked up");
		break;
	}
}

static void load_array_to_register(struct code_generator *generator,
				   char *array, const char *destination)
{
	uint64_t offset =
		(uint64_t)g_hash_table_lookup(generator->offsets, array);
	if (offset != 0)
		g_print("\tleaq -%llu(%%rbp), %%%s\n", offset, destination);
	else
		g_print("\tleaq %s(%%rip), %%%s\n", array, destination);
}

static void store_from_register(struct code_generator *generator,
				char *destination, const char *source)
{
	uint64_t offset =
		(uint64_t)g_hash_table_lookup(generator->offsets, destination);
	if (offset != 0)
		g_print("\tmovq %%%s, -%llu(%%rbp)\n", source, offset);
	else
		g_print("\tmovq %%%s, %s(%%rip)\n", source, destination);
}

static void generate_method_call(struct code_generator *generator)
{
	struct llir_assignment *call = generator->node->assignment;

	int32_t stack_argument_count =
		call->argument_count - G_N_ELEMENTS(ARGUMENT_REGISTERS);
	int32_t extra_stack_size =
		8 * (stack_argument_count + stack_argument_count % 2);

	if (stack_argument_count > 0)
		g_print("\tsubq $%d, %%rsp\n", extra_stack_size);

	for (uint32_t i = 0; i < call->argument_count; i++) {
		struct llir_operand argument = call->arguments[i];

		if (i < G_N_ELEMENTS(ARGUMENT_REGISTERS)) {
			load_to_register(generator, argument,
					 ARGUMENT_REGISTERS[i]);
		} else {
			load_to_register(generator, argument, "r10");
			uint32_t offset =
				8 * (i - G_N_ELEMENTS(ARGUMENT_REGISTERS));
			g_print("\tmovq %%r10, %u(%%rsp)\n", offset);
		}
	}

	g_print("\tmovq $0, %%rax\n");
#ifdef __APPLE__
	g_print("\tcall _%s\n", call->method);
#else
	g_print("\tcall %s\n", call->method);
#endif
	if (stack_argument_count > 0)
		g_print("\taddq $%i, %%rsp\n", extra_stack_size);

	store_from_register(generator, call->destination, "rax");
}

static void generate_assignment(struct code_generator *generator)
{
	struct llir_assignment *assignment = generator->node->assignment;

	switch (assignment->type) {
	case LLIR_ASSIGNMENT_TYPE_MOVE:
		load_to_register(generator, assignment->source, "r10");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_ADD:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\taddq %%r11, %%r10\n");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_SUBTRACT:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tsubq %%r11, %%r10\n");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_MULTIPLY:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\timulq %%r11, %%r10\n");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_DIVIDE:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tmovq %%r10, %%rax\n");
		g_print("\tcqto\n");
		g_print("\tidivq %%r11\n");
		store_from_register(generator, assignment->destination, "rax");
		break;
	case LLIR_ASSIGNMENT_TYPE_MODULO:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tmovq %%r10, %%rax\n");
		g_print("\tcqto\n");
		g_print("\tidivq %%r11\n");
		store_from_register(generator, assignment->destination, "rdx");
		break;
	case LLIR_ASSIGNMENT_TYPE_EQUAL:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsete %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbl  %%al, %%eax\n");
		g_print("\tcltq\n");
		store_from_register(generator, assignment->destination, "rax");
		break;
	case LLIR_ASSIGNMENT_TYPE_NOT_EQUAL:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetne %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbl  %%al, %%eax\n");
		g_print("\tcltq\n");
		store_from_register(generator, assignment->destination, "rax");
		break;
	case LLIR_ASSIGNMENT_TYPE_LESS:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetl %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%r10\n");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_LESS_EQUAL:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetle %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%r10\n");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_GREATER_EQUAL:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetge %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%r10\n");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_GREATER:
		load_to_register(generator, assignment->left, "r10");
		load_to_register(generator, assignment->right, "r11");
		g_print("\tcmpq %%r11, %%r10\n");
		g_print("\tsetg %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%r10\n");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_NEGATE:
		load_to_register(generator, assignment->source, "r10");
		g_print("\tnegq %%r10\n");
		store_from_register(generator, assignment->destination, "r10");
		break;
	case LLIR_ASSIGNMENT_TYPE_NOT:
		load_to_register(generator, assignment->source, "r10");
		g_print("\tcmpq $0, %%r10\n");
		g_print("\tsetne %%al\n");
		g_print("\txorb $-1, %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbl  %%al, %%eax\n");
		g_print("\tcltq\n");
		store_from_register(generator, assignment->destination, "rax");
		break;
	case LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS:
		load_array_to_register(generator, assignment->access_array,
				       "r10");
		load_to_register(generator, assignment->access_index, "r11");
		g_print("\timulq $8, %%r11\n");
		g_print("\taddq %%r11, %%r10\n");
		g_print("\tmovq 0(%%r10), %%r11\n");
		store_from_register(generator, assignment->destination, "r11");
		break;
	case LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE:
		load_array_to_register(generator, assignment->destination,
				       "r10");
		load_to_register(generator, assignment->update_index, "r11");
		g_print("\timulq $8, %%r11\n");
		g_print("\taddq %%r11, %%r10\n");
		load_to_register(generator, assignment->update_value, "r11");
		g_print("\tmovq %%r11, 0(%%r10)\n");
		break;
	case LLIR_ASSIGNMENT_TYPE_METHOD_CALL:
		generate_method_call(generator);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
}

static void generate_label(struct code_generator *generator)
{
	struct llir_label *label = generator->node->label;
	g_print("%s:\n", label->name);
}

static void generate_branch(struct code_generator *generator)
{
	struct llir_branch *branch = generator->node->branch;

	load_to_register(generator, branch->left, "r11");
	load_to_register(generator, branch->right, "r10");

	g_print("\tcmpq %%r10, %%r11\n");

	g_print("\t");
	switch (branch->type) {
	case LLIR_BRANCH_TYPE_EQUAL:
		g_print("je");
		break;
	case LLIR_BRANCH_TYPE_NOT_EQUAL:
		g_print("jne");
		break;
	case LLIR_BRANCH_TYPE_LESS:
		if (branch->unsigned_comparison)
			g_print("jb");
		else
			g_print("jl");
		break;
	case LLIR_BRANCH_TYPE_LESS_EQUAL:
		if (branch->unsigned_comparison)
			g_print("jbe");
		else
			g_print("jle");

		break;
	case LLIR_BRANCH_TYPE_GREATER:
		if (branch->unsigned_comparison)
			g_print("ja");
		else
			g_print("jg");
		break;
	case LLIR_BRANCH_TYPE_GREATER_EQUAL:
		if (branch->unsigned_comparison)
			g_print("jae");
		else
			g_print("jge");
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
	g_print(" %s\n", branch->label->name);
}

static void generate_jump(struct code_generator *generator)
{
	struct llir_jump *jump = generator->node->jump;
	g_print("\tjmp %s\n", jump->label->name);
}

static void generate_return(struct code_generator *generator)
{
	struct llir_operand source = generator->node->llir_return->source;

	load_to_register(generator, source, "rax");
	g_print("\tmovq %%rbp, %%rsp\n");
	g_print("\tpopq %%rbp\n");
	g_print("\tret\n");
}

static void generate_shit_yourself(struct code_generator *generator)
{
	struct llir_shit_yourself *exit = generator->node->shit_yourself;

	g_print("\tmovq $%lld, %%rdi\n", exit->return_value);
#ifdef __APPLE__
	g_print("\tcall _exit\n");
#else
	g_print("\tcall exit\n");
#endif
}

static void generate_text_section(struct code_generator *generator)
{
	g_print(".text\n");

	for (; generator->node != NULL;
	     generator->node = generator->node->next) {
		switch (generator->node->type) {
		case LLIR_NODE_TYPE_FIELD:
			break;
		case LLIR_NODE_TYPE_METHOD:
			generate_method_declaration(generator);
			break;
		case LLIR_NODE_TYPE_ASSIGNMENT:
			generate_assignment(generator);
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
		case LLIR_NODE_TYPE_SHIT_YOURSELF:
			generate_shit_yourself(generator);
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
	generator->offsets = g_hash_table_new(g_str_hash, g_str_equal);
	return generator;
}

void code_generator_generate(struct code_generator *generator,
			     struct llir_node *llir)
{
	generator->node = llir;
	generator->strings = g_hash_table_new(g_str_hash, g_str_equal);
	generator->string_counter = 1;

	generate_data_section(generator);
	generate_text_section(generator);

	g_hash_table_unref(generator->strings);
}

void code_generator_free(struct code_generator *generator)
{
	g_hash_table_unref(generator->offsets);
	g_free(generator);
}
