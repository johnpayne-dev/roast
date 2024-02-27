#include <stdio.h>
#include <stdbool.h>
#include <glib.h>

#include "scanner/scanner.h"

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
		G_OPTION_ENTRY_NULL,
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

static int run_scan_target(char *source)
{
	struct scanner *scanner = scanner_new();

	int result = scanner_tokenize(scanner, source, true, NULL);

	scanner_free(scanner);
	return result;
}

static int run_parse_target(char *source)
{
	g_printerr("parse target not implemented yet.\n");
	return -1;
}

static int run_intermediate_target(char *source)
{
	g_printerr("inter target not implemented yet.\n");
	return -1;
}

static int run_assembly_target(char *source)
{
	g_printerr("assembly target not implemented yet.\n");
	return -1;
}

static int run_target(char *target, char *source)
{
	if (target == NULL)
		return run_assembly_target(source);

	if (g_strcmp0(target, "scan") == 0)
		return run_scan_target(source);
	if (g_strcmp0(target, "parse") == 0)
		return run_parse_target(source);
	if (g_strcmp0(target, "inter") == 0)
		return run_intermediate_target(source);
	if (g_strcmp0(target, "assembly") == 0)
		return run_assembly_target(source);

	g_printerr("Unknown target '%s' passed in as option.\n", target);
	return -1;
}

int main(int argc, char *argv[])
{
	struct options options;
	if (parse_options(&argc, &argv, &options) != 0)
		return -1;

	if (set_output_file(options.output_file) != 0)
		goto error_cleanup_options;

	char *file_contents;
	if (get_input_file_contents(argc, argv, &file_contents) != 0)
		goto error_cleanup_options;

	if (run_target(options.target, file_contents) != 0)
		goto error_cleanup;

	return 0;

error_cleanup:
	g_free(file_contents);
error_cleanup_options:
	free_options(&options);
	return -1;
}
