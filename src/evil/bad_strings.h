#pragma once

#include <R.h>

SEXP/*CHARSXP*/ mkBadChar   (const char* contents);
SEXP/*CHARSXP*/ mkBadCharN  (const char* contents, R_len_t size);
SEXP/*STRSXP*/  mkBadString (const char* contents);