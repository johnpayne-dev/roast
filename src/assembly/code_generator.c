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
		if (node->type != LLIR_NODE_TYPE_METHOD_CALL)
			continue;

		struct llir_method_call *call = generator->node->method_call;

		for (uint32_t i = 0; i < call->argument_count; i++) {
			struct llir_operand argument = call->arguments[i];

			if (argument.type != LLIR_OPERAND_TYPE_STRING)
				continue;

			generate_global_string(generator, argument.string);
		}
	}
}

static void generate_global_field(struct llir_field *field)
{
	g_print("%s:\n", field->identifier);
	g_print("\t.fill %llu\n", 8 * field->value_count);
	g_print("\t.align 16\n");
}

static void generate_global_fields(struct code_generator *generator)
{
	generator->global_fields = generator->node;

	for (; generator->node->type == LLIR_NODE_TYPE_FIELD;
	     generator->node = generator->node->next)
		generate_global_field(generator->node->field);
}

static void generate_data_section(struct code_generator *generator)
{
	g_print(".data\n");
	generate_global_strings(generator);
	generate_global_fields(generator);
}

static void generate_stack_allocation(struct code_generator *generator)
{
	uint32_t stack_size = 0;

	struct llir_method *method = generator->node->method;
	for (uint32_t i = 0; i < method->argument_count; i++) {
		struct llir_field *field = method->arguments[i];

		stack_size += field->value_count * 8;
		g_hash_table_insert(generator->offsets, field->identifier,
				    stack_size);
	}

	for (struct llir_node *node = generator->node->next;
	     node != NULL && node->type != LLIR_NODE_TYPE_METHOD;
	     node = node->next) {
		if (node->type != LLIR_NODE_TYPE_FIELD)
			continue;

		struct llir_field *field = node->field;

		stack_size += field->value_count * 8;
		g_hash_table_insert(generator->offsets, field->identifier,
				    stack_size);
	}

	if (stack_size % 16 != 0)
		stack_size += 8;

	g_print("\tsubq $%u, %%rsp\n", stack_size);
}

static void generate_method_arguments(struct code_generator *generator)
{
	struct llir_method *method = generator->node->method;

	for (uint32_t i = 0; i < method->argument_count; i++) {
		struct llir_field *field = method->arguments[i];

		uint32_t offset = g_hash_table_lookup(generator->offsets,
						      field->identifier);
		g_assert(offset != 0);

		if (i < G_N_ELEMENTS(ARGUMENT_REGISTERS)) {
			g_print("\tmovq %%%s, -%i(%%rbp)\n",
				ARGUMENT_REGISTERS[i], offset);
		} else {
			int32_t argument_offset =
				(i - G_N_ELEMENTS(ARGUMENT_REGISTERS)) * 8 + 16;
			g_print("\tmovq %i(%%rbp), %%r10\n", argument_offset);
			g_print("\tmovq %%r10, -%i(%%rbp)\n", offset);
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

static void generate_field_initialization(struct code_generator *generator)
{
	struct llir_field *field = generator->node->field;

	uint32_t offset =
		g_hash_table_lookup(generator->offsets, field->identifier);
	g_assert(offset != 0);

	for (int64_t i = 0; i < field->value_count; i++)
		g_print("\tmovq $%lld, -%lld(%%rbp)\n", field->values[i],
			offset - 8 * i);
}

static void generate_operation(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_method_call_argument(struct code_generator *generator,
					  struct llir_operand argument,
					  uint32_t i)
{
	g_assert(!"TODO");
}

static void generate_method_call(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_label(struct code_generator *generator)
{
	struct llir_label *label = generator->node->label;
	g_print("%s:\n", label->name);
}

static void generate_branch(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_jump(struct code_generator *generator)
{
	struct llir_jump *jump = generator->node->jump;
	g_print("\tjmp %s\n", jump->label->name);
}

static void generate_return(struct code_generator *generator)
{
	struct llir_operand source = generator->node->llir_return->source;

	uint32_t offset;
	switch (source.type) {
	case LLIR_OPERAND_TYPE_LITERAL:
		g_print("\tmovq $%lld, %%rax\n", source.literal);
		break;
	case LLIR_OPERAND_TYPE_VARIABLE:
		offset = g_hash_table_lookup(generator->offsets,
					     source.identifier);
		if (offset != 0)
			g_print("\tmovq -%u(%%rbp), %%rax\n", offset);
		else
			g_print("\tmovq %s(%%rip), %%rax\n", source.identifier);
	case LLIR_OPERAND_TYPE_DEREFERENCE:
		offset = g_hash_table_lookup(generator->offsets,
					     source.dereference.identifier);
		if (offset != 0) {
			g_print("\tmovq -%u(%%rbp), %%r10\n", offset);
			g_print("\tmovq %lld(%%r10), %%rax\n",
				source.dereference.offset);
		} else {
			g_print("\tmovq %s+%lld(%%rip), %%rax\n",
				source.dereference.identifier,
				source.dereference.offset);
		}
	default:
		g_assert(!"you fucked up");
		break;
	}

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
			generate_field_initialization(generator);
			break;
		case LLIR_NODE_TYPE_METHOD:
			generate_method_declaration(generator);
			break;
		case LLIR_NODE_TYPE_OPERATION:
			generate_operation(generator);
			break;
		case LLIR_NODE_TYPE_METHOD_CALL:
			generate_method_call(generator);
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
