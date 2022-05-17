#include "ufo_bind.h"

#include "Rinternals.h"

#include "stdbool.h"

#include "helpers.h"
#include "debug.h"
#include "ufo_operators.h"

#include "../include/ufos.h"

typedef enum {
    BIND_NILSXP  = 0,
    BIND_LGLSXP  = 1,
    BIND_RAWSXP  = 2,
    BIND_CHARSXP = 4,    
    BIND_INTSXP  = 8,
    BIND_REALSXP = 16,
    BIND_CPLXSXP = 32,
    BIND_STRSXP  = 64,
    BIND_VECSXP  = 128,
} bind_type_t;

// Unsupported:
// UFO_CHAR = CHARSXP,
// UFO_STR  = STRSXP,
// UFO_VEC  = VECSXP,

typedef union {
    bind_type_t type;
    struct {
        unsigned int LGL  : 1;
        unsigned int RAW  : 1;
        unsigned int CHAR : 1;        
        unsigned int INT  : 1;
        unsigned int REAL : 1;
        unsigned int CPLX : 1;
        unsigned int STR  : 1;
        unsigned int VEC  : 1;
    } bitfields;
} bind_type_detector_t;

ufo_vector_type_t bind_type_to_ufo_type(const bind_type_detector_t *detector) {
    // Straight types
    switch (detector->type) {
        case BIND_NILSXP:  Rf_error("No vectors were provided.");
        case BIND_RAWSXP:  return UFO_RAW;
        case BIND_CHARSXP: return UFO_CHAR;
        case BIND_LGLSXP:  return UFO_LGL;
        case BIND_INTSXP:  return UFO_INT;
        case BIND_REALSXP: return UFO_REAL;
        case BIND_CPLXSXP: return UFO_CPLX;
        case BIND_STRSXP:  return UFO_STR;
        case BIND_VECSXP:  return UFO_VEC;
    }

    // Some things we cannot mix
    if (detector->bitfields.VEC) {
        Rf_error("UFO bind cannot mix generic vectors with other vector types");
    }
    if (detector->bitfields.STR) {
        Rf_error("UFO bind cannot mix string vectors with other vector types");
    }
    if (detector->bitfields.CHAR) {
        Rf_error("UFO bind cannot mix character vectors with other types");
    }

    // A lot of the time we can coerse a common supertype
    if (detector->bitfields.CPLX) return UFO_CPLX; // CPLX REAL INT RAW LGL -> CPLX
    if (detector->bitfields.REAL) return UFO_REAL; //      REAL INT RAW LGL -> REAL
    if (detector->bitfields.INT)  return UFO_INT;  //           INT RAW LGL -> INT
    if (detector->bitfields.RAW)  return UFO_RAW;  //               RAW LGL -> RAW
    if (detector->bitfields.LGL)  return UFO_LGL;  //                   LGL -> LGL    

    Rf_error("UFO bind cannot figure out type coersion for supplied vectors");    
    return -1;
}

void bind_type_record (bind_type_detector_t *detector, SEXPTYPE type) {
    switch(type) {
        case RAWSXP:  detector->bitfields.RAW  = 1; break;
        case LGLSXP:  detector->bitfields.LGL  = 1; break;
        case INTSXP:  detector->bitfields.INT  = 1; break;
        case REALSXP: detector->bitfields.REAL = 1; break;
        case CPLXSXP: detector->bitfields.CPLX = 1; break;
        case CHARSXP: detector->bitfields.CHAR = 1; break;
        case STRSXP:  detector->bitfields.STR  = 1; break;
        case VECSXP:  detector->bitfields.VEC  = 1; break;
        default: Rf_error("Cannot bind object of type %s.", type2char(type));
    }
}

typedef int (*bind_action)(SEXP vector, R_xlen_t vi, unsigned char* target, uintptr_t ti);

