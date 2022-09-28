#include "helpers.h"
#include <assert.h>
#include <stdint.h>

#include "../include/ufos.h"

int __extract_int_or_die(SEXP/*INTSXP*/ sexp) {
    if (TYPEOF(sexp) != INTSXP) {
        Rf_error("Invalid type for integer vector: %s\n", type2char(TYPEOF(sexp)));
    }

    if (LENGTH(sexp) == 0) {
        Rf_error("Provided a zero length vector for integer vector\n");
    }

    if (LENGTH(sexp) > 1) {
        Rf_warning("Provided multiple values for integer vector, "
                           "using the first one only\n");
    }

    return INTEGER_ELT(sexp, 0);
}

int __extract_boolean_or_die(SEXP/*LGLSXP*/ sexp) {
    if (TYPEOF(sexp) != LGLSXP) {
        Rf_error("Invalid type for boolean vector: %s\n", type2char(TYPEOF(sexp)));
    }

    if (LENGTH(sexp) == 0) {
        Rf_error("Provided a zero length vector for boolean vector\n");
    }

    if (LENGTH(sexp) > 1) {
        Rf_warning("Provided multiple values for boolean vector, "
                           "using the first one only\n");
    }

    int element = LOGICAL_ELT(sexp, 0);
    return element == 1;
}

R_xlen_t __extract_R_xlen_t_or_die(SEXP/*REALSXP|INTSXP*/ sexp) {
	if (TYPEOF(sexp) != REALSXP &&  TYPEOF(sexp) != INTSXP) {
        Rf_error("Invalid type for R_xlen_t vector: %s\n", type2char(TYPEOF(sexp)));
    }

    if (LENGTH(sexp) == 0) {
        Rf_error("Provided a zero length vector for R_xlen_t vector\n");
    }

    if (LENGTH(sexp) > 1) {
        Rf_warning("Provided multiple values for R_xlen_t vector, "
                   "using the first one only\n");
    }

    if (sizeof(int64_t) != sizeof(double)) {
        Rf_warning("Expecting double and int64_t to be same size, but they are %li and %li, respectively",
        		   sizeof(int64_t), sizeof(double));
    }

    int64_t value = TYPEOF(sexp) == REALSXP ? (int64_t) REAL_ELT(sexp, 0)
    		                                : (int64_t) INTEGER_ELT(sexp, 0);

    if (value > SIZE_MAX) {
    	Rf_error("Cannot convert REALSXP to R_xlen_t, value %li is larger than SIZE_MAX=%li", value, SIZE_MAX);
    }

    return (R_xlen_t) value;
}


R_xlen_t *__extract_R_xlen_t_array_or_die(SEXP/*REALSXP|INTSXP*/ sexp) {
	if (TYPEOF(sexp) != REALSXP &&  TYPEOF(sexp) != INTSXP) {
        Rf_error("Invalid type for R_xlen_t vector: %s\n", type2char(TYPEOF(sexp)));
    }

    if (sizeof(R_xlen_t) != sizeof(double)) {
        Rf_warning("Expecting double and int64_t to be same size, but they are %li and %li, respectively",
        		   sizeof(int64_t), sizeof(double));
    }

    R_xlen_t length = XLENGTH(sexp);
    R_xlen_t *result = (R_xlen_t *) malloc(sizeof(int64_t) * length);

    if (TYPEOF(sexp) == REALSXP) {
        for (R_xlen_t i = 0; i < length; i++) {
            R_xlen_t value = (R_xlen_t) REAL_ELT(sexp, i);
            if (value > SIZE_MAX) {
        	    Rf_error("Cannot convert REALSXP to R_xlen_t, value %li is larger than SIZE_MAX=%li", value, SIZE_MAX);
            }
            result[i] = value;
        }
    } else {
        for (R_xlen_t i = 0; i < length; i++) {
            result[i] = (R_xlen_t) INTEGER_ELT(sexp, i);
        }
    }   

    return (R_xlen_t *) result;
}

/**
 * Check if the path provided via this SEXP makes sense:
 *  - it contains strings,
 *  - it's has at least one element,
 *  - preferably it has only one element.
 *
 * If the number of elements in the path vector is too large the function exits
 * but prints a warning in the R interpreter. If the other requirements are not
 * met the function dies and shows a message to the R interpreter.
 *
 * If the path makes sense, it is is extracted into a C string.
 *
 * @param path A SEXP containing the file path to validate.
 * @return
 */
const char* __extract_path_or_die(SEXP/*STRSXP*/ path) {
    if (TYPEOF(path) != STRSXP) {
        Rf_error("Invalid type for paths: %s\n", type2char(TYPEOF(path)));
    }

    if (LENGTH(path) == 0) {
        Rf_error("Provided a zero length string for path\n");
    }

    if (TYPEOF(STRING_ELT(path, 0)) != CHARSXP) {
        Rf_error("Invalid type for path: %s\n", type2char(TYPEOF(STRING_ELT(path, 0))));
    }

    if (LENGTH(path) > 2) {
        Rf_warning("Provided multiple string values for path, "
                           "using the first one only\n");
    }

    // Copy string and return the copy.
    const char *tmp = CHAR(STRING_ELT(path, 0));
    char *ret = (char *) malloc(sizeof(char) * (strlen(tmp) + 1));  // FIXME reclaim
    strcpy(ret, tmp);
    ret[strlen(tmp)] = '\0';
    return ret;
}

