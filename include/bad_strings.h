#pragma once

#include <R.h>
#define USE_RINTERNALS
#include <Rinternals.h>

SEXP/*CHARSXP*/ allocBadCharacter(R_xlen_t size);
SEXP/*CHARSXP*/ allocBadCharacter3(R_xlen_t size, R_allocator_t* allocator);
SEXP/*CHARSXP*/ allocBadCharacterFromContents(const char *contents);
SEXP/*CHARSXP*/ allocBadCharacter3FromContents(const char *contents, R_allocator_t* allocator);