int32_t bind_iterate(void *user_data, uintptr_t start, uintptr_t end, unsigned char* target, bind_action generic_action) {
    SEXP/*VECSXP*/ vectors = (SEXP) user_data;

    // The basics
    R_xlen_t vector_count = XLENGTH(vectors);

    // Skip the vectors before the first index we need
    R_xlen_t index_offset = 0;
    R_xlen_t first_vector_within_range = 0;
    R_xlen_t index_within_vector = 0;
    for (R_xlen_t i = 0; i < vector_count; i++) {
        SEXP vector = VECTOR_ELT(vectors, i);
        R_xlen_t max_index_within_vector = XLENGTH(vector);
        if (index_offset + max_index_within_vector <= start) {
            // Index within this vector
            index_within_vector = start - index_offset - 1;
            first_vector_within_range = i;
            break;
        } else {
            // Keep going
            index_offset += max_index_within_vector;
        }
    }
    UFO_LOG("Index %li translates to index %li in vector %li", 
            start, index_within_vector, first_vector_within_range);

    // Start coercing and copying values until last index.
    R_xlen_t to_fill = end - start;
    R_xlen_t index_in_target = 0;
    for (R_xlen_t vi = first_vector_within_range; vi < vector_count; vi++) {
        SEXP vector = VECTOR_ELT(vectors, vi);
        R_xlen_t vector_size = XLENGTH(vector);
        for(R_xlen_t i = index_within_vector; i < vector_size; i++, index_in_target++) {
            
            // Do the thing on one elemnt of the source vector and one element of the target.
            int result = generic_action(vector, i, target, index_in_target);

            // Stop on error.
            if (result != 0) {
                return result;
            }

            // If the target is filled, exit loop
            if (--to_fill == 0) {
                goto end_of_loop;
            }
        }

        // All vectors except possibly the first one start indexing from zero
        index_within_vector = 0;
    }

    // Something went wrong: we did not have enough data to fill the target.
    UFO_REPORT("Cannot fill area of memory from vectors");
    return 1;

    // Order fulfilled
    end_of_loop:
    return 0;
}


int coerce_to_int_and_copy(SEXP source, R_xlen_t si, unsigned char* target, uintptr_t ti) {   
    // Extra check to prevent Rf_error from firing within thje funcion
    switch (TYPEOF(source))	{
	    case INTSXP:  ((int *) target)[ti] = INTEGER_ELT(source, si);                     return 0;
	    // case REALSXP: ((int *) target)[ti] = real_as_integer(REAL_ELT(source, si));       return 0;
	    // case CPLXSXP: ((int *) target)[ti] = complex_as_integer(COMPLEX_ELT(source, si)); return 0;		
	    // case STRSXP:  ((int *) target)[ti] = string_as_integer(STRING_ELT(source, si));   return 0;
	    case LGLSXP:  ((int *) target)[ti] = logical_as_integer(LOGICAL_ELT(source, si)); return 0;
        case RAWSXP:  ((int *) target)[ti] = raw_as_integer(RAW_ELT(source, si));         return 0;
	    default:
        UFO_REPORT("Cannot coerce values from vector of type %s to integer", type2char(TYPEOF(source)));
        return 1;
    }    
}

int coerce_to_real_and_copy(SEXP source, R_xlen_t si, unsigned char* target, uintptr_t ti) {
    // Extra check to prevent Rf_error from firing within thje funcion
    switch (TYPEOF(source))	{
        case INTSXP:  ((double *) target)[ti] = integer_as_real(INTEGER_ELT(source, si)); return 0;
	    case REALSXP: ((double *) target)[ti] = REAL_ELT(source, si);                     return 0; 
	    // case CPLXSXP: ((double *) target)[ti] = complex_as_real(COMPLEX_ELT(source, si)); return 0; 		
	    // case STRSXP:  ((double *) target)[ti] = string_as_real(STRING_ELT(source, si));   return 0;
	    case LGLSXP:  ((double *) target)[ti] = logical_as_real(LOGICAL_ELT(source, si)); return 0;
	    case RAWSXP:  ((double *) target)[ti] = raw_as_real(RAW_ELT(source, si));         return 0;
	    default:
        UFO_REPORT("Cannot coerce values from vector of type %s to real", type2char(TYPEOF(source)));
        return 1;
    }    
}

