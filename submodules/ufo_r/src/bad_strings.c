#include "bad_strings.h"

#ifndef intCHARSXP
#define intCHARSXP 73
#endif

#ifndef CHAR
#define CHAR(x) ((const char *) STDVEC_DATAPTR(x))
#endif

#ifndef CHAR_RW
#define CHAR_RW(x) ((char *) CHAR(x))
#endif

#ifndef ASCII_MASK
#define ASCII_MASK (1<<6)
#endif

#ifndef SET_ASCII
#define SET_ASCII(x) ((x)->sxpinfo.gp) |= ASCII_MASK
#endif

SEXP/*CHARSXP*/ allocBadCharacter(R_xlen_t size) {
    return allocBadCharacter3(size, NULL);
}
SEXP/*CHARSXP*/ allocBadCharacter3(R_xlen_t size, R_allocator_t* allocator) {
    SEXP bad_string;
    PROTECT(bad_string = allocVector3(intCHARSXP, size, allocator));
    SET_ASCII(bad_string); 
    UNPROTECT(1);
    return bad_string;
}
SEXP/*CHARSXP*/ allocBadCharacterFromContents(const char *contents) {
    return allocBadCharacter3FromContents(contents, NULL);
}

SEXP/*CHARSXP*/ allocBadCharacter3FromContents(const char *contents, R_allocator_t* allocator) {
    if (0 == strcmp(contents, ""))       return R_BlankString;
    if (0 == strcmp(contents, "NA"))     return NA_STRING;
    //if (0 == strcmp(contents, "base"))   return R_BaseNamespaceName;

    SEXP/*CHARSXP*/ bad_string;
    R_len_t size = strlen(contents);

    PROTECT(bad_string = allocVector3(intCHARSXP, size, allocator));
    memcpy(CHAR_RW(bad_string), contents, size);
    SET_ASCII(bad_string);                                  // FIXME
    UNPROTECT(1);
    return bad_string;
}