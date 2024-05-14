#include "assembly/code_generator.h"

static const char *ARGUMENT_REGISTERS[] = { "rdi", "rsi", "rdx",
					    "rcx", "r8",  "r9" };

static const char *CALLEE_SAVED_REGISTERS[] = {
	"rbx", "r12", "r13", "r14", "r15",
};

static void use_register(struct code_generator *generator, const char *reg)
{
	uint32_t i = 0;
	for (; i < G_N_ELEMENTS(generator->least_recently_used); i++) {
		if (g_strcmp0(generator->least_recently_used[i], reg) == 0)
			break;
	}
	g_assert(i != G_N_ELEMENTS(generator->least_recently_used));

	memmove(&generator->least_recently_used[i],
		&generator->least_recently_used[i + 1],
		(G_N_ELEMENTS(generator->least_recently_used) - i - 1) *
			sizeof(generator->least_recently_used[0]));
	generator->least_recently_used[G_N_ELEMENTS(
					       generator->least_recently_used) -
				       1] = reg;
}

static const char *allocate_register(struct code_generator *generator,
				     char *variable)
{
	const char *reg =
		g_hash_table_lookup(generator->variable_to_register, variable);
	if (reg != NULL) {
		use_register(generator, reg);
		return reg;
	}

	reg = generator->least_recently_used[0];

	char *old_variable =
		g_hash_table_lookup(generator->register_to_variable, reg);
	if (old_variable != NULL) {
		uint64_t offset = (uint64_t)g_hash_table_lookup(
			generator->offsets, old_variable);
		if (offset != 0)
			g_print("\tmovq %%%s, -%llu(%%rbp)\n", reg, offset);
		else
			g_print("\tmovq %%%s, %s(%%rip)\n", reg, old_variable);

		g_hash_table_insert(generator->register_to_variable,
				    (char *)reg, variable);
		g_hash_table_insert(generator->variable_to_register, variable,
				    (char *)reg);
		g_hash_table_insert(generator->variable_to_register,
				    old_variable, NULL);
	}

	uint64_t offset =
		(uint64_t)g_hash_table_lookup(generator->offsets, variable);
	if (offset != 0)
		g_print("\tmovq -%llu(%%rbp), %%%s\n", offset, reg);
	else
		g_print("\tmovq %s(%%rip), %%%s\n", variable, reg);

	use_register(generator, reg);

	return reg;
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

static void find_strings_in_method_call(struct code_generator *generator,
					struct llir_assignment *call)
{
	for (uint32_t i = 0; i < call->argument_count; i++) {
		struct llir_operand argument = call->arguments[i];
		if (argument.type != LLIR_OPERAND_TYPE_STRING)
			continue;

		generate_global_string(generator, argument.string);
	}
}

static void find_strings_in_block(struct code_generator *generator,
				  struct llir_block *block)
{
	for (uint32_t i = 0; i < block->assignments->len; i++) {
		struct llir_assignment *assignment = g_array_index(
			block->assignments, struct llir_assignment *, i);
		if (assignment->type != LLIR_ASSIGNMENT_TYPE_METHOD_CALL)
			continue;

		find_strings_in_method_call(generator, assignment);
	}
}

static void find_strings_in_method(struct code_generator *generator,
				   struct llir_method *method)
{
	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);
		find_strings_in_block(generator, block);
	}
}

static void generate_global_strings(struct code_generator *generator)
{
	for (uint32_t i = 0; i < generator->llir->methods->len; i++) {
		struct llir_method *method = g_array_index(
			generator->llir->methods, struct llir_method *, i);
		find_strings_in_method(generator, method);
	}
}

static void generate_global_fields(struct code_generator *generator)
{
	for (uint32_t i = 0; i < generator->llir->fields->len; i++) {
		struct llir_field *field = g_array_index(
			generator->llir->fields, struct llir_field *, i);

		g_print("%s:\n", field->identifier);
		g_print("\t.fill %llu\n", 8 * field->value_count);
		g_print("\t.align 16\n");
	}
}

static void generate_data_section(struct code_generator *generator)
{
	g_print(".data\n");
	generate_global_strings(generator);
	generate_global_fields(generator);
}

static void generate_stack_allocation(struct code_generator *generator,
				      struct llir_method *method)
{
	uint64_t stack_size = 0;

