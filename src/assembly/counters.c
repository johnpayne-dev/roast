#include "assembly/counters.h"

static uint32_t AVAILABLE_TEMPORARY_VARIABLE_COUNTER = 0;

static uint32_t AVAILABLE_LABEL_COUNTER = 0;

static char *counter_to_string(uint32_t counter)
{
	return g_strdup_printf("$%u", counter);
}

char *last_temporary_variable(void)
{
	return counter_to_string(AVAILABLE_TEMPORARY_VARIABLE_COUNTER - 1);
}

char *peek_temporary_variable(void)
{
	return counter_to_string(AVAILABLE_TEMPORARY_VARIABLE_COUNTER);
}

char *next_temporary_variable(void)
{
	return counter_to_string(AVAILABLE_TEMPORARY_VARIABLE_COUNTER++);
}

static char *label_counter_to_string(uint32_t counter)
{
	return g_strdup_printf("label_%i", counter);
}

char *next_label(void)
{
	return label_counter_to_string(AVAILABLE_LABEL_COUNTER++);
}