int coerce_to_complex_and_copy(SEXP source, R_xlen_t si, unsigned char* target, uintptr_t ti) {
    // Extra check to prevent Rf_error from firing within thje funcion
    switch (TYPEOF(source))	{
        case INTSXP:  ((Rcomplex *) target)[ti] = integer_as_complex(INTEGER_ELT(source, si)); return 0;
	    case REALSXP: ((Rcomplex *) target)[ti] = real_as_complex(REAL_ELT(source, si));       return 0; 
	    case CPLXSXP: ((Rcomplex *) target)[ti] = COMPLEX_ELT(source, si);                     return 0; 		
	    case STRSXP:  ((Rcomplex *) target)[ti] = string_as_complex(STRING_ELT(source, si));   return 0;
	    case LGLSXP:  ((Rcomplex *) target)[ti] = logical_as_complex(LOGICAL_ELT(source, si)); return 0;
	    case RAWSXP:  ((Rcomplex *) target)[ti] = raw_as_complex(RAW_ELT(source, si));         return 0;
	    default:
        UFO_REPORT("Cannot coerce values from vector of type %s to complex", type2char(TYPEOF(source)));
        return 1;
    }    
}

int coerce_to_raw_and_copy(SEXP source, R_xlen_t si, unsigned char* target, uintptr_t ti) {
    // Extra check to prevent Rf_error from firing within thje funcion
    switch (TYPEOF(source))	{        
	    case LGLSXP:  ((Rbyte *) target)[ti] = (LOGICAL_ELT(source, si)); return 0;
	    case RAWSXP:  ((Rbyte *) target)[ti] = RAW_ELT(source, si);                     return 0;
	    default:
        UFO_REPORT("Cannot coerce values from vector of type %s to raw", type2char(TYPEOF(source)));
        return 1;
    }    
}


int coerce_to_logical_and_copy(SEXP source, R_xlen_t si, unsigned char* target, uintptr_t ti) {
    // Extra check to prevent Rf_error from firing within thje funcion
    switch (TYPEOF(source))	{        
	    case LGLSXP:  ((Rboolean *) target)[ti] = LOGICAL_ELT(source, si); return 0;
        // Good enough, is!	    
	    default:
        UFO_REPORT("Cannot coerce values from vector of type %s to logical", type2char(TYPEOF(source)));
        return 1;
    }    
}

int coerce_to_string_and_copy(SEXP source, R_xlen_t si, unsigned char* target, uintptr_t ti) {
    // Extra check to prevent Rf_error from firing within thje funcion
    switch (TYPEOF(source))	{        
	    case STRSXP:  ((SEXP *) target)[ti] = STRING_ELT(source, si); return 0;
        // Good enough, is!	    
	    default:
        UFO_REPORT("Cannot coerce values from vector of type %s to character vector", type2char(TYPEOF(source)));
        return 1;
    }    
}

int coerce_to_character_and_copy(SEXP source, R_xlen_t si, unsigned char* target, uintptr_t ti) {
    // Extra check to prevent Rf_error from firing within thje funcion
    switch (TYPEOF(source))	{        
	    case CHARSXP:  ((char *) target)[ti] = ((char *) DATAPTR(source))[si]; return 0;
        // Good enough, is!	    
	    default:
        UFO_REPORT("Cannot coerce values from vector of type %s to CHARSXP", type2char(TYPEOF(source)));
        return 1;
    }    
}

int coerce_to_generic_and_copy(SEXP source, R_xlen_t si, unsigned char* target, uintptr_t ti) {
    // Extra check to prevent Rf_error from firing within thje funcion
    switch (TYPEOF(source))	{        
	    case VECSXP:  ((SEXP *) target)[ti] = VECTOR_ELT(source, si); return 0;
        // Good enough, is!	    
	    default:
        UFO_REPORT("Cannot coerce values from vector of type %s to generic vector", type2char(TYPEOF(source)));
        return 1;
    }    
}

