#include "scanner/scanner.h"

static GString *merge_regex_patterns(void)
{
	GString *merged_pattern = g_string_new("");

	for (uint32_t i = 0; i < TOKEN_TYPE_COUNT; i++) {
		const char *pattern = token_type_regex_pattern(i);
		g_string_append(merged_pattern, "(");
		g_string_append(merged_pattern, pattern);
		g_string_append(merged_pattern, ")");
		if (i != TOKEN_TYPE_COUNT - 1)
			g_string_append(merged_pattern, "|");
	}

	return merged_pattern;
}

struct scanner *scanner_new(void)
{
	struct scanner *scanner = g_new(struct scanner, 1);

	GString *merged_pattern = merge_regex_patterns();

	GError *error = NULL;
	scanner->regex = g_regex_new(merged_pattern->str,
				     G_REGEX_MULTILINE | G_REGEX_OPTIMIZE, 0,
				     &error);
	if (error != NULL)
		g_error("%s", error->message);

	g_string_free(merged_pattern, true);
	return scanner;
}

void scanner_start(struct scanner *scanner, const char *source)
{
	scanner->source = source;
	scanner->position = 0;
}

static enum token_type get_matched_token_type(GMatchInfo *match_info,
					      int32_t *start, int32_t *end)
{
	for (uint32_t i = 0; i < TOKEN_TYPE_COUNT; i++) {
		g_match_info_fetch_pos(match_info, i + 1, start, end);
		if (*start != -1)
			return i;
	}

	return TOKEN_TYPE_UNKNOWN;
}

static enum token_type match_token(struct scanner *scanner, uint32_t *length)
{
	const char *source = scanner->source + scanner->position;

	GMatchInfo *match_info = NULL;
	if (!g_regex_match(scanner->regex, source, 0, &match_info)) {
		*length = (uint32_t)strlen(source);
		return TOKEN_TYPE_UNKNOWN;
	}

	int32_t start, end;
	enum token_type token_type =
		get_matched_token_type(match_info, &start, &end);
	g_match_info_free(match_info);

	if (start != 0) {
		*length = start;
		return TOKEN_TYPE_UNKNOWN;
	}

	*length = end;
	return token_type;
}

bool scanner_next_token(struct scanner *scanner, struct token *token)
{
	if (scanner->source[scanner->position] == '\0')
		return false;

	token->type = match_token(scanner, &token->length);
	token->offset = scanner->position;
	token->source = scanner->source;
	scanner->position += token->length;
	return true;
}

int scanner_tokenize(struct scanner *scanner, const char *source,
		     bool print_output, GArray **out_tokens)
{
	int result = 0;
	scanner_start(scanner, source);

	GArray *tokens = g_array_new(false, false, sizeof(struct token));

	struct token token;
	while (scanner_next_token(scanner, &token)) {
		if (token_type_is_ignored(token.type))
			continue;

		if (token_type_is_error(token.type)) {
			token_print_error(&token);
			result = -1;
			continue;
		}

		if (print_output)
			token_print(&token);

		g_array_append_val(tokens, token);
	}

	if (out_tokens != NULL && result == 0)
		*out_tokens = tokens;
	else
		g_array_free(tokens, true);

	return result;
}

void scanner_free(struct scanner *scanner)
{
	g_regex_unref(scanner->regex);
	g_free(scanner);
}
