#include "assembly/ssa.h"

struct ssa_context {
	GArray *defined_fields;
	GHashTable *counters;
	GHashTable *new_fields;
};

static bool rename_operand(struct ssa_context *ssa,
			   struct llir_operand *operand)
{
	if (operand->type != LLIR_OPERAND_TYPE_FIELD)
		return false;

	uint64_t counter =
		(uint64_t)g_hash_table_lookup(ssa->counters, operand->field);
	if (counter <= 1)
		return false;

	struct llir_field *field =
		g_hash_table_lookup(ssa->new_fields, operand->field);
	operand->field = field->identifier;
	return true;
}

static void rename_assignment_operands(struct ssa_context *ssa,
				       struct llir_assignment *assignment)
{
	if (assignment->type == LLIR_ASSIGNMENT_TYPE_PHI)
		return;

	if (assignment->type == LLIR_ASSIGNMENT_TYPE_MOVE ||
	    assignment->type == LLIR_ASSIGNMENT_TYPE_NOT ||
	    assignment->type == LLIR_ASSIGNMENT_TYPE_NEGATE) {
		rename_operand(ssa, &assignment->source);
	} else if (assignment->type == LLIR_ASSIGNMENT_TYPE_ARRAY_ACCESS) {
		rename_operand(ssa, &assignment->access_index);
	} else if (assignment->type == LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE) {
		rename_operand(ssa, &assignment->update_index);
		rename_operand(ssa, &assignment->update_value);
	} else if (assignment->type == LLIR_ASSIGNMENT_TYPE_METHOD_CALL) {
		for (uint32_t i = 0; i < assignment->argument_count; i++)
			rename_operand(ssa, &assignment->arguments[i]);
	} else {
		rename_operand(ssa, &assignment->left);
		rename_operand(ssa, &assignment->right);
	}
}

static void rename_destination(struct ssa_context *ssa,
			       struct llir_block *block,
			       struct llir_assignment *assignment)
{
	if (assignment->type == LLIR_ASSIGNMENT_TYPE_ARRAY_UPDATE)
		return;

	uint64_t counter = (uint64_t)g_hash_table_lookup(
		ssa->counters, assignment->destination);
	g_hash_table_insert(ssa->counters, assignment->destination,
			    (gpointer)(counter + 1));

	if (counter == 0)
		return;

	char *renamed_identifier =
		g_strdup_printf("%s_%llu", assignment->destination, counter);

	struct llir_field *field =
		llir_field_new(renamed_identifier, 0, false, 1);
	llir_block_add_field(block, field);
	g_hash_table_insert(ssa->new_fields, assignment->destination, field);

	assignment->destination = field->identifier;

	g_free(renamed_identifier);
}

static void rename_phi_operands(struct ssa_context *ssa,
				struct llir_block *block,
				struct llir_block *next_block)
{
	for (uint32_t i = 0; i < next_block->assignments->len; i++) {
		struct llir_assignment *assignment = g_array_index(
			next_block->assignments, struct llir_assignment *, i);

		if (assignment->type != LLIR_ASSIGNMENT_TYPE_PHI)
			continue;

		for (uint32_t j = 0; j < assignment->phi_arguments->len; j++) {
			struct llir_operand *operand =
				&g_array_index(assignment->phi_arguments,
					       struct llir_operand, j);
			struct llir_block **phi_block = &g_array_index(
				assignment->phi_blocks, struct llir_block *, j);

			if (*phi_block == NULL &&
			    rename_operand(ssa, operand)) {
				*phi_block = block;
				break;
			}

			if (j == assignment->phi_arguments->len - 1) {
				for (int32_t k = j; k >= 0; k--) {
					struct llir_block **phi_block =
						&g_array_index(
							assignment->phi_blocks,
							struct llir_block *, k);
					if (*phi_block == NULL) {
						*phi_block = block;
						break;
					}
				}
			}
		}
	}
}

static void transform_block(struct ssa_context *ssa, struct llir_block *block)
{
	for (uint32_t i = 0; i < block->assignments->len; i++) {
		struct llir_assignment *assignment = g_array_index(
			block->assignments, struct llir_assignment *, i);

		rename_assignment_operands(ssa, assignment);
		rename_destination(ssa, block, assignment);
	}

	if (block->terminal_type == LLIR_BLOCK_TERMINAL_TYPE_JUMP) {
		rename_phi_operands(ssa, block, block->jump->block);
	} else if (block->terminal_type == LLIR_BLOCK_TERMINAL_TYPE_BRANCH) {
		rename_operand(ssa, &block->branch->left);
		rename_operand(ssa, &block->branch->right);
		rename_phi_operands(ssa, block, block->branch->true_block);
		rename_phi_operands(ssa, block, block->branch->false_block);
	} else if (block->terminal_type == LLIR_BLOCK_TERMINAL_TYPE_RETURN) {
		rename_operand(ssa, &block->llir_return->source);
	}
}

