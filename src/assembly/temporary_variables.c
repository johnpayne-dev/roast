#include "assembly/temporary_variables.h"

static uint32_t AVAILABLE_TEMPORARY_VARIABLE_COUNTER = 0;

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
