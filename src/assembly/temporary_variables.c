#include "assembly/temporary_variables.h"

static uint32_t TEMP_VARAIBLE_COUNTER = 0;

static char *counter_to_string(uint32_t counter)
{
	return g_strdup_printf("$%u", counter);
}

char *peek_temporary_variable(void)
{
	return counter_to_string(TEMP_VARAIBLE_COUNTER);
}

char *next_temporary_variable(void)
{
	return counter_to_string(TEMP_VARAIBLE_COUNTER++);
}
