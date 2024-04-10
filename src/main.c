#include <stdio.h>
#include <stdbool.h>
#include <glib.h>

#include "scanner/scanner.h"
#include "parser/parser.h"
#include "semantics/semantics.h"
#include "assembly/assembly.h"
#include "assembly/code_generator.h"

struct options {
	char *target;
	char *output_file;
	bool debug;
};

static int parse_options(int *argc, char ***argv, struct options *options)
{
	char *target = NULL;
	char *output_file = NULL;
	bool debug = false;

	const GOptionEntry option_entries[] = {
		{
			.long_name = "target",
			.short_name = 't',
			.flags = 0,
			.arg = G_OPTION_ARG_STRING,
			.arg_data = (void *)&target,
			.description =
				"<stage> is one of scan, parse, inter, or assembly.",
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

	GError *error = NULL;
	if (!g_option_context_parse(context, argc, argv, &error)) {
		g_printerr("Failed to parse options: %s\n", error->message);
		g_free(target);
		g_free(output_file);
		g_option_context_free(context);
		return -1;
	}

	options->target = target;
	options->output_file = output_file;
	options->debug = debug;
	return 0;
}

static void free_options(struct options *options)
{
	g_free(options->target);
	g_free(options->output_file);
}

static int set_output_file(char *output_file)
{
	if (output_file != NULL && freopen(output_file, "w", stdout) == NULL) {
		g_printerr("Failed to set stdout to file %s\n", output_file);
		return -1;
	}

	return 0;
}

static int get_input_file_contents(int argc, char *argv[], char **file_contents)
{
	if (argc != 2) {
		g_printerr(argc < 2 ? "No input file passed in.\n" :
				      "Too many input files passed in.\n");
		return -1;
	}

	GError *error = NULL;
	if (!g_file_get_contents(argv[1], file_contents, NULL, &error)) {
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

static int run_assembly_target(char *file_name, char *source)
{
	struct ir_program *ir;
	if (run_intermediate_target(file_name, source, &ir) != 0)
		return -1;

	struct assembly *assembly = assembly_new();
	struct llir_node *llir = assembly_generate_llir(assembly, ir);
	assembly_free(assembly);

	struct code_generator *generator = code_generator_new();
	code_generator_generate(generator, llir);
	code_generator_free(generator);

	llir_node_free(llir);
	ir_program_free(ir);
	return 0;
}

static int run_target(char *target, char *file_name, char *source)
{
	if (target == NULL)
		return run_assembly_target(file_name, source);

	if (g_strcmp0(target, "scan") == 0)
		return run_scan_target(file_name, source, true, NULL);
	if (g_strcmp0(target, "parse") == 0)
		return run_parse_target(file_name, source, NULL);
	if (g_strcmp0(target, "inter") == 0)
		return run_intermediate_target(file_name, source, NULL);
	if (g_strcmp0(target, "assembly") == 0)
		return run_assembly_target(file_name, source);

	g_printerr("Unknown target '%s' passed in as option.\n", target);
	return -1;
}

int main(int argc, char *argv[])
{
	struct options options = { 0 };
	char *file_contents = NULL;

	if (parse_options(&argc, &argv, &options) != 0)
		goto error_cleanup;

	if (set_output_file(options.output_file) != 0)
		goto error_cleanup;

	if (get_input_file_contents(argc, argv, &file_contents) != 0)
		goto error_cleanup;

	if (run_target(options.target, argv[1], file_contents) != 0)
		goto error_cleanup;

	return 0;

error_cleanup:
	g_free(file_contents);
	free_options(&options);
	return -1;
}
