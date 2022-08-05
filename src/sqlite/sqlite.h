#include <stdlib.h>

#pragma once


void sqlite_table_columns(const char *db, const char *table, char ***column_names, size_t *column_count);