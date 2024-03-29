#include "sqlite.h" // My header for this file, do not confuse with sqlite3.h

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../safety_first.h"

#define MAX_QUERY_SIZE 1024
#define MAX_IDENTIFIER_SIZE 64
#define MAX_TABLE_COLUMNS 32

typedef int (*sqlite_callback) (void */*user_data*/, int /*argc*/, char **/*argv*/, char **/*column_name*/);

void sqlite_quote_identifier(const char *identifier, char *out) {
    sprintf(out, "`%s`", identifier);
}

columns_info_t *columns_info_new(const char *database, const char *table, size_t column_count, size_t row_count) {
    columns_info_t *columns = (columns_info_t *) malloc(sizeof(columns_info_t));

    columns->names = (char **) malloc(sizeof(char *) * column_count);
    columns->types = (sqlite_type_t *) malloc(sizeof(sqlite_type_t) * column_count);
    columns->column_count = 0;
    columns->row_count = row_count;
    columns->capacity = column_count;

    columns->database = (char *) malloc(sizeof(char) * (strlen(database) + 1));
    columns->table = (char *) malloc(sizeof(char) * (strlen(table) + 1));

    strcpy(columns->database, database);
    strcpy(columns->table, table);

    return columns;
}

void columns_info_free(columns_info_t *columns) {
    for (size_t i = 0; i < columns->column_count; i++) {
        free(columns->names[i]);
    }
    free(columns->database);
    free(columns->table);
    free(columns->names);
    free(columns->types);    
    free(columns);
}

int columns_info_push(columns_info_t *columns, const char *name, const char *sql_type) {
    make_sure(columns->column_count >= 0 && columns->column_count < columns->capacity, 
             "Index out of bounds 0 >= %ld < %ld.", columns->column_count, columns->capacity);
    
    columns->names[columns->column_count] = (char *) malloc(sizeof(char) * (strlen(name) + 1));
    if (columns->names[columns->column_count] == NULL) {
        return 1;
    }

    strcpy(columns->names[columns->column_count], name);
    
    if (0 == strcmp(sql_type, "INTEGER")) {
        columns->types[columns->column_count] = UFO_SQLITE_INTEGER;
    } else if (0 == strcmp(sql_type, "TEXT")) {
        columns->types[columns->column_count] = UFO_SQLITE_TEXT;
    } else if (0 == strcmp(sql_type, "BLOB")) {
        columns->types[columns->column_count] = UFO_SQLITE_BLOB;
    } else if (0 == strcmp(sql_type, "FLOAT") || 0 == strcmp(sql_type, "REAL")) {
        columns->types[columns->column_count] = UFO_SQLITE_FLOAT;
    } else if (0 == strcmp(sql_type, "NULL")) {
        columns->types[columns->column_count] = UFO_SQLITE_NULL;
    } else {
        fprintf(stderr, "Cannot parse column info for %s: unknown sql column type %s", name, sql_type);
        return 3;
    }

    columns->column_count += 1;
    return 0;
}

bool columns_info_exists(const columns_info_t *columns, const char *name) {
    for (size_t i = 0; i < columns->column_count; i++) {
        if (0 == strcmp(columns->names[i], name)) {
            return true;
        }
    }
    return false;
}

int columns_info_type(const columns_info_t *columns, const char *name, ufo_vector_type_t *out) {
    for (size_t i = 0; i < columns->column_count; i++) {
        if (0 == strcmp(columns->names[i], name)) {
            (*out) = columns->types[i];
            return 0;
        }
    }
    return 1;
}

void handle_sqlite_error_with_message(sqlite3 *connection, const char *query, const char *error_message) {
    size_t message_length = strlen(error_message);
    char private_error_message[message_length + 1];
    strcpy(private_error_message, error_message);

    fprintf(stderr, "Failed to execute query: %s\n%s\n", private_error_message, query);    
    
    sqlite3_free((void *) error_message);
    sqlite3_close(connection);

}

void handle_sqlite_error(sqlite3 *connection, const char *query) {
    const char *error_message = sqlite3_errmsg(connection);
    size_t message_length = strlen(error_message);
    char private_error_message[message_length + 1];
    strcpy(private_error_message, error_message);
    
    sqlite3_close(connection);
    
    fprintf(stderr, "Failed to execute query: %s\n%s\n", private_error_message, query);
}

