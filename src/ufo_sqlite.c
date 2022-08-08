#include "ufo_sqlite.h"
#include "sqlite/sqlite.h"
#include "helpers.h"
#include "safety_first.h"

SEXP ufo_sqlite_test() {

    columns_info_t *columns = columns_info_from_sqlite("test.db", "sharks");

    printf("Columns: %ld\n", columns->column_count);
    for (size_t i = 0; i < columns->column_count; i++) {
        printf("  - column [%ld] %s: %d\n", i, columns->names[i], columns->types[i]);
    }    

    int data[10];
    for (int i = 0; i < 10; i++) data[i] = 0;
    sqlite_get_range("test.db", "sharks", "length", 0, 3, &sqlite_get_range_int_callback, data);

    char *names[10];
    for (int i = 0; i < 10; i++) names[i] = "";
    sqlite_get_range("test.db", "sharks", "name", 0, 3, &sqlite_get_range_text_callback, names);

    printf("sharks.name:\n");
    for (int i = 0; i < 10; i++) printf("  - %d %s\n", data[i], names[i]);

    column_info_free(columns);

    return R_NilValue;
}

ufo_vector_type_t ufo_sqlite_column_type(const columns_info_t *columns, size_t column_index) {
    make_sure(column_index < columns->column_count, 
              "Column \"%ld\" not found in table \"%s\" in database \"%s\" which has %ld columns",
              column_index, columns->table, columns->database, columns->column_count);

    switch (columns->types[column_index]) {
        case UFO_SQLITE_INTEGER: return UFO_INT;           
        case UFO_SQLITE_FLOAT:   return UFO_REAL;
        case UFO_SQLITE_TEXT:    return UFO_STR;
        case UFO_SQLITE_NULL: 
            Rf_error("Cannot create a vector from column \"%s\" of table \"%s\" in database \"%s\": " 
                     "type NULL cannot be expressed as an R UFO vector",
                     columns->names[column_index], columns->table, columns->database);            
        case UFO_SQLITE_BLOB: 
            Rf_error("Cannot create a vector from column \"%s\" of table \"%s\" in database \"%s\": " 
                     "type BLOB cannot be expressed as an R UFO vector",
                     columns->names[column_index], columns->table, columns->database);
    }

    return 0; // Unreachable!
}

void ufo_sqlite_column_constructor(const columns_info_t *columns, size_t column_index) {
    ufo_vector_type_t column_type = ufo_sqlite_column_type(columns, column_index);
    printf("column type: %ld -> %d\n", column_index, column_type);

    sqlite_get_range_callback callback = NULL;
    void *data = NULL;
    switch (column_type) {
        case UFO_INT: 
            callback = sqlite_get_range_int_callback; 
            data = malloc(sizeof(int) * columns->row_count);
            break;
        case UFO_REAL: 
            callback = sqlite_get_range_real_callback; 
            data = malloc(sizeof(double) * columns->row_count);
            break;
        case UFO_STR: 
            callback = sqlite_get_range_text_callback; 
            data = malloc(sizeof(char *) * columns->row_count);
            break;
        default: Rf_error("Unknown column type."); // Should be unreachable.
    }

    sqlite_get_range(columns->database, columns->table, columns->names[column_index], 0, columns->row_count, callback, data);    

printf("%ld\n", columns->row_count);
    switch (column_type) {
        case UFO_INT: 
            for (size_t i = 0; i < columns->row_count; i++) {
                printf("[%ld] %d\n", i, ((int *) data)[i]);
            }
            break;
        case UFO_REAL: 
            for (size_t i = 0; i < columns->row_count; i++) {
                printf("[%ld] %f\n", i, ((double *) data)[i]);
            }
        case UFO_STR: 
            for (size_t i = 0; i < columns->row_count; i++) {
                printf("[%ld] %s\n", i, ((char **) data)[i]);
            }
        default: Rf_error("Unknown column type."); // Should be unreachable.
    }
    
}

size_t ufo_sqlite_column_select(const columns_info_t *columns, const char *column) {
    for (size_t i = 0; i < columns->column_count; i++) {                
        if (0 == strcmp(columns->names[i], column)) {
            return i;
        }        
    }  
    Rf_error("Column \"%s\" not found in table \"%s\" in database \"%s\"", column, columns->table, columns->database);
}

SEXP ufo_sqlite_column(SEXP/*STRSXP*/ db, SEXP/*STRSXP*/ table, SEXP/*STRSXP*/ column, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    // Read the arguements into practical types (with checks).
    // bool read_only_value = __extract_boolean_or_die(read_only);
    // int min_load_count_value = __extract_int_or_die(min_load_count);
    const char *db_value = __extract_string_or_die(db);             // eg. "host=localhost port=5432 dbname=ufo user=ufo"
    const char *table_value = __extract_string_or_die(table);       // these should be sanitized
    const char *column_value = __extract_string_or_die(column);

    columns_info_t *columns = columns_info_from_sqlite(db_value, table_value);
    
    printf("Columns: %ld\n", columns->column_count);
    
    size_t column_index = ufo_sqlite_column_select(columns, column_value);

    for (size_t i = 0; i < columns->column_count; i++) {        
        char *sigil = 0 == strcmp(columns->names[i], column_value) ? "+" : "-";
        if (0 == strcmp(columns->names[i], column_value)) {
            
        }
        printf("  %s column [%ld] %s: %d\n", sigil, i, columns->names[i], columns->types[i]);
    }  

    ufo_sqlite_column_constructor(columns, column_index);

    column_info_free(columns);

    return R_NilValue;
}