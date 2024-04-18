#include <stdio.h>
#include <stdbool.h>
#include <glib.h>

#include "scanner/scanner.h"
#include "parser/parser.h"
#include "semantics/semantics.h"
#include "assembly/llir_generator.h"
#include "assembly/code_generator.h"

enum target {
	TARGET_SCAN,
	TARGET_PARSE,
	TARGET_INTER,
	TARGET_ASSEMBLY,
};

enum optimzation {
	OPTIMIZATION_CSE = 1 << 1,
	OPTIMIZATION_CP = 1 << 2,
	OPTIMIZATION_DCE = 1 << 3,
	OPTIMIZATION_ALL = ~0,
};

struct options {
	enum target target;
	char *output_file;
	enum optimzation optimizations;
	bool debug;
	char *input_file;
};

static void free_options(struct options *options)
{
	g_free(options->output_file);
	g_free(options->input_file);
}

static int parse_target(char *target, struct options *options)
{
	if (target == NULL) {
		options->target = TARGET_ASSEMBLY;
	} else if (g_strcmp0(target, "scan") == 0) {
		options->target = TARGET_SCAN;
	} else if (g_strcmp0(target, "parse") == 0) {
		options->target = TARGET_PARSE;
	} else if (g_strcmp0(target, "inter") == 0) {
		options->target = TARGET_INTER;
	} else if (g_strcmp0(target, "assembly") == 0) {
		options->target = TARGET_ASSEMBLY;
	} else {
		g_printerr("Unknown target '%s' passed in as option.\n",
			   target);
		return -1;
	}

	return 0;
}

static int parse_optimizations(char *optimizations, struct options *options)
{
	options->optimizations = 0;

	if (optimizations == NULL)
		return 0;

	char **optimization_list = g_strsplit(optimizations, ",", -1);
	for (uint32_t i = 0; optimization_list[i] != NULL; i++) {
		char *optimization = optimization_list[i];
		if (g_strcmp0(optimization, "cse") == 0)
			options->optimizations |= OPTIMIZATION_CSE;
		else if (g_strcmp0(optimization, "-cse") == 0)
			options->optimizations &= ~OPTIMIZATION_CSE;
		else if (g_strcmp0(optimization, "cp") == 0)
			options->optimizations |= OPTIMIZATION_CP;
		else if (g_strcmp0(optimization, "-cp") == 0)
			options->optimizations &= ~OPTIMIZATION_CP;
		else if (g_strcmp0(optimization, "dce") == 0)
			options->optimizations |= OPTIMIZATION_DCE;
		else if (g_strcmp0(optimization, "-dce") == 0)
			options->optimizations &= ~OPTIMIZATION_DCE;
		else if (g_strcmp0(optimization, "all") == 0)
			options->optimizations |= OPTIMIZATION_ALL;
		else if (g_strcmp0(optimization, "-all") == 0)
			options->optimizations &= ~OPTIMIZATION_ALL;
		else {
			g_printerr("Unknown optimizations '%s' passed in",
				   optimization);
			g_free(optimization_list);
			return -1;
		}
	}

	g_free(optimization_list);
	return 0;
}

static int parse_options(int argc, char **argv, struct options *options)
{
	char *target = NULL;
	char *output_file = NULL;
	char *optimizations = NULL;
	bool debug = false;

	const GOptionEntry option_entries[] = {
		{
			.long_name = "target",
			.short_name = 't',
			.flags = 0,
			.arg = G_OPTION_ARG_STRING,
			.arg_data = (void *)&target,
			.description =
				"<stage> is one of 'scan', 'parse', 'inter', or 'assembly'.",
			.arg_description = "<stage>",
		},
		{
			.long_name = "output",
			.short_name = 'o',
			.flags = 0,
			.arg = G_OPTION_ARG_FILENAME,
			.arg_data = (void *)&output_file,
			.description = "Writes output to <output>",
			.arg_description = "<output>",
		},
		{
			.long_name = "optimizations",
			.short_name = 'O',
			.flags = 0,
			.arg = G_OPTION_ARG_STRING,
			.arg_data = (void *)&optimizations,
			.description =
				"<optimization> is one of 'cse', 'cp', 'dce' or 'all'.",
			.arg_description = "<optimization>,...",
		},
		{
			.long_name = "debug",
			.short_name = 'd',
			.flags = 0,
			.arg = G_OPTION_ARG_NONE,
			.arg_data = (void *)&debug,
			.description = "Print debugging information.",
			.arg_description = NULL,
		},
		{ 0 },
	};

	GOptionContext *context = g_option_context_new("- decaf compiler.");
	g_option_context_add_main_entries(context, option_entries, NULL);

	int result = 0;

	GError *error = NULL;
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_printerr("Failed to parse options: %s\n", error->message);
		return -1;
	}

	if (argc != 2) {
		g_printerr(argc < 2 ? "No input file passed in.\n" :
				      "Too many input files passed in.\n");
		options->input_file = NULL;
		result = -1;
	} else {
		options->input_file = g_strdup(argv[1]);
	}

	options->output_file = output_file;
	options->debug = debug;

	if (parse_target(target, options) != 0)
		result = -1;

	if (parse_optimizations(optimizations, options) != 0)
		result = -1;

	g_free(target);
	g_free(optimizations);
	g_option_context_free(context);
	return result;
}

