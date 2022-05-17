#include "ufos.h"

#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>

#include "ufo_operators.h"

// List of functions provided by the package.
static const R_CallMethodDef CallEntries[] __attribute__ ((unused)) = {
    // Start up and shutdown the system.
    {"ufo_initialize", (DL_FUNC) &ufo_initialize, 2},
    {"ufo_shutdown", (DL_FUNC) &ufo_shutdown, 0},
	{"is_ufo", (DL_FUNC) &is_ufo, 1},

    // Terminates the function list. Necessary.
    {NULL, NULL, 0} 
};

// Initializes the package and registers the routines with the Rdynload 
// library. Name follows the pattern: R_init_<package_name> 
void attribute_visible R_init_ufos(DllInfo *dll) {
    R_RegisterCCallable("ufos", "ufo_new", (DL_FUNC) &ufo_new);
    R_RegisterCCallable("ufos", "ufo_new_multidim", (DL_FUNC) &ufo_new_multidim);

    element_as_integer = (element_as_integer_t) R_GetCCallable("ufos", "element_as_integer");
    element_as_real    = (element_as_real_t)    R_GetCCallable("ufos", "element_as_real");
    element_as_complex = (element_as_complex_t) R_GetCCallable("ufos", "element_as_complex");
    element_as_string  = (element_as_string_t)  R_GetCCallable("ufos", "element_as_string");
    element_as_logical = (element_as_logical_t) R_GetCCallable("ufos", "element_as_logical");
    element_as_raw     = (element_as_raw_t)     R_GetCCallable("ufos", "element_as_raw");
    complex            = (complex_t)            R_GetCCallable("ufos", "complex");
    integer_as_logical = (integer_as_logical_t) R_GetCCallable("ufos", "integer_as_logical");
    real_as_logical    = (real_as_logical_t)    R_GetCCallable("ufos", "real_as_logical");
    complex_as_logical = (complex_as_logical_t) R_GetCCallable("ufos", "complex_as_logical");
    string_as_logical  = (string_as_logical_t)  R_GetCCallable("ufos", "string_as_logical");
    real_as_integer    = (real_as_integer_t)    R_GetCCallable("ufos", "real_as_integer");
    complex_as_integer = (complex_as_integer_t) R_GetCCallable("ufos", "complex_as_integer");
    string_as_integer  = (string_as_integer_t)  R_GetCCallable("ufos", "string_as_integer");
    logical_as_integer = (logical_as_integer_t) R_GetCCallable("ufos", "logical_as_integer");
    raw_as_integer     = (raw_as_integer_t)     R_GetCCallable("ufos", "raw_as_integer");
    integer_as_real    = (integer_as_real_t)    R_GetCCallable("ufos", "integer_as_real");
    complex_as_real    = (complex_as_real_t)    R_GetCCallable("ufos", "complex_as_real");
    string_as_real     = (string_as_real_t)     R_GetCCallable("ufos", "string_as_real");
    logical_as_real    = (logical_as_real_t)    R_GetCCallable("ufos", "logical_as_real");
    raw_as_real        = (raw_as_real_t)        R_GetCCallable("ufos", "raw_as_real");
    integer_as_complex = (integer_as_complex_t) R_GetCCallable("ufos", "integer_as_complex");
    real_as_complex    = (real_as_complex_t)    R_GetCCallable("ufos", "real_as_complex");
    string_as_complex  = (string_as_complex_t)  R_GetCCallable("ufos", "string_as_complex");
    logical_as_complex = (logical_as_complex_t) R_GetCCallable("ufos", "logical_as_complex");
    raw_as_complex     = (raw_as_complex_t)     R_GetCCallable("ufos", "raw_as_complex");
    integer_as_string  = (integer_as_string_t)  R_GetCCallable("ufos", "integer_as_string");
    real_as_string     = (real_as_string_t)     R_GetCCallable("ufos", "real_as_string");
    complex_as_string  = (complex_as_string_t)  R_GetCCallable("ufos", "complex_as_string");
    logical_as_string  = (logical_as_string_t)  R_GetCCallable("ufos", "logical_as_string");
    raw_as_string      = (raw_as_string_t)      R_GetCCallable("ufos", "raw_as_string");
    logical_as_raw     = (logical_as_raw_t)     R_GetCCallable("ufos", "logical_as_raw");
}