int sqlite_count_results_callback(void *user_data, int argc, char **argv, char **column_name) {
    size_t *n = (size_t *) user_data; 
    printf("sqlite_count_results_callback (1): %ld",*n);    
    (*n) += 1;
    printf("sqlite_count_results_callback (2): %ld",*n);    
    return 0;
}

size_t columns_info_column_count_from_sqlite(sqlite3 *connection, const char *table)  {
    char quoted_table[MAX_IDENTIFIER_SIZE];
    char query[MAX_QUERY_SIZE];

    sqlite_quote_identifier(table, quoted_table);    
    sprintf(query, "PRAGMA table_info(%s)", quoted_table);    
    size_t columns = 0;

    char *error_message;
    int result_code = sqlite3_exec(connection, query, &sqlite_count_results_callback, &columns, &error_message);
    if (result_code != SQLITE_OK) {
        handle_sqlite_error_with_message(connection, query, error_message);
    }

    return columns;
}

size_t columns_info_row_count_from_sqlite(sqlite3 *connection, const char *table)  {
    char quoted_table[MAX_IDENTIFIER_SIZE];
    char query[MAX_QUERY_SIZE];

    sqlite_quote_identifier(table, quoted_table);    
    sprintf(query, "SELECT COUNT(*) FROM %s", quoted_table);    

    sqlite3_stmt *statement;
    int result_code = sqlite3_prepare_v2(connection, query, strlen(query), &statement, NULL);

    if (result_code != SQLITE_OK) {
        handle_sqlite_error(connection, query);
    }

    result_code = sqlite3_step(statement);
    
    if (result_code != SQLITE_ROW) {
        handle_sqlite_error(connection, query);
    }

    size_t rows = sqlite3_column_int64(statement, 0);

    result_code = sqlite3_finalize(statement);
    if (result_code != SQLITE_OK) {
        handle_sqlite_error(connection, query);
    }

    return rows;
}

int columns_info_from_sqlite_callback(void *user_data, int argc, char **argv, char **column_name) {
    columns_info_t *columns = (columns_info_t *) user_data; 

    bool found_name = false;
    bool found_type = false;

    char *name = NULL;
    char *sql_type = NULL;

    for (int i = 0; i < argc; i++) {
        printf("xxx %i %s=%s\n", i, column_name[i], argv[i]);
        if (0 == strcmp(column_name[i], "name")) {
            name = argv[i];
            found_name = true;
        }
        if (0 == strcmp(column_name[i], "type")) {
            sql_type = argv[i];
            found_type = true;
        }
    }

    if (!found_name || !found_type) return 1;  
    int result = columns_info_push(columns, name, sql_type);
    printf("xxx ===%i\n", result);
    return result;
}


columns_info_t *columns_info_from_sqlite(const char *db, const char *table)  {
    sqlite3 *connection;

    int result_code = sqlite3_open(db, &connection);
    if (result_code != SQLITE_OK) {
        const char *error_message = sqlite3_errmsg(connection);
        sqlite3_close(connection);
        fprintf(stderr, "Can't open database: %s\n", error_message);   
        return NULL;
    }

    size_t column_count = columns_info_column_count_from_sqlite(connection, table);
    size_t row_count = columns_info_row_count_from_sqlite(connection, table);
    columns_info_t *columns = columns_info_new(db, table, column_count, row_count);

    char quoted_table[MAX_IDENTIFIER_SIZE];
    char query[MAX_QUERY_SIZE];

    sqlite_quote_identifier(table, quoted_table);   
    printf("HERE?\n");
    sprintf(query, "PRAGMA table_info(%s)", quoted_table);

    char *error_message;
    result_code = sqlite3_exec(connection, query, &columns_info_from_sqlite_callback, columns, &error_message);
    if (result_code != SQLITE_OK) {
        handle_sqlite_error_with_message(connection, query, error_message);
    }

    sqlite3_close(connection);
    return columns;  
}

typedef void (*sqlite_get_range_callback) (sqlite3_stmt */*statement*/, void */*user_data*/, size_t /*row*/);

void sqlite_get_range_int_callback(sqlite3_stmt *statement, void *data, size_t row) {
    ((int *) data)[row] = sqlite3_column_int(statement, 0); // Assume single column at this point.
}

void sqlite_get_range_real_callback(sqlite3_stmt *statement, void *data, size_t row) {
    ((double *) data)[row] = sqlite3_column_double(statement, 0); // Assume single column at this point.
}

