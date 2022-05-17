#include "ufo_psql.h"

#include <stdbool.h>
#include <pthread.h> // For locks

#include "psql/psql.h"

#include "debug.h"
#include "helpers.h"
#include "../include/bad_strings.h"
#include "../include/ufos_writeback.h"

#include <R.h>
#define USE_RINTERNALS
#include <Rinternals.h>

// select name from (select row_number() over () nth, name from league) as numbered where numbered.nth = 999
// select column_name, data_type from information_schema.columns where table_name = 'league'

ufo_vector_type_t psql_type_to_vector_type(psql_column_type_t type) {
    // printf("PSQL TYPE %i\n", type);
    switch (type) {
        case PSQL_COL_INT: return UFO_INT;
        case PSQL_COL_REAL: return UFO_REAL;
        case PSQL_COL_CHAR: return UFO_STR;
        case PSQL_COL_BOOL: return UFO_LGL;
        case PSQL_COL_BYTE: return UFO_RAW;
        default:
        Rf_error("Unsupported column type.");
        return 0;
    }    
}

typedef struct {
    PGconn *database;
    const char *table;
    const char *column;
    const char *pk;
} psql_t;

psql_t *psql_new(PGconn *database, const char *table, const char *column) {
    psql_t *psql = (psql_t *) malloc(sizeof(psql_t));
    psql->database = database;
    psql->table = strdup(table);
    psql->column = strdup(column);

    int result;
    // int result = start_transaction(database);
    // if (result != 0) {
    //     Rf_error("Cannot start transaction\n");
    // }

    psql->pk = retrieve_table_pk(database, table, column);
    if (psql->pk == NULL) {
        Rf_error("Cannot establish a PK for table \"%s\"\n", table);
    }

    result = create_table_column_subscript(database, table, psql->pk, column);
    if (result != 0) {
        Rf_error("Cannot create an auxiliary view for table \"%s\"\n", table);
    }

    // result = end_transaction(database);
    // if (result != 0) {
    //     Rf_error("Cannot commit transaction\n");
    // }
    
    return psql;
}

void psql_free(void *data) {
    psql_t *psql = (psql_t *) data;
    disconnect_from_database(psql->database);
    free((void *) psql->pk);
    free((void *) psql->table);
    free((void *) psql->column);
    free(psql);
}

int int_action(uintptr_t index_in_vector, int index_in_target, unsigned char *target, char *element, bool missing) {
    // printf("INT %s\n", element);
    ((int *) target)[index_in_target] = missing ? NA_INTEGER : atoi(element);
    return 0;
}

int real_action(uintptr_t index_in_vector, int index_in_target, unsigned char *target, char *element, bool missing) {
    // printf("REAL %s\n", element);
    char *remainder;
    ((double *) target)[index_in_target] = missing ? NA_REAL : strtod(element, &remainder);
    return 0;
}

int logical_action(uintptr_t index_in_vector, int index_in_target, unsigned char *target, char *element, bool missing) {
    // printf("LGL %s\n", element);
    Rboolean value;
    if (missing) {
        value = NA_LOGICAL;
    } else if (element[0] == 't' || element[0] == 'T') {
        value = TRUE;
    } else if (element[0] == 'f' || element[0] == 'F') { /// Probably unnnecessary
        value = FALSE;
    } else {
        UFO_REPORT("Cannot parse boolean value: %s", element);
        return 2;
    }
    ((Rboolean *) target)[index_in_target] = value;
    return 0;
}

// This doesn't work.
int raw_action(uintptr_t index_in_vector, int index_in_target, unsigned char *target, char *element, bool missing) {
    ((Rbyte *) target)[index_in_target] = missing ? 0 : (Rbyte) strtol(element, NULL, 16);
    return 0; 
}

// It depends on interned strings, so this is not good.
int string_action(uintptr_t index_in_vector, int index_in_target, unsigned char *target, char *element, bool missing) {
    UFO_REPORT("string_action unimplemented");
    return 1;
    // printf("STR %s\n", element);
    // SEXP/*CHARSXP*/ bad_string;
    // //UFO_REPORT("string_action unimplemented");
    // R_len_t size = strlen(element);
    // PROTECT(bad_string = allocVector(73, size));
    // memcpy(DATAPTR(bad_string), element, size);
    // // SET_ASCII(bad_string);                                // FIXME
    // UNPROTECT(1);

    // ((SEXP *) target)[index_in_target] = missing ? R_NaString : bad_string;
    // return 0;  
}