static int set_output_file(char *output_file)
{
	if (output_file != NULL && freopen(output_file, "w", stdout) == NULL) {
		g_printerr("Failed to set stdout to file %s\n", output_file);
		return -1;
	}

	return 0;
}

static int get_input_file_contents(char *input_file, char **file_contents)
{
	GError *error = NULL;
	if (!g_file_get_contents(input_file, file_contents, NULL, &error)) {
		g_printerr("%s\n", error->message);
		return -1;
	}

	return 0;
}

static int run_scan_target(char *file_name, char *source, bool print_output,
			   GArray **token_list)
{
	struct scanner *scanner = scanner_new();

	int result = scanner_tokenize(scanner, file_name, source, print_output,
				      token_list);

	scanner_free(scanner);
	return result;
}

static int run_parse_target(char *file_name, char *source,
			    struct ast_node **ast)
{
	GArray *tokens;
	if (run_scan_target(file_name, source, false, &tokens) != 0)
		return -1;

	struct parser *parser = parser_new();

	int result = parser_parse(parser, tokens, ast);

	parser_free(parser);
	g_array_free(tokens, true);
	return result;
}

static int run_intermediate_target(char *file_name, char *source,
				   struct ir_program **ir)
{
	struct ast_node *ast;
	if (run_parse_target(file_name, source, &ast) != 0)
		return -1;

	struct semantics *semantics = semantics_new();

	int result = semantics_analyze(semantics, ast, ir);

	semantics_free(semantics);
	ast_node_free(ast);
	return result;
}

static int run_assembly_target(char *file_name, char *source, bool debug)
{
	struct ir_program *ir;
	if (run_intermediate_target(file_name, source, &ir) != 0)
		return -1;

	struct llir_generator *llir_generator = llir_generator_new();
	struct llir_node *llir = llir_generator_generate_llir(llir_generator, ir);
	llir_generator_free(llir_generator);

	if (debug) {
		llir_node_print(llir);
	} else {
		struct code_generator *generator = code_generator_new();
		code_generator_generate(generator, llir);
		code_generator_free(generator);
	}

	llir_node_free(llir);
	ir_program_free(ir);
	return 0;
}

static int run_target(struct options *options, char *source)
{
	switch (options->target) {
	case TARGET_SCAN:
		return run_scan_target(options->input_file, source, true, NULL);
	case TARGET_PARSE:
		return run_parse_target(options->input_file, source, NULL);
	case TARGET_INTER:
		return run_intermediate_target(options->input_file, source,
					       NULL);
	case TARGET_ASSEMBLY:
		return run_assembly_target(options->input_file, source,
					   options->debug);
	default:
		g_assert(!"Unknown target");
		return -1;
	}
}

int main(int argc, char *argv[])
{
	struct options options = { 0 };
	char *file_contents = NULL;

	if (parse_options(argc, argv, &options) != 0)
		goto error_cleanup;

	if (set_output_file(options.output_file) != 0)
		goto error_cleanup;

	if (get_input_file_contents(options.input_file, &file_contents) != 0)
		goto error_cleanup;

	if (run_target(&options, file_contents) != 0)
		goto error_cleanup;

	return 0;

error_cleanup:
	g_free(file_contents);
	free_options(&options);
	return -1;
}