void sqlite_get_range_text_callback(sqlite3_stmt *statement, void *data, size_t row) {
    const char *string = (const char *) sqlite3_column_text(statement, 0); // Assume single column at this point.
    ((char **) data)[row] = (char *) malloc(sizeof(char) * (strlen(string) + 1)); // FIXME: allocate in arena.
    // FIXME error check
    strcpy(((char **) data)[row], string);
}

int sqlite_get_range(const char *db, const char *table, const char *column, size_t start, size_t end, sqlite_get_range_callback callback, void *data) {
    sqlite3 *connection;

    int result_code = sqlite3_open(db, &connection);
    if (result_code != SQLITE_OK) {
        const char *error_message = sqlite3_errmsg(connection);
        sqlite3_close(connection);
        fprintf(stderr, "Can't open database: %s\n", error_message); \
        return 1;  
    }

    char quoted_table[MAX_IDENTIFIER_SIZE];
    char quoted_column[MAX_IDENTIFIER_SIZE];
    char query[MAX_QUERY_SIZE];

    sqlite_quote_identifier(table, quoted_table);   
    sqlite_quote_identifier(column, quoted_column);   
    sprintf(query, 
        "SELECT %s FROM ( "
            "SELECT ROW_NUMBER() OVER(ORDER BY ROWID) __ufo_index, %s FROM %s "
        ") WHERE __ufo_index > %ld AND  __ufo_index <= %ld", 
        quoted_column, quoted_column, quoted_table, start, end
    );

    // char *error_message;
    sqlite3_stmt *statement;
    result_code = sqlite3_prepare_v2(connection, query, strlen(query), &statement, NULL);

    if (result_code != SQLITE_OK) {
        handle_sqlite_error(connection, query);
        return 2;
    }

    for (size_t row = 0; ; row++) {    
        result_code = sqlite3_step(statement);
        if (result_code == SQLITE_DONE) {
            break;
        }
        if (result_code != SQLITE_ROW) {
            result_code = sqlite3_finalize(statement);        
            handle_sqlite_error(connection, query);
            return 3;
        }       
        make_sure(sqlite3_column_count(statement) == 1, 
                  "Expected the SQL query to return a single column, but found %ld", 
                  sqlite3_column_count(statement));
        callback(statement, data, row);
    }

    result_code = sqlite3_finalize(statement);
    sqlite3_close(connection);

    if (result_code != SQLITE_OK) {
        handle_sqlite_error(connection, query);
        return 4;
    }    

    return 0;
}

int sqlite_get_table_indices(sqlite3 *connection, const char *table, const char *column, size_t *result, size_t start, size_t end) {
    char quoted_table[MAX_IDENTIFIER_SIZE];
    char quoted_column[MAX_IDENTIFIER_SIZE];
    char query[MAX_QUERY_SIZE];

    sqlite_quote_identifier(table, quoted_table);   
    sqlite_quote_identifier(column, quoted_column);   
    sprintf(query, 
        "SELECT __ufo_index, __table_index FROM ( "
            "SELECT ROW_NUMBER() OVER(ORDER BY ROWID) __ufo_index, "
                "ROWID as __table_index "
            "FROM %s "
        ") WHERE __ufo_index > %ld AND  __ufo_index <= %ld "
           "ORDER BY __ufo_index ",
        quoted_table, start, end
    );

    // char *error_message;
    sqlite3_stmt *statement;
    int result_code = sqlite3_prepare_v2(connection, query, strlen(query), &statement, NULL);

    if (result_code != SQLITE_OK) {
        handle_sqlite_error(connection, query);
        return 2;
    }

    size_t row;
    for (row = 0; ; row++) {    
        result_code = sqlite3_step(statement);
        if (result_code == SQLITE_DONE) {
            break;
        }
        if (result_code != SQLITE_ROW) {
            result_code = sqlite3_finalize(statement);        
            handle_sqlite_error(connection, query);
            return 3;
        }       
        make_sure(sqlite3_column_count(statement) == 2, 
                  "Expected the SQL query to return a single column, but found %ld", 
                  sqlite3_column_count(statement));

                
        size_t ufo_index = sqlite3_column_int64(statement, 0); 
        size_t table_index = sqlite3_column_int64(statement, 1); 

        fprintf(stderr, ":: ufo_index: %ld table_index: %ld at [%ld]\n", ufo_index, table_index, ufo_index - start - 1);

        result[ufo_index - start - 1] = table_index;
    }
    make_sure(row == (end - start),
              "Expected the SQL query to return %ld rows, but it returned %ld", 
              row, end - start);

    result_code = sqlite3_finalize(statement);
    // sqlite3_close(connection);

    if (result_code != SQLITE_OK) {
        handle_sqlite_error(connection, query);
        return 4;
    }    

    return 0;
}