	for (uint32_t i = 0; i < method->arguments->len; i++) {
		struct llir_field *field = g_array_index(
			method->arguments, struct llir_field *, i);

		stack_size += field->value_count * 8;
		g_hash_table_insert(generator->offsets, field->identifier,
				    (gpointer)stack_size);
	}

	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);

		for (uint32_t j = 0; j < block->fields->len; j++) {
			struct llir_field *field = g_array_index(
				block->fields, struct llir_field *, j);

			stack_size += field->value_count * 8;
			g_hash_table_insert(generator->offsets,
					    field->identifier,
					    (gpointer)stack_size);
		}
	}

	if (stack_size % 16 != 0)
		stack_size += 8;

	g_print("\tsubq $%llu, %%rsp\n", stack_size);
}

static void generate_method_arguments(struct code_generator *generator,
				      struct llir_method *method)
{
	for (uint32_t i = 0; i < method->arguments->len; i++) {
		struct llir_field *field = g_array_index(
			method->arguments, struct llir_field *, i);

		uint64_t offset = (uint64_t)g_hash_table_lookup(
			generator->offsets, field->identifier);
		g_assert(offset != 0);

		if (i < G_N_ELEMENTS(ARGUMENT_REGISTERS)) {
			g_print("\tmovq %%%s, -%llu(%%rbp)\n",
				ARGUMENT_REGISTERS[i], offset);
		} else {
			int32_t argument_offset =
				(i - G_N_ELEMENTS(ARGUMENT_REGISTERS)) * 8 +
				16 + 5 * 8;
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

static void generate_method_declaration(struct code_generator *generator,
					struct llir_method *method)
{
#ifdef __APPLE__
	g_print(".globl _%s\n", method->identifier);
	g_print("_%s:\n", method->identifier);
#else
	g_print(".globl %s\n", method->identifier);
	g_print("%s:\n", method->identifier);
#endif
	g_print("\tpushq %%rbp\n");
	for (uint32_t i = 0; i < G_N_ELEMENTS(CALLEE_SAVED_REGISTERS); i++)
		g_print("\tpushq %%%s\n", CALLEE_SAVED_REGISTERS[i]);

	g_print("\tmovq %%rsp, %%rbp\n");

	generate_stack_allocation(generator, method);
	generate_method_arguments(generator, method);

	if (g_strcmp0(method->identifier, "main") != 0)
		return;

	for (uint32_t i = 0; i < generator->llir->fields->len; i++) {
		struct llir_field *field = g_array_index(
			generator->llir->fields, struct llir_field *, i);
		generate_global_field_initialization(field);
	}
}

static void load_to_register(struct code_generator *generator,
			     struct llir_operand operand,
			     const char *destination)
{
	uint64_t offset, string_id;
	char *reg;

	switch (operand.type) {
	case LLIR_OPERAND_TYPE_LITERAL:
		g_print("\tmovq $%lld, %%%s\n", operand.literal, destination);
		break;
	case LLIR_OPERAND_TYPE_FIELD:
		reg = g_hash_table_lookup(generator->variable_to_register,
					  operand.field);
		if (reg != NULL) {
			if (g_strcmp0(reg, destination) == 0)
				break;

			g_print("\tmovq %%%s, %%%s\n", reg, destination);
			break;
		}

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
	char *reg = g_hash_table_lookup(generator->variable_to_register,
					destination);
	if (reg != NULL) {
		if (g_strcmp0(source, reg) == 0)
			return;

		g_print("\tmovq %%%s, %%%s\n", reg, destination);
		return;
	}

	uint64_t offset =
		(uint64_t)g_hash_table_lookup(generator->offsets, destination);
	if (offset != 0)
		g_print("\tmovq %%%s, -%llu(%%rbp)\n", source, offset);
	else
		g_print("\tmovq %%%s, %s(%%rip)\n", source, destination);
}

static void generate_method_call(struct code_generator *generator,
				 struct llir_assignment *call)
{
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

static const char *operand_to_reg(struct code_generator *generator,
				  struct llir_operand operand,
				  const char *default_reg)
{
	if (operand.type != LLIR_OPERAND_TYPE_FIELD)
		return default_reg;

	return allocate_register(generator, operand.field);
}

static void generate_assignment(struct code_generator *generator,
				struct llir_assignment *assignment)
{
	const char *reg1, *reg2;
	switch (assignment->type) {
	case LLIR_ASSIGNMENT_TYPE_MOVE:
		reg1 = operand_to_reg(generator, assignment->source, "r10");
		load_to_register(generator, assignment->source, reg1);
		store_from_register(generator, assignment->destination, reg1);
		break;
	case LLIR_ASSIGNMENT_TYPE_ADD:
		reg1 = operand_to_reg(generator, assignment->source, "r10");
		load_to_register(generator, assignment->left, reg1);
		reg2 = operand_to_reg(generator, assignment->source, "r11");
		load_to_register(generator, assignment->right, reg2);
		g_print("\taddq %%%s, %%%s\n", reg2, reg1);
		store_from_register(generator, assignment->destination, reg1);
		break;
	case LLIR_ASSIGNMENT_TYPE_SUBTRACT:
		reg1 = operand_to_reg(generator, assignment->source, "r10");
		load_to_register(generator, assignment->left, reg1);
		reg2 = operand_to_reg(generator, assignment->source, "r11");
		load_to_register(generator, assignment->right, reg2);
		g_print("\tsubq %%%s, %%%s\n", reg2, reg1);
		store_from_register(generator, assignment->destination, reg1);
		break;
	case LLIR_ASSIGNMENT_TYPE_MULTIPLY:
		reg1 = operand_to_reg(generator, assignment->source, "r10");
		load_to_register(generator, assignment->left, reg1);
		reg2 = operand_to_reg(generator, assignment->source, "r11");
		load_to_register(generator, assignment->right, reg2);
		g_print("\timulq %%%s, %%%s\n", reg2, reg1);
		store_from_register(generator, assignment->destination, reg1);
		break;
	case LLIR_ASSIGNMENT_TYPE_DIVIDE:
		reg1 = operand_to_reg(generator, assignment->source, "r10");
		load_to_register(generator, assignment->left, reg1);
		reg2 = operand_to_reg(generator, assignment->source, "r11");
		load_to_register(generator, assignment->right, reg2);
		g_print("\tmovq %%%s, %%rax\n", reg1);
		g_print("\tcqto\n");
		g_print("\tidivq %%%s\n", reg2);
		store_from_register(generator, assignment->destination, "rax");
		break;
	case LLIR_ASSIGNMENT_TYPE_MODULO:
		reg1 = operand_to_reg(generator, assignment->source, "r10");
		load_to_register(generator, assignment->left, reg1);
		reg2 = operand_to_reg(generator, assignment->source, "r11");
		load_to_register(generator, assignment->right, reg2);
		g_print("\tmovq %%%s, %%rax\n", reg1);
		g_print("\tcqto\n");
		g_print("\tidivq %%%s\n", reg2);
		store_from_register(generator, assignment->destination, "rdx");
		break;
	case LLIR_ASSIGNMENT_TYPE_EQUAL:
		reg1 = operand_to_reg(generator, assignment->source, "r10");
		load_to_register(generator, assignment->left, reg1);
		reg2 = operand_to_reg(generator, assignment->source, "r11");
		load_to_register(generator, assignment->right, reg2);
		g_print("\tcmpq %%%s, %%%s\n", reg2, reg1);
		g_print("\tsete %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbl  %%al, %%eax\n");
		g_print("\tcltq\n");
		store_from_register(generator, assignment->destination, "rax");
		break;
	case LLIR_ASSIGNMENT_TYPE_NOT_EQUAL:
			reg1 = operand_to_reg(generator, assignment->source, "r10");
			load_to_register(generator, assignment->left, reg1);
			reg2 = operand_to_reg(generator, assignment->source, "r11");
			load_to_register(generator, assignment->right, reg2);
		g_print("\tcmpq %%%s, %%%s\n", reg2, reg1);
		g_print("\tsetne %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbl  %%al, %%eax\n");
		g_print("\tcltq\n");
		store_from_register(generator, assignment->destination, "rax");
		break;
	case LLIR_ASSIGNMENT_TYPE_LESS:
			reg1 = operand_to_reg(generator, assignment->source, "r10");
			load_to_register(generator, assignment->left, reg1);
			reg2 = operand_to_reg(generator, assignment->source, "r11");
			load_to_register(generator, assignment->right, reg2);
		g_print("\tcmpq %%%s, %%%s\n", reg2, reg1);
		g_print("\tsetl %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%%s\n", reg1);
		store_from_register(generator, assignment->destination, reg1);
		break;
	case LLIR_ASSIGNMENT_TYPE_LESS_EQUAL:
			reg1 = operand_to_reg(generator, assignment->source, "r10");
			load_to_register(generator, assignment->left, reg1);
			reg2 = operand_to_reg(generator, assignment->source, "r11");
			load_to_register(generator, assignment->right, reg2);
		g_print("\tcmpq %%%s, %%%s\n", reg2, reg1);
		g_print("\tsetle %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%%s\n", reg1);
		store_from_register(generator, assignment->destination, reg1);
		break;
	case LLIR_ASSIGNMENT_TYPE_GREATER_EQUAL:
			reg1 = operand_to_reg(generator, assignment->source, "r10");
			load_to_register(generator, assignment->left, reg1);
			reg2 = operand_to_reg(generator, assignment->source, "r11");
			load_to_register(generator, assignment->right, reg2);
		g_print("\tcmpq %%%s, %%%s\n", reg2, reg1);
		g_print("\tsetge %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%%s\n", reg1);
		store_from_register(generator, assignment->destination, reg1);
		break;
	case LLIR_ASSIGNMENT_TYPE_GREATER:
			reg1 = operand_to_reg(generator, assignment->source, "r10");
			load_to_register(generator, assignment->left, reg1);
			reg2 = operand_to_reg(generator, assignment->source, "r11");
			load_to_register(generator, assignment->right, reg2);
		g_print("\tcmpq %%%s, %%%s\n", reg2, reg1);
		g_print("\tsetg %%al\n");
		g_print("\tandb $1, %%al\n");
		g_print("\tmovzbq %%al, %%%s\n", reg1);
		store_from_register(generator, assignment->destination, reg1);
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
		generate_method_call(generator, assignment);
		break;
	default:
		g_assert(!"you fucked up");
		break;
	}
}

static void generate_branch(struct code_generator *generator,
			    struct llir_branch *branch)
{
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
	g_print(" block_%u\n", branch->false_block->id);
}

static void generate_jump(struct code_generator *generator,
			  struct llir_jump *jump, uint32_t next_block)
{
	if (generator->pinhole_optimize && (jump->block->id == next_block))
		return;
	g_print("\tjmp block_%u\n", jump->block->id);
}

static void generate_return(struct code_generator *generator,
			    struct llir_return *llir_return)
{
	load_to_register(generator, llir_return->source, "rax");

	g_print("\tmovq %%rbp, %%rsp\n");
	for (int32_t i = G_N_ELEMENTS(CALLEE_SAVED_REGISTERS) - 1; i >= 0; i--)
		g_print("\tpopq %%%s\n", CALLEE_SAVED_REGISTERS[i]);

	g_print("\tpopq %%rbp\n");
	g_print("\tret\n");
}

static void generate_shit_yourself(struct code_generator *generator,
				   struct llir_shit_yourself *exit)
{
	g_print("\tmovq $%lld, %%rdi\n", exit->return_value);
#ifdef __APPLE__
	g_print("\tcall _exit\n");
#else
	g_print("\tcall exit\n");
#endif
}

static void generate_method_body(struct code_generator *generator,
				 struct llir_method *method)
{
	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);
		g_print("block_%u:\n", block->id);

		for (uint32_t j = 0; j < block->assignments->len; j++) {
			struct llir_assignment *assignment =
				g_array_index(block->assignments,
					      struct llir_assignment *, j);
			generate_assignment(generator, assignment);
		}

		uint32_t next_block_id =
			(i < (method->blocks->len - 1)) ?
				g_array_index(method->blocks,
					      struct llir_block *, i + 1)
					->id :
				-1;

		switch (block->terminal_type) {
		case LLIR_BLOCK_TERMINAL_TYPE_JUMP:
			generate_jump(generator, block->jump, next_block_id);
			break;
		case LLIR_BLOCK_TERMINAL_TYPE_BRANCH:
			generate_branch(generator, block->branch);
			break;
		case LLIR_BLOCK_TERMINAL_TYPE_RETURN:
			generate_return(generator, block->llir_return);
			break;
		case LLIR_BLOCK_TERMINAL_TYPE_SHIT_YOURSELF:
			generate_shit_yourself(generator, block->shit_yourself);
			break;
		default:
			g_assert(!"you fucked up");
			break;
		}
	}
}

static void generate_text_section(struct code_generator *generator)
{
	g_print(".text\n");

	struct llir *llir = generator->llir;

	for (uint32_t i = 0; i < llir->methods->len; i++) {
		struct llir_method *method =
			g_array_index(llir->methods, struct llir_method *, i);
		generate_method_declaration(generator, method);
		generate_method_body(generator, method);
	}
}

struct code_generator *code_generator_new(bool pinhole_optimize)
{
	struct code_generator *generator = g_new(struct code_generator, 1);
	generator->offsets = g_hash_table_new(g_str_hash, g_str_equal);
	generator->pinhole_optimize = pinhole_optimize;
	return generator;
}

void code_generator_generate(struct code_generator *generator,
			     struct llir *llir)
{
	generator->llir = llir;
	generator->strings = g_hash_table_new(g_str_hash, g_str_equal);
	generator->variable_to_register =
		g_hash_table_new(g_str_hash, g_str_equal);
	generator->register_to_variable =
		g_hash_table_new(g_str_hash, g_str_equal);
	memcpy(generator->least_recently_used, CALLEE_SAVED_REGISTERS,
	       sizeof(CALLEE_SAVED_REGISTERS));
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