static void add_defined_fields(struct ssa_context *ssa, GArray *fields)
{
	for (uint32_t i = 0; i < fields->len; i++) {
		struct llir_field *field =
			g_array_index(fields, struct llir_field *, i);
		if (field->identifier[0] == '$')
			continue;
		g_array_append_val(ssa->defined_fields, field->identifier);
	}
}

static void create_phi_assignments(struct ssa_context *ssa,
				   struct llir_block *block)
{
	for (uint32_t i = 0; i < ssa->defined_fields->len; i++) {
		char *field = g_array_index(ssa->defined_fields, char *, i);
		struct llir_assignment *phi = llir_assignment_new_phi(field);

		for (uint32_t j = 0; j < block->predecessors->len; j++) {
			struct llir_operand operand =
				llir_operand_from_field(field);
			llir_assignment_add_phi_argument(phi, operand, NULL);
		}

		llir_block_prepend_assignment(block, phi);
	}
}

static void transform_method(struct ssa_context *ssa,
			     struct llir_method *method)
{
	ssa->defined_fields = g_array_new(false, false, sizeof(char *));
	ssa->counters = g_hash_table_new(g_str_hash, g_str_equal);
	ssa->new_fields = g_hash_table_new(g_str_hash, g_str_equal);

	add_defined_fields(ssa, method->arguments);

	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);

		if (block->predecessors->len > 1)
			create_phi_assignments(ssa, block);

		add_defined_fields(ssa, block->fields);
	}

	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);
		transform_block(ssa, block);
	}

	g_array_free(ssa->defined_fields, true);
	g_hash_table_unref(ssa->counters);
	g_hash_table_unref(ssa->new_fields);
}

static void transform_program(struct ssa_context *ssa, struct llir *llir)
{
	for (uint32_t i = 0; i < llir->methods->len; i++) {
		struct llir_method *method =
			g_array_index(llir->methods, struct llir_method *, i);
		transform_method(ssa, method);
	}
}

void ssa_transform(struct llir *llir)
{
	struct ssa_context ssa;
	transform_program(&ssa, llir);
}

static void remove_phi_operands(struct ssa_context *ssa,
				struct llir_assignment *phi)
{
	for (uint32_t i = 0; i < phi->phi_arguments->len; i++) {
		struct llir_operand operand = g_array_index(
			phi->phi_arguments, struct llir_operand, i);
		struct llir_block *block =
			g_array_index(phi->phi_blocks, struct llir_block *, i);

		g_assert(block != NULL);

		struct llir_field *field =
			llir_field_new(phi->destination, 0, false, 1);
		llir_block_add_field(block, field);
		struct llir_assignment *assignment = llir_assignment_new_unary(
			LLIR_ASSIGNMENT_TYPE_MOVE, operand, field->identifier);
		llir_block_add_assignment(block, assignment);

		g_array_remove_index(phi->phi_arguments, i);
		g_array_remove_index(phi->phi_blocks, i);
		i--;
	}
}

static void inverse_transform_block(struct ssa_context *ssa,
				    struct llir_block *block)
{
	for (uint32_t i = 0; i < block->assignments->len; i++) {
		struct llir_assignment *assignment = g_array_index(
			block->assignments, struct llir_assignment *, i);

		if (assignment->type != LLIR_ASSIGNMENT_TYPE_PHI)
			continue;

		remove_phi_operands(ssa, assignment);
	}
}

static void remove_phi_assignments(struct ssa_context *ssa,
				   struct llir_block *block)
{
	for (uint32_t i = 0; i < block->assignments->len; i++) {
		struct llir_assignment *assignment = g_array_index(
			block->assignments, struct llir_assignment *, i);

		if (assignment->type == LLIR_ASSIGNMENT_TYPE_PHI)
			g_array_remove_index(block->assignments, i--);
	}
}

static void inverse_transform_method(struct ssa_context *ssa,
				     struct llir_method *method)
{
	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);
		inverse_transform_block(ssa, block);
	}

	for (uint32_t i = 0; i < method->blocks->len; i++) {
		struct llir_block *block =
			g_array_index(method->blocks, struct llir_block *, i);
		remove_phi_assignments(ssa, block);
	}
}

static void inverse_transform_program(struct ssa_context *ssa,
				      struct llir *llir)
{
	for (uint32_t i = 0; i < llir->methods->len; i++) {
		struct llir_method *method =
			g_array_index(llir->methods, struct llir_method *, i);
		inverse_transform_method(ssa, method);
	}
}

void ssa_inverse_transform(struct llir *llir)
{
	struct ssa_context ssa;
	inverse_transform_program(&ssa, llir);
}
