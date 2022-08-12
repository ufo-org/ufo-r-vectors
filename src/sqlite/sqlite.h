#include <stdlib.h>
#include "../../include/ufos.h"
#include <sqlite3.h>

#pragma once

typedef enum {
    UFO_SQLITE_TEXT,
    UFO_SQLITE_INTEGER,
    UFO_SQLITE_BLOB,
    UFO_SQLITE_FLOAT,
    UFO_SQLITE_NULL,
} sqlite_type_t;

typedef struct {
    char  *database;
    char  *table;
    size_t column_count;
    size_t row_count;
    size_t capacity;
    char **names;
    sqlite_type_t *types;
} columns_info_t;

columns_info_t *columns_info_new(const char *database, const char *table, size_t column_count, size_t row_count);
void columns_info_free(columns_info_t *columns);
int columns_info_push(columns_info_t *columns, const char *name, const char *sql_type);
bool columns_info_exists(const columns_info_t *columns, const char *name);
int columns_info_type(const columns_info_t *columns, const char *name, ufo_vector_type_t *out);

columns_info_t *columns_info_from_sqlite(const char *db, const char *table);

typedef void (*sqlite_get_range_callback) (sqlite3_stmt */*statement*/, void */*user_data*/, size_t /*row*/);
void sqlite_get_range_int_callback(sqlite3_stmt *statement, void *data, size_t row);
void sqlite_get_range_real_callback(sqlite3_stmt *statement, void *data, size_t row);
void sqlite_get_range_text_callback(sqlite3_stmt *statement, void *data, size_t row);

int sqlite_get_range(const char *db, const char *table, const char *column, size_t start, size_t end, sqlite_get_range_callback callback, void *data);

typedef int (*sqlite_update_function)(sqlite3 *connection, const char *table, const char *column, const size_t *keys, const void *data, size_t length);
int sqlite_update(const char *db, const char *table, const char *columne, size_t start, size_t end, const void* values, sqlite_update_function updater);
int sqlite_get_table_indices(sqlite3 *connection, const char *table, const char *column, size_t *result, size_t start, size_t end);
int sqlite_update_integers(sqlite3 *connection, const char *table, const char *column, const size_t *keys, const void *data, size_t length);