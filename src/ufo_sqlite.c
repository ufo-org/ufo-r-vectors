#include "ufo_sqlite.h"
#include "sqlite/sqlite.h"
#include "helpers.h"
#include "safety_first.h"
#include "../include/ufos_writeback.h"

typedef struct {
    char *database;
    char *table;
    char *column;
    size_t row_count;    
    sqlite_type_t sqlite_type;
    ufo_vector_type_t ufo_type;
} column_info_t;

ufo_vector_type_t ufo_vector_type_from(sqlite_type_t sqlite_type) {
    switch (sqlite_type) {
        case UFO_SQLITE_INTEGER: return UFO_INT;           
        case UFO_SQLITE_FLOAT:   return UFO_REAL;
        case UFO_SQLITE_TEXT:    return UFO_STR;
        case UFO_SQLITE_NULL: 
            Rf_error(//"Cannot create a vector from column \"%s\" of table \"%s\" in database \"%s\": " 
                     "Type NULL cannot be expressed as an R UFO vector"
                     //columns->names[column_index], columns->table, columns->database
                     );            
        case UFO_SQLITE_BLOB: 
            Rf_error(//"Cannot create a vector from column \"%s\" of table \"%s\" in database \"%s\": " 
                     "Type BLOB cannot be expressed as an R UFO vector"
                     //columns->names[column_index], columns->table, columns->database);
                     );
    }
    return 0; //Unreachable!
}

column_info_t *column_info_from(const columns_info_t *columns, size_t column_index) {
    make_sure(column_index < columns->column_count, 
              "Column \"%ld\" not found in table \"%s\" in database \"%s\" which has %ld columns",
              column_index, columns->table, columns->database, columns->column_count);

    column_info_t *column_info = (column_info_t *) malloc (sizeof(column_info_t));
    
    column_info->database = (char *) malloc (sizeof(char) * (strlen(columns->database) + 1));
    strcpy(column_info->database, columns->database);
    
    column_info->table = (char *) malloc (sizeof(char) * (strlen(columns->table) + 1));
    strcpy(column_info->table, columns->table);

    column_info->column = (char *) malloc (sizeof(char) * (strlen(columns->names[column_index]) + 1));
    strcpy(column_info->column, columns->names[column_index]);

    column_info->row_count = columns->row_count;
    column_info->sqlite_type = columns->types[column_index];
    column_info->ufo_type = ufo_vector_type_from(column_info->sqlite_type);

    return column_info;
}

void column_info_free(column_info_t *column_info) {
    free(column_info->database);
    free(column_info->table);
    free(column_info->column);
    free(column_info);
}

int32_t sqlite_intsxp_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    column_info_t *column_info = (column_info_t *) user_data;
    return sqlite_get_range(column_info->database, column_info->table, column_info->column, 0, column_info->row_count, &sqlite_get_range_int_callback, target);    
}

int32_t sqlite_realsxp_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    column_info_t *column_info = (column_info_t *) user_data;
    return sqlite_get_range(column_info->database, column_info->table, column_info->column, 0, column_info->row_count, &sqlite_get_range_real_callback, target);    
}

void sqlite_get_range_charsxp_callback(sqlite3_stmt *statement, void *data, size_t row) {
    const char *string = (const char *) sqlite3_column_text(statement, 0); // Assume single column at this point.
    
    // FIXME play nice with GC
    ((SEXP/*STRSXP*/ *) data)[row] = mkChar(string);
}


int32_t sqlite_strsxp_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    column_info_t *column_info = (column_info_t *) user_data;
    return sqlite_get_range(column_info->database, column_info->table, column_info->column, 0, column_info->row_count, &sqlite_get_range_charsxp_callback, target);    
}

void ufo_sqlite_free(void *data) {
    column_info_t *column_info = (column_info_t *) data;
    // disconnect_from_database(psql->database);
    column_info_free(column_info);
}