int32_t intsxp_bind_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    return bind_iterate(user_data, start, end, target, coerce_to_int_and_copy);
}
int32_t lglsxp_bind_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    return bind_iterate(user_data, start, end, target, coerce_to_logical_and_copy);
}
int32_t rawsxp_bind_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    return bind_iterate(user_data, start, end, target, coerce_to_raw_and_copy);
}
int32_t realsxp_bind_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    return bind_iterate(user_data, start, end, target, coerce_to_real_and_copy);
}
int32_t cplxsxp_bind_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    return bind_iterate(user_data, start, end, target, coerce_to_complex_and_copy);
}
int32_t strsxp_bind_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    return bind_iterate(user_data, start, end, target, coerce_to_string_and_copy);
}
int32_t vecsxp_bind_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    return bind_iterate(user_data, start, end, target, coerce_to_generic_and_copy);
}
int32_t charsxp_bind_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    return bind_iterate(user_data, start, end, target, coerce_to_character_and_copy);
}

void bind_free(void* data) {}

SEXP ufo_bind (SEXP/*VECSXP*/ vectors, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    // Read the arguements into practical types (with checks).
    bool read_only_value = __extract_boolean_or_die(read_only);
    int min_load_count_value = __extract_int_or_die(min_load_count);

    // Check remaining argument types: we only take VECSXP, we don't do pairlists
    if (TYPEOF(vectors) != VECSXP) {
        Rf_error("UFO bind expected VECSXP, but found %s", type2char(TYPEOF(vectors)));
        return R_NilValue;
    }
    
    // Just in case
    PROTECT(vectors);

    // No vectors, no binding
    if (XLENGTH(vectors) == 0) {
        return R_NilValue;
    }

    // Figure out common supertype
    bind_type_detector_t common_type_detector = {0};
    for (R_xlen_t i = 0; i < XLENGTH(vectors); i++) {
        SEXP vector = VECTOR_ELT(vectors, i);
        bind_type_record(&common_type_detector, TYPEOF(vector));
    }
    ufo_vector_type_t common_type = bind_type_to_ufo_type(&common_type_detector);
    UFO_LOG("Common type: %i\n", common_type);

    // Figure out vector size
    R_xlen_t size = 0;
    for (R_xlen_t i = 0; i < XLENGTH(vectors); i++) {
        SEXP vector = VECTOR_ELT(vectors, i);
        size += XLENGTH(vector);
    }
    UFO_LOG("Binding vector length: %i\n", common_type);

    // TODO it would be a lot safer to collect lengths and DATAPTRS at this point, to "discharge" any altreps hiding in the midst.

    // Create UFO
    ufo_source_t* source = (ufo_source_t*) malloc(sizeof(ufo_source_t));
    source->vector_type = common_type;
    source->element_size = __get_element_size(common_type);
    source->vector_size = size;

    // Behavior specification
    source->data = (void*) vectors; // FIXME protect from GC
    source->destructor_function = bind_free; //&destroy_data;

    switch (common_type) {
        case UFO_CHAR : source->population_function = charsxp_bind_populate; break;
        case UFO_REAL : source->population_function = realsxp_bind_populate; break;
        case UFO_CPLX : source->population_function = cplxsxp_bind_populate; break;
        case UFO_LGL  : source->population_function = lglsxp_bind_populate;  break;
        case UFO_INT  : source->population_function = intsxp_bind_populate;  break;
        case UFO_RAW  : source->population_function = rawsxp_bind_populate;  break;
        case UFO_STR  : source->population_function = strsxp_bind_populate;  break;
        case UFO_VEC  : source->population_function = vecsxp_bind_populate;  break;    
        default: Rf_error("Unsupported UFO type: %s", type2char(common_type));
    }

    source->writeback_function = NULL;

    // Chunk-related parameters
    source->read_only = read_only_value;
    source->min_load_count = __select_min_load_count(min_load_count_value, source->element_size);

    // Unused.
    source->dimensions = NULL;
    source->dimensions_length = 0;

    // Call UFO constructor
    ufo_new_t ufo_new = (ufo_new_t) R_GetCCallable("ufos", "ufo_new");
    SEXP ufo = ufo_new(source);    

    // Wrap vectors in extenral pointer to prevent them from being freed, as long as the UFO is live
    PROTECT(ufo);
    R_MakeExternalPtr(vectors, vectors, ufo);
    UNPROTECT(2);

    return ufo;
}