const char **__extract_path_array_or_die(SEXP/*STRSXP*/ paths) {
    if (TYPEOF(paths) != STRSXP) {
        Rf_error("Invalid type for paths: %s\n", type2char(TYPEOF(paths)));
    }

    if (TYPEOF(STRING_ELT(paths, 0)) != CHARSXP) {
        Rf_error("Invalid type for path: %s\n", type2char(TYPEOF(STRING_ELT(paths, 0))));
    }

    R_xlen_t length = XLENGTH(paths);
    const char **ret = (const char **) malloc(sizeof(char *) * length);

    for (R_xlen_t i = 0; i < length; i++) {
        SEXP/*CHARSXP*/ tmp = STRING_ELT(paths, i);
        R_xlen_t tmp_length = XLENGTH(tmp);
        char *string = (char *) malloc(sizeof(char) * (tmp_length + 1));
        strncpy(string, CHAR(tmp), tmp_length);
        string[tmp_length] = '\0';
        ret[i] = (const char *) string;
    }    

    return ret;
}

const char* __extract_string_or_die(SEXP/*STRSXP*/ string) {
    if (TYPEOF(string) != STRSXP) {
        Rf_error("Invalid type for string vector: %s\n", type2char(TYPEOF(string)));
    }

    if (LENGTH(string) == 0) {
        Rf_error("Provided a zero length string vector\n");
    }

    if (TYPEOF(STRING_ELT(string, 0)) != CHARSXP) {
        Rf_error("Invalid type for string vector: %s\n", type2char(TYPEOF(STRING_ELT(string, 0))));
    }

    if (LENGTH(string) > 1) {
        Rf_warning("Provided multiple string values, "
                           "using the first one only\n");
    }

    // Copy string and return the copy.
    const char *tmp = CHAR(STRING_ELT(string, 0));
    char *ret = (char *) malloc(sizeof(char) * strlen(tmp));  // FIXME reclaim
    strcpy(ret, tmp);
    return ret;
}

char __extract_char_or_die(SEXP/*STRSXP*/ string) {
    if (TYPEOF(string) != STRSXP) {
        Rf_error("Invalid type for string vector: %s\n", type2char(TYPEOF(string)));
    }

    if (LENGTH(string) == 0) {
        Rf_error("Provided a zero length string vector\n");
    }

    if (TYPEOF(STRING_ELT(string, 0)) != CHARSXP) {
        Rf_error("Invalid type for string vector: %s\n", type2char(TYPEOF(STRING_ELT(string, 0))));
    }

    if (LENGTH(string) > 1) {
        Rf_warning("Provided multiple string values, "
                           "using the first one only\n");
    }

    // Copy string and return the copy.
    SEXP /*CHARSXP*/ tmp = STRING_ELT(string, 0);

    if (LENGTH(tmp) == 0) {
        Rf_error("Provided an empty string, "
                 "cannot extract a character from it\n");
    }

    if (LENGTH(tmp) > 1) {
        Rf_error("Provided a string of length > 1, "
                   "but expecting a single character");
    }

    return CHAR(tmp)[0];
}

int32_t __select_min_load_count(int32_t min_load_count, size_t element_size) {
	if (min_load_count > 0) {
		return min_load_count;
	} else {
		return __1MB_of_elements(element_size);
	}
}

/**
 * Calculates how many elements fit in 1 MB of memory.
 *
 * @param element_size
 * @return number of elements
 */
int32_t __1MB_of_elements(size_t element_size) {
    assert (element_size < 1 << 24);
    assert (element_size > 0);
    return (1024 * 1024) / ((int32_t) element_size);
}


/**
 * Translates the type of UFO vector to the size of its element in bytes.
 *
 * @param vector_type The type of the vector as specified by the ufo_vector_type
 *                    enum.
 * @return Returns the size of the element vector or dies in case of an
 *         unrecognized vector type.
 */
size_t __get_element_size(SEXPTYPE vector_type) {
    switch (vector_type) {
        case CHARSXP:
            return sizeof(Rbyte);
        case LGLSXP:
            return sizeof(Rboolean);
        case INTSXP:
            return sizeof(int);
        case REALSXP:
            return sizeof(double);
        case CPLXSXP:
            return sizeof(Rcomplex);
        case RAWSXP:
            return sizeof(Rbyte);
        case STRSXP:
            return sizeof(SEXP/*STRSXP*/);
        default:
            Rf_error("Unrecognized vector type: %s\n", type2char(vector_type));
    }
}