int sqlite_update_via_statement(sqlite3 *connection, const char *query) {    
    sqlite3_stmt *statement;
    int result_code = sqlite3_prepare_v2(connection, query, strlen(query), &statement, NULL);

    if (result_code != SQLITE_OK) {
        handle_sqlite_error(connection, query);
        return 2;
    }
    result_code = sqlite3_step(statement);

    if (result_code != SQLITE_DONE) {
        result_code = sqlite3_finalize(statement);        
        handle_sqlite_error(connection, query);
        return 3;
    }  

    result_code = sqlite3_finalize(statement);
    if (result_code != SQLITE_OK) {
        handle_sqlite_error(connection, query);
    }

    return 0;
}

int sqlite_update_string(sqlite3 *connection, const char *table, const char *column, size_t index, const char *value) {
    char quoted_table[MAX_IDENTIFIER_SIZE];
    char quoted_column[MAX_IDENTIFIER_SIZE];
    char query[MAX_QUERY_SIZE];

    sqlite_quote_identifier(table, quoted_table);   
    sqlite_quote_identifier(column, quoted_column);
    sprintf(query, 
        "UPDATE %s SET %s = \"%s\" WHERE ROWID == \"%ld\"", 
        quoted_table, quoted_column, value, index
    );

    return sqlite_update_via_statement(connection, query);
}

int sqlite_update_double(sqlite3 *connection, const char *table, const char *column, size_t index, double value) {
    char quoted_table[MAX_IDENTIFIER_SIZE];
    char quoted_column[MAX_IDENTIFIER_SIZE];
    char query[MAX_QUERY_SIZE];

    sqlite_quote_identifier(table, quoted_table);   
    sqlite_quote_identifier(column, quoted_column);
    sprintf(query, 
        "UPDATE %s SET %s = \"%f\" WHERE ROWID == \"%ld\"", 
        quoted_table, quoted_column, value, index
    );

    return sqlite_update_via_statement(connection, query);
}

int sqlite_update_integer(sqlite3 *connection, const char *table, const char *column, size_t index, int value) {    
    char quoted_table[MAX_IDENTIFIER_SIZE];
    char quoted_column[MAX_IDENTIFIER_SIZE];
    char query[MAX_QUERY_SIZE];

    sqlite_quote_identifier(table, quoted_table);   
    sqlite_quote_identifier(column, quoted_column);
    sprintf(query, 
        "UPDATE %s SET %s = \"%d\" WHERE ROWID == \"%ld\"", 
        quoted_table, quoted_column, value, index
    );

    return sqlite_update_via_statement(connection, query);
}

int sqlite_update(const char *db, const char *table, const char *column, size_t start, size_t end, const void *values, sqlite_update_function updater) {
    sqlite3 *connection;

    int result_code = sqlite3_open(db, &connection);
    if (result_code != SQLITE_OK) {
        const char *error_message = sqlite3_errmsg(connection);
        sqlite3_close(connection);
        fprintf(stderr, "Can't open database: %s\n", error_message); 
        return 1;
    }

    size_t length = end - start;
    size_t keys[length + 1];    

    result_code = sqlite_get_table_indices(connection, table, column, keys, start, end);
    if (result_code != 0) {
        return result_code;
    }

    result_code = updater(connection, table, column, keys, values, length);

    sqlite3_close(connection);
    return result_code;
}

int sqlite_update_integers(sqlite3 *connection, const char *table, const char *column, const size_t *keys, const void *data, size_t length) {
    int *values = (int *) data;
    for (size_t i = 0; i < length; i++) {
        fprintf(stderr, "[%ld] Updating 1 %s[%ld] to %d\n", i, column, keys[i], values[i]);
        int result = sqlite_update_integer(connection, table, column, keys[i], values[i]);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

int sqlite_update_doubles(sqlite3 *connection, const char *table, const char *column, const size_t *keys, const void *data, size_t length) {
    double *values = (double *) data;
    for (size_t i = 0; i < length; i++) {
        fprintf(stderr, "[%ld] Updating 2 %s[%ld] to %f\n", i, column, keys[i], values[i]);
        int result = sqlite_update_double(connection, table, column, keys[i], values[i]);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}