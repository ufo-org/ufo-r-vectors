#pragma once

#include "Rinternals.h"

SEXP ufo_strsxp_mmap(
    SEXP/*STRSXP*/ path, 
    SEXP/*LEN*/ offsets, 
    SEXP/*LEN*/ extents, 
    SEXP/*STRSXP*/ fill,
    SEXP/*LGLSXP*/ read_only, 
    SEXP/*INTSXP*/ min_load_count
);