int32_t intsxp_psql_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    psql_t *psql = (psql_t *) user_data;
    return retrieve_from_table(psql->database, psql->table, psql->column, start, end, int_action, target);
}
int32_t lglsxp_psql_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    psql_t *psql = (psql_t *) user_data;
    return retrieve_from_table(psql->database, psql->table, psql->column, start, end, logical_action, target);
}
int32_t rawsxp_psql_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    psql_t *psql = (psql_t *) user_data;
    return retrieve_from_table(psql->database, psql->table, psql->column, start, end, raw_action, target);
}
int32_t realsxp_psql_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    psql_t *psql = (psql_t *) user_data;
    return retrieve_from_table(psql->database, psql->table, psql->column, start, end, real_action, target);
}
int32_t strsxp_psql_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    psql_t *psql = (psql_t *) user_data;
    return retrieve_from_table(psql->database, psql->table, psql->column, start, end, string_action, target);
}

// FIXME guard buffer from overflow
int int_writeback(uintptr_t index_in_vector, int index_in_target, const unsigned char *data, char *buffer, bool *missing) {
    printf("index_in_vector %li index in target %i\n", index_in_vector, index_in_target);
    int element = ((int *) data)[index_in_target];
    printf("element %i\n", element);
    (*missing) = (element == NA_INTEGER);
    sprintf(buffer, "%i", element);
    return 0;
}

int real_writeback(uintptr_t index_in_vector, int index_in_target, const unsigned char *data, char *buffer, bool *missing) {
    double element = ((double *) data)[index_in_target];
    (*missing) = ISNAN(element);
    sprintf(buffer, "%f", element);
    return 0;
}

int logical_writeback(uintptr_t index_in_vector, int index_in_target, const unsigned char *data, char *buffer, bool *missing) {
    Rboolean element = ((Rboolean *) data)[index_in_target];
    (*missing) = (element == NA_LOGICAL);
    sprintf(buffer, "%s", element == TRUE ? "TRUE" : "FALSE");
    return 0;
}

int raw_writeback(uintptr_t index_in_vector, int index_in_target, const unsigned char *data, char *buffer, bool *missing) {
    Rbyte element = ((Rbyte *) data)[index_in_target];
    (*missing) = false;
    sprintf(buffer, "%x", element);
    return 0;
}

int string_writeback(uintptr_t index_in_vector, int index_in_target, const unsigned char *data, char *buffer, bool *missing) {
    //UFO_REPORT("string_writeback unimplemented");
    return 1;  
}

void intsxp_psql_writeback(void* user_data, UfoWriteListenerEvent event) {
    psql_t *psql = (psql_t *) user_data;
    if (event.tag != Writeback) { return; }

    uintptr_t start = event.writeback.start_idx;
    uintptr_t end = event.writeback.end_idx;
    const unsigned char *data = (const unsigned char *) event.writeback.data;

    update_table(psql->database, psql->table, psql->column, psql->pk, start, end, int_writeback, data);
    return;
}

void lglsxp_psql_writeback(void* user_data, UfoWriteListenerEvent event) {
    psql_t *psql = (psql_t *) user_data;
    if (event.tag != Writeback) { return; }

    uintptr_t start = event.writeback.start_idx;
    uintptr_t end = event.writeback.end_idx;
    const unsigned char *data = (const unsigned char *) event.writeback.data;

    update_table(psql->database, psql->table, psql->column, psql->pk, start, end, logical_writeback, data);
    return;
}

void rawsxp_psql_writeback(void* user_data, UfoWriteListenerEvent event) {
    psql_t *psql = (psql_t *) user_data;
    if (event.tag != Writeback) { return; }

    uintptr_t start = event.writeback.start_idx;
    uintptr_t end = event.writeback.end_idx;
    const unsigned char *data = (const unsigned char *) event.writeback.data;

    update_table(psql->database, psql->table, psql->column, psql->pk, start, end, raw_writeback, data);
    return;
}

