#include "sqlite.h" // My header for this file, do not confuse with sqlite3.h

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Rinternals.h"

#define MAX_QUERY_SIZE 256
#define MAX_IDENTIFIER_SIZE 128
#define MAX_TABLE_COLUMNS 32

typedef int (*sqlite_callback) (void */*user_data*/, int /*argc*/, char **/*argv*/, char **/*column_name*/);

void sqlite_quote_identifier(const char *identifier, char *out) {
    sprintf(out, "`%s`", identifier);
}

typedef struct {
    size_t count;
    char names[MAX_IDENTIFIER_SIZE][MAX_TABLE_COLUMNS];
} columns_info;

int sqlite_table_columns_callback(void *user_data, int argc, char **argv, char **column_name) {
    columns_info *columns = (columns_info *) user_data; 
    for (int i = 0; i < argc; i++) {
        if (0 == strcmp(column_name[i], "name")) {
            strcpy(columns->names[columns->count], argv[i]);
            columns->count += 1;
        }
    }
    return 0;
}

void sqlite_table_columns(const char *db, const char *table, char ***column_names, size_t *column_count)  {
    sqlite3 *connection;

    int result_code = sqlite3_open(db, &connection);
    if (result_code != SQLITE_OK) {
        const char *error_message = sqlite3_errmsg(connection);
        sqlite3_close(connection);
        Rf_error("Can't open database: %s\n", error_message);
        return;
    }

    char quoted_table[MAX_IDENTIFIER_SIZE];
    sqlite_quote_identifier(table, quoted_table);

    char query[MAX_QUERY_SIZE];
    sprintf(query, "PRAGMA table_info(%s)", quoted_table);
    
    columns_info columns;
    columns.count = 0;

    char *error_message;
    result_code = sqlite3_exec(connection, query, &sqlite_table_columns_callback, &columns, &error_message);

    if (result_code != SQLITE_OK) {        
        
        size_t message_length = strlen(error_message);
        char private_error_message[message_length];
        strcpy(private_error_message, error_message);
        
        sqlite3_free((void *) error_message);
        sqlite3_close(connection);
        
        Rf_error("Failed to execute query: %s\n", error_message);
        
        return;
    }

    (*column_count) = columns.count;
    (*column_names) = (char **) malloc(sizeof(char *) * columns.count);
    for (size_t i = 0; i < columns.count; i++) {
        (*column_names)[i] = malloc(sizeof(char) * strlen(columns.names[i]));        
        strcpy((*column_names)[i], columns.names[i]);
    }   
}

// sqlite_column_type <- function(db, table, column, ...)  {
//     connection <- do.call(DBI::dbConnect, c(drv = RSQLite::SQLite(), db, ...))
//     result <- DBI::dbSendQuery(connection, paste0("PRAGMA table_info(", DBI::dbQuoteIdentifier(connection, table), ")"))
//     columns <- DBI::dbFetch(result)
//     DBI::dbClearResult(result)

//     one_column <- columns[columns$name == column, ]
//     if (nrow(one_column) == 0) {
//         stop("Column \"", column, "\" not found in table \"", table, "\"")
//     }
//     if (nrow(one_column) != 1) {
//         stop("Search for column \"", column, "\" in table \"", table, "\" returned ", nrow(one_column), " results (expoected one)")
//     }
    
//     if (one_column$type == "INTEGER") return("integer")
//     if (one_column$type == "REAL")    return("numeric")
//     if (one_column$type == "TEXT")    return("character")
//     if (one_column$type == "BLOB")    return("raw")

//     stop("Column \"", column, "\" from table \"", table,"\" has type ", one_column$type, " which cannot be represented as an R vector")
// }