#include "ufo_write_protect.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "safety_first.h"

#define USE_RINTERNALS
#include <R.h>
#include <Rinternals.h>

#include "../include/ufos.h"

#include "helpers.h"
#include "debug.h"

// TODO we should avoid calling INTEGER_ELT etc inside these functions. better grab DATAPTR inconstructor and stick it in an EXTRPTR for good measure.
int32_t intsxp_write_protect_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    SEXP/*INTSXP*/ vector = (SEXP) user_data;
    if (TYPEOF(vector) != INTSXP) {
        fprintf(stderr, "Write protected vector was expecting an integer vector, but a found: %s.", type2char(TYPEOF(vector) ));
        return 1;
    }
    for (size_t i = 0; i < end - start; i++) {
        ((int *) target)[i] = INTEGER_ELT(vector, i); // this could probably be done with regions       
    }
    return 0;
}

int32_t realsxp_write_protect_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    SEXP/*REALSXP*/ vector = (SEXP) user_data;
    if (TYPEOF(vector) != REALSXP) {
        fprintf(stderr, "Write protected vector was expecting a numerical vector, but a found: %s.", type2char(TYPEOF(vector) ));
        return 1;
    }
    for (size_t i = 0; i < end - start; i++) {
        ((double *) target)[i] = REAL_ELT(vector, i); // this could probably be done with regions        
    }
    return 0;
}

int32_t lglsxp_write_protect_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    SEXP/*LGLSXP*/ vector = (SEXP) user_data;
    if (TYPEOF(vector) != LGLSXP) {
        fprintf(stderr, "Write protected vector was expecting a logical vector, but a found: %s.", type2char(TYPEOF(vector) ));
        return 1;
    }
    for (size_t i = 0; i < end - start; i++) {
        ((Rboolean *) target)[i] = LOGICAL_ELT(vector, i); // this could probably be done with regions
    }
    return 0;
}

int32_t strsxp_write_protect_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    SEXP/*STRSXP*/ vector = (SEXP) user_data;
    if (TYPEOF(vector) != STRSXP) {
        fprintf(stderr, "Write protected vector was expecting a character vector, but a found: %s.", type2char(TYPEOF(vector) ));
        return 1;
    }
    for (size_t i = 0; i < end - start; i++) {
        ((SEXP *) target)[i] = VECTOR_ELT(vector, i);
    }
    return 0;
}

int32_t vecsxp_write_protect_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    SEXP/*VECSXP*/ vector = (SEXP) user_data;
    if (TYPEOF(vector) != VECSXP) {
        fprintf(stderr, "Write protected vector was expecting a generic vector, but a found: %s.", type2char(TYPEOF(vector) ));
        return 1;
    }
    for (size_t i = 0; i < end - start; i++) {
        ((SEXP *) target)[i] = VECTOR_ELT(vector, i);
    }
    return 0;
}

int32_t rawsxp_write_protect_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    SEXP/*RAWSXP*/ vector = (SEXP) user_data;
    if (TYPEOF(vector) != RAWSXP) {
        fprintf(stderr, "Write protected vector was expecting a raw vector, but a found: %s.", type2char(TYPEOF(vector) ));
        return 1;
    }
    for (size_t i = 0; i < end - start; i++) {
        ((Rbyte *) target)[i] = RAW_ELT(vector, i); // this could probably be done with regions
    }
    return 0;
}

int32_t cplxsxp_write_protect_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    SEXP/*CPLXSXP*/ vector = (SEXP) user_data;
    if (TYPEOF(vector) != CPLXSXP) {
        fprintf(stderr, "Write protected vector was expecting a complex vector, but a found: %s.", type2char(TYPEOF(vector) ));
        return 1;
    }
    for (size_t i = 0; i < end - start; i++) {
        ((Rcomplex *) target)[i] = COMPLEX_ELT(vector, i); // this could probably be done with regions
    }
    return 0;
}

int32_t charsxp_write_protect_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    SEXP/*CHARSXP*/ vector = (SEXP) user_data;
    if (TYPEOF(vector) != CHARSXP) {
        fprintf(stderr, "Write protected vector was expecting a CHARSXP, but a found: %s.", type2char(TYPEOF(vector) ));
        return 1;
    }
    for (size_t i = 0; i < end - start; i++) {
        ((char *) target)[i] = ((char *) DATAPTR(vector))[i];
    }
    return 0;
}

void write_protect_free(void* data) {}

SEXP ufo_write_protect(SEXP vector, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    PROTECT(vector);

    // Read the arguements into practical types (with checks).
    bool read_only_value = __extract_boolean_or_die(read_only);
    int min_load_count_value = __extract_int_or_die(min_load_count);

    SEXPTYPE type = TYPEOF(vector);

    // Create a source struct for UFOs.
    ufo_source_t* source = (ufo_source_t*) malloc(sizeof(ufo_source_t));

    // Element size and count metadata
    switch (type) {
        case UFO_CHAR : source->vector_type = CHARSXP; break;
        case UFO_LGL  : source->vector_type = LGLSXP;  break;
        case UFO_INT  : source->vector_type = INTSXP;  break;
        case UFO_REAL : source->vector_type = REALSXP; break;
        case UFO_CPLX : source->vector_type = CPLXSXP; break;
        case UFO_RAW  : source->vector_type = RAWSXP;  break;
        case UFO_STR  : source->vector_type = STRSXP;  break;
        case UFO_VEC  : source->vector_type = VECSXP;  break;    
    }
    source->element_size = __get_element_size(type);
    source->vector_size = XLENGTH(vector);

    // Behavior specification
    source->data = (void*) vector; // FIXME protect from GC
    source->destructor_function = write_protect_free; //&destroy_data;

    switch (type) {
        case UFO_CHAR : source->population_function = charsxp_write_protect_populate; break;
        case UFO_LGL  : source->population_function = lglsxp_write_protect_populate;  break;
        case UFO_INT  : source->population_function = intsxp_write_protect_populate;  break;
        case UFO_REAL : source->population_function = realsxp_write_protect_populate; break;
        case UFO_CPLX : source->population_function = cplxsxp_write_protect_populate; break;
        case UFO_RAW  : source->population_function = rawsxp_write_protect_populate;  break;
        case UFO_STR  : source->population_function = strsxp_write_protect_populate;  break;
        case UFO_VEC  : source->population_function = vecsxp_write_protect_populate;  break;    
    }
    
    source->writeback_function = NULL;

    // Chunk-related parameters
    source->read_only = read_only_value;
    source->min_load_count = __select_min_load_count(min_load_count_value, source->element_size);

    // Unused.
    source->dimensions = NULL;
    source->dimensions_length = 0;

    ufo_new_t ufo_new = (ufo_new_t) R_GetCCallable("ufos", "ufo_new");
    SEXP ufo = ufo_new(source);
    PROTECT(ufo);

    // Wrap vector in extenral pointer to rpevent it from being freed, as long as the UFO is live
    R_MakeExternalPtr(vector, vector, ufo);
    UNPROTECT(2);

    return ufo;
}

