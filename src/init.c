#include "../include/ufos.h"
#include "ufo_vectors.h"
#include "ufo_empty.h"
#include "ufo_csv.h"
#include "ufo_seq.h"
#include "ufo_psql.h"
#include "ufo_sqlite.h"
#include "ufo_sqlite.h"
#include "ufo_bz2.h"
#include "ufo_write_protect.h"
#include "ufo_bind.h"

#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>

// List of functions provided by the package.
static const R_CallMethodDef CallEntries[] = {

    // Constructors for vectors that partially materialize on-demand from
    // binary files.
    {"vectors_intsxp_bin",      (DL_FUNC) &ufo_vectors_intsxp_bin,          3},
    {"vectors_realsxp_bin",     (DL_FUNC) &ufo_vectors_realsxp_bin,         3},
    {"vectors_cplxsxp_bin",     (DL_FUNC) &ufo_vectors_cplxsxp_bin,         3},
    {"vectors_lglsxp_bin",      (DL_FUNC) &ufo_vectors_lglsxp_bin,          3},
    {"vectors_rawsxp_bin",      (DL_FUNC) &ufo_vectors_rawsxp_bin,          3},

    // Constructors for matrices composed of the above-mentioned vectors.
    {"matrix_intsxp_bin",       (DL_FUNC) &ufo_matrix_intsxp_bin,           5},
    {"matrix_realsxp_bin",      (DL_FUNC) &ufo_matrix_realsxp_bin,          5},
    {"matrix_cplxsxp_bin",      (DL_FUNC) &ufo_matrix_cplxsxp_bin,          5},
    {"matrix_lglsxp_bin",       (DL_FUNC) &ufo_matrix_lglsxp_bin,           5},
    {"matrix_rawsxp_bin",       (DL_FUNC) &ufo_matrix_rawsxp_bin,           5},

	// Constructors for empty vectors.
	{"intsxp_empty",			(DL_FUNC) &ufo_intsxp_empty,				3},
	{"realsxp_empty",			(DL_FUNC) &ufo_realsxp_empty,				3},
	{"cplxsxp_empty",			(DL_FUNC) &ufo_cplxsxp_empty,				3},
	{"lglsxp_empty",			(DL_FUNC) &ufo_lglsxp_empty,				3},
	{"rawsxp_empty",			(DL_FUNC) &ufo_rawsxp_empty,				2},
	{"strsxp_empty",			(DL_FUNC) &ufo_strsxp_empty,				3},
	{"vecsxp_empty",			(DL_FUNC) &ufo_vecsxp_empty,				2},

    // Sequences
    {"intsxp_seq",				(DL_FUNC) &ufo_intsxp_seq,					5},
	{"realsxp_seq",				(DL_FUNC) &ufo_realsxp_seq,					5},

    // BZip2
    {"intsxp_bzip2",            (DL_FUNC) &ufo_intsxp_bzip2,                3},
    {"realsxp_bzip2",           (DL_FUNC) &ufo_realsxp_bzip2,               3},
    {"rawsxp_bzip2",            (DL_FUNC) &ufo_rawsxp_bzip2,                3},
    {"cplxsxp_bzip2",           (DL_FUNC) &ufo_cplxsxp_bzip2,               3},
    {"lglsxp_bzip2",            (DL_FUNC) &ufo_lglsxp_bzip2,                3},
    {"vecsxp_bzip2",            (DL_FUNC) &ufo_vecsxp_bzip2,                3},
    {"strsxp_bzip2",            (DL_FUNC) &ufo_strsxp_bzip2,                3},
    
    // Write protect
    {"write_protect",           (DL_FUNC) &ufo_write_protect,               3},

    // Bind multiple vectors
    {"bind",					(DL_FUNC) &ufo_bind,						3},

    // CSV support
    {"csv",						(DL_FUNC) &ufo_csv,							6},

    // PSQL column
    {"psql",        			(DL_FUNC) &ufo_psql,						5},

    // SQLite
    {"sqlite_column",  			(DL_FUNC) &ufo_sqlite_column,   	 		6},

    {"test",					(DL_FUNC) &test,  							0},

    // Storage.
    {"store_bin",				(DL_FUNC) &ufo_store_bin,					2},

    // Turn on debug mode.
    {"vectors_set_debug_mode",  (DL_FUNC) &ufo_vectors_set_debug_mode,      1},

    // Terminates the function list. Necessary.
    {NULL,						NULL,										0}
};

// Initializes the package and registers the routines with the Rdynload
// library. Name follows the pattern: R_init_<package_name> 
void attribute_visible R_init_ufovectors(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE); // causes failure to lookup the ufo_get_chunk symbol
}
