#include "assembly/assembly.h"

static void generate_global_string(struct code_generator *generator,
				   char *string)
{
	g_print("string_%i:\n", generator->string_counter);
	g_print("\t.string %s\n", string);
	g_print("\t.align 16");

	g_hash_table_insert(generator->strings, string,
			    generator->string_counter);
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

static void generate_global_fields(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_method_declaration(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_fields(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_assignment(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_literal_assignment(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_indexed_assignment(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_binary_operation(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_unary_operation(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_method_call(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_array_index(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_label(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_branch(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_jump(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_return(struct code_generator *generator)
{
	g_assert(!"TODO");
}

static void generate_data_section(struct code_generator *generator)
{
	g_print(".data\n");
	generate_global_strings(generator);
	generate_global_fields(generator);
}

static void generate_text_section(struct code_generator *generator)
{
	g_print(".text\n.globl _main\n");

	for (; generator->node->next != NULL;
	     generator->node = generator->node->next) {
		switch (generator->node->type) {
		case LLIR_NODE_TYPE_FIELD:
			generate_fields(generator);
			break;
		case LLIR_NODE_TYPE_METHOD:
			generate_method_declaration(generator);
			break;
		case LLIR_NODE_TYPE_BLOCK_START:
			g_assert(!"TODO create field table");
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
			g_assert(!"TODO figure out the fuck this does");
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

	while (generator->node->type == LLIR_NODE_TYPE_IMPORT)
		generator->node = generator->node->next;

	generate_data_section(generator);
	generate_text_section(generator);

	g_hash_table_unref(generator->strings);
	llir_node_free(head);
	return 0;
}

void code_generator_free(struct code_generator *generator)
{
	g_free(generator);
}
