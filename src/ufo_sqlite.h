#pragma once

#include "Rinternals.h"

SEXP ufo_sqlite_test();

SEXP ufo_sqlite_column(SEXP/*STRSXP*/ db, SEXP/*STRSXP*/ table, SEXP/*STRSXP*/ column, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count);
SEXP ufo_sqlite_table(SEXP/*STRSXP*/ db, SEXP/*STRSXP*/ table, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count);