// This is here because it is extremely R specicfic
int sqlite_update_from_charsxp_vec(sqlite3 *connection, const char *table, const char *column, const size_t *keys, const void *data, size_t length) {
    SEXP/*CHARSXP*/ *values = (SEXP *) data;
    for (size_t i = 0; i < length; i++) {
        const char *string = CHAR(values[i]);
        fprintf(stderr, "[%ld] Updating 3 %s[%ld] to '%s'\n", i, column, keys[i], string);
        int result = sqlite_update_string(connection, table, column, keys[i], string);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

void sqlite_writeback(void *data, UfoWriteListenerEvent event) {

    printf("WRITEBACK EVENT %i\n", event.tag);

    if (event.tag == Writeback) {
        column_info_t *column_info = (column_info_t *) data;

        sqlite_update_function updater = NULL;
        switch (column_info->sqlite_type) {
            case UFO_SQLITE_TEXT:
                updater = sqlite_update_from_charsxp_vec;          
                break;
            case UFO_SQLITE_INTEGER:
                updater = sqlite_update_integers;
                break;
            case UFO_SQLITE_FLOAT:
                updater = sqlite_update_doubles;
                break;
            case UFO_SQLITE_BLOB:        
                printf("Cannot write back to vector of type BLOB");
                return;
            case UFO_SQLITE_NULL: 
                printf("Cannot write back to vector of type NULL");
                return;
        }

        sqlite_update(
            column_info->database, column_info->table, column_info->column, 
            event.writeback.start_idx, event.writeback.end_idx, 
            event.writeback.data, updater
        );
    }
}

SEXP ufo_sqlite_column_constructor(const column_info_t *column_info, bool writeback, bool read_only, int32_t min_load_count) {
    ufo_source_t* source = (ufo_source_t*) malloc(sizeof(ufo_source_t));

    source->vector_type = column_info->ufo_type;
    source->vector_size = column_info->row_count;
    source->element_size = __get_element_size(source->vector_type);

    source->dimensions = NULL;
    source->dimensions_length = 0;

    source->min_load_count =  __select_min_load_count(min_load_count, source->element_size);
    source->read_only = read_only;
    
    source->data = (void *) column_info;

    source->destructor_function = ufo_sqlite_free;
    source->writeback_function = writeback ? sqlite_writeback : NULL;
    fprintf(stderr, "WRITEBACK: %d %p\n", writeback, source->writeback_function);

    switch (column_info->ufo_type) {
        case UFO_INT: 
            source->population_function = sqlite_intsxp_populate;
            break;
        case UFO_REAL: 
            source->population_function = sqlite_realsxp_populate;
            break;
        case UFO_STR: 
            source->population_function = sqlite_strsxp_populate;
            break;
        default: Rf_error("Unknown column type."); // Should be unreachable.
    }  

    // Call UFO constructor and return the result
    ufo_new_t ufo_new = (ufo_new_t) R_GetCCallable("ufos", "ufo_new");
    SEXP ufo = ufo_new(source);    
    return ufo;    
}

column_info_t *ufo_sqlite_column_select(const columns_info_t *columns, const char *column) {
    for (size_t i = 0; i < columns->column_count; i++) {                
        if (0 == strcmp(columns->names[i], column)) {
            return column_info_from(columns, i);
        }        
    }  
    Rf_error("Column \"%s\" not found in table \"%s\" in database \"%s\"", column, columns->table, columns->database);
}

SEXP ufo_sqlite_column(SEXP/*STRSXP*/ db, SEXP/*STRSXP*/ table, SEXP/*STRSXP*/ column, SEXP/*LGLSXP*/ writeback, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    // Read the arguements into practical types (with checks).
    bool read_only_value = __extract_boolean_or_die(read_only);
    bool writeback_value = __extract_boolean_or_die(writeback);
    int min_load_count_value = __extract_int_or_die(min_load_count);
    const char *db_value = __extract_string_or_die(db);             // eg. "host=localhost port=5432 dbname=ufo user=ufo"
    const char *table_value = __extract_string_or_die(table);       // these should be sanitized
    const char *column_value = __extract_string_or_die(column);

    columns_info_t *columns = columns_info_from_sqlite(db_value, table_value);
    if (columns == NULL) {
        Rf_error("Error creating SQLite UFO");
    }
    
    printf("Columns: %ld\n", columns->column_count);
    for (size_t i = 0; i < columns->column_count; i++) {        
        char *sigil = 0 == strcmp(columns->names[i], column_value) ? "+" : "-";
        printf("  %s column [%ld] %s: %d\n", sigil, i, columns->names[i], columns->types[i]);
    }  

    column_info_t *column_info = ufo_sqlite_column_select(columns, column_value);
    SEXP sexp = ufo_sqlite_column_constructor(column_info, writeback_value, read_only_value, min_load_count_value);

    columns_info_free(columns);

    return sexp;
}

SEXP test() {

    int values[3] = { 672, 674, 676 };
    sqlite_update("test.db", "sharks", "length", 0, 3, values, sqlite_update_integers);

    return R_NilValue;
}