void realsxp_psql_writeback(void* user_data, UfoWriteListenerEvent event) {
    psql_t *psql = (psql_t *) user_data;
    if (event.tag != Writeback) { return; }

    uintptr_t start = event.writeback.start_idx;
    uintptr_t end = event.writeback.end_idx;
    const unsigned char *data = (const unsigned char *) event.writeback.data;

    update_table(psql->database, psql->table, psql->column, psql->pk, start, end, real_writeback, data);
    return;
}

void strsxp_psql_writeback(void* user_data, UfoWriteListenerEvent event) {
    psql_t *psql = (psql_t *) user_data;
    if (event.tag != Writeback) { return; }

    uintptr_t start = event.writeback.start_idx;
    uintptr_t end = event.writeback.end_idx;
    const unsigned char *data = (const unsigned char *) event.writeback.data;

    update_table(psql->database, psql->table, psql->column, psql->pk, start, end, string_writeback, data);
    return;
}

SEXP ufo_psql(SEXP/*STRSXP*/ db, SEXP/*STRSXP*/ table, SEXP/*STRSXP*/ column, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    // Read the arguements into practical types (with checks).
    bool read_only_value = __extract_boolean_or_die(read_only);
    int min_load_count_value = __extract_int_or_die(min_load_count);
    const char *db_value = __extract_string_or_die(db);             // eg. "host=localhost port=5432 dbname=ufo user=ufo"
    const char *table_value = __extract_string_or_die(table);       // these should be sanitized
    const char *column_value = __extract_string_or_die(column);


    // Get the data
    PGconn *database = connect_to_database(db_value);
    if (NULL == database) {
        Rf_error("Cannot connect to database with connection string \"%s\".\n", db_value);
    }

    // Get vector size from database
    size_t vector_size = 0;
    int size_query_result = retrieve_size_of_table(database, table_value, &vector_size);
    if (size_query_result != 0) {
        Rf_error("Cannot calculate vector size from table \"%s\".\n", table_value);
    }

    // Get vector type from the database
    psql_column_type_t psql_type = PSQL_COL_UNSUPPORTED;
    int type_query_result = retrieve_type_of_column(database, table_value, column_value, &psql_type);
    if (type_query_result != 0) {
        Rf_error("Cannot calculate vector type from column \"%s\" of table \"%s\".\n", column_value, table_value);
    }
    ufo_vector_type_t vector_type = psql_type_to_vector_type(psql_type);

    // Create UFO
    ufo_source_t* source = (ufo_source_t*) malloc(sizeof(ufo_source_t));

    // Basics
    source->vector_size = vector_size;
    source->vector_type = vector_type;
    source->element_size = __get_element_size(vector_type);
     
    // Behavior specification
    source->data = psql_new(database, table_value, column_value);
    source->destructor_function = psql_free;
    
    switch (vector_type) {
    case UFO_REAL: 
        source->population_function = realsxp_psql_populate; 
        source->writeback_function = realsxp_psql_writeback; 
        break;        
    case UFO_LGL:
        source->population_function = lglsxp_psql_populate;
        source->writeback_function = lglsxp_psql_writeback;
        break;
    case UFO_INT:  
        source->population_function = intsxp_psql_populate;  
        source->writeback_function = intsxp_psql_writeback;  
        break;
    case UFO_RAW:  
        source->population_function = rawsxp_psql_populate;  
        source->writeback_function = rawsxp_psql_writeback;  
        break;
    case UFO_STR:  
        source->population_function = strsxp_psql_populate;
        source->writeback_function = strsxp_psql_writeback;
        break;
        // case UFO_CHAR: source->population_function = charsxp_psql_populate; break;
        // case UFO_CPLX: source->population_function = cplxsxp_psql_populate; break;
        // case UFO_VEC:  source->population_function = vecsxp_psql_populate;  break;    
    default: Rf_error("Unsupported UFO type: %s", type2char(vector_type));
    }

    // REprintf("Writeback listener 1 %p\n", source->writeback_function);

    // Chunk-related parameters
    source->read_only = read_only_value;
    source->min_load_count = __select_min_load_count(min_load_count_value, source->element_size);

    // Unused.
    source->dimensions = NULL;
    source->dimensions_length = 0;

    // Call UFO constructor and return the result
    ufo_new_t ufo_new = (ufo_new_t) R_GetCCallable("ufos", "ufo_new");
    SEXP ufo = ufo_new(source);    
    return ufo;
}