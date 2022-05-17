#include "ufo_bz2.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
// #include <pthread.h>
// #include <errno.h>

#include "safety_first.h"

#define USE_RINTERNALS
#include <R.h>
#include <Rinternals.h>

#include "../include/ufos.h"
#include "helpers.h"
#include "debug.h"

//#include "evil/bad_strings.h"

#include "ufo_bz2.h"
#include "bzip2/blocks.h"
#include "bzip2/block.h"

// TODO The buffer should probably be handled by R?
#define DECOMPRESSED_BUFFER_SIZE 1024 * 1024 * 1024 // 900kB + some room just in case

// #include <bzlib.h>
typedef struct {
    Blocks *blocks;
    size_t element_width;
    size_t decompressed_buffer_size;
} BZip2;


BZip2 *BZip2_new(ufo_vector_type_t type, const char* path) {
    // Pre-scan the file
    Blocks *blocks = Blocks_new(path, DECOMPRESSED_BUFFER_SIZE);

    // Check for bad blocks
    if (blocks->bad_blocks > 0) {
        Blocks_free(blocks);
        Rf_error("UFO some blocks could not be read. Quitting.\n");
        return NULL;
    }

    BZip2 *bzip2 = (BZip2 *) malloc(sizeof(BZip2));
    bzip2->element_width = __get_element_size(UFO_RAW);
    bzip2->blocks = blocks;
    return bzip2;
}

int32_t BZip2_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    BZip2 *bzip2 = (BZip2 *) user_data;
    return Blocks_read(bzip2->blocks, start * bzip2->element_width, end * bzip2->element_width, target);
}

void BZip2_free(void* data) {
    BZip2 *bzip2 = (BZip2 *) data;
    Blocks_free(bzip2->blocks);
    free(bzip2);
    // Everything else is handled by UFO-R.
}

/*ufo_vector_type_t result_type, */
SEXP ufo_bzip2(ufo_vector_type_t type, SEXP/*STRSXPXP*/ filename, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {

    // Read the arguements into practical types (with checks).
    bool read_only_value = __extract_boolean_or_die(read_only);
    int min_load_count_value = __extract_int_or_die(min_load_count);
    const char *path = __extract_path_or_die(filename);    

    size_t element_size = __get_element_size(type);
    BZip2 *bzip2 = BZip2_new(element_size, path);
   
    // Create a source struct for UFOs.
    ufo_source_t* source = (ufo_source_t*) malloc(sizeof(ufo_source_t));

    // Element size and count metadata
    source->vector_type = type;
    source->element_size = __get_element_size(type);
    source->vector_size = bzip2->blocks->decompressed_size;

    // Behavior specification
    source->data = (void*) bzip2;
    source->destructor_function = BZip2_free; //&destroy_data;
    source->population_function = BZip2_populate;
    source->writeback_function = NULL;
    
    // Chunk-related parameters
    source->read_only = read_only_value;
    source->min_load_count = __select_min_load_count(min_load_count_value, source->element_size);

    // Unused.
    source->dimensions = NULL;
    source->dimensions_length = 0;

    ufo_new_t ufo_new = (ufo_new_t) R_GetCCallable("ufos", "ufo_new");
    return ufo_new(source);
}

SEXP ufo_intsxp_bzip2 (SEXP/*STRSXP*/ path, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    return ufo_bzip2(UFO_INT, path, read_only, min_load_count);
}
SEXP ufo_realsxp_bzip2(SEXP/*STRSXP*/ path, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    return ufo_bzip2(UFO_REAL, path, read_only, min_load_count);
}
SEXP ufo_rawsxp_bzip2 (SEXP/*STRSXP*/ path, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    return ufo_bzip2(UFO_RAW, path, read_only, min_load_count);
}
SEXP ufo_cplxsxp_bzip2(SEXP/*STRSXP*/ path, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    return ufo_bzip2(UFO_CPLX, path, read_only, min_load_count);
}
SEXP ufo_lglsxp_bzip2 (SEXP/*STRSXP*/ path, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    return ufo_bzip2(UFO_LGL, path, read_only, min_load_count);
}
SEXP ufo_vecsxp_bzip2 (SEXP/*STRSXP*/ path, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    return ufo_bzip2(UFO_VEC, path, read_only, min_load_count);
}
SEXP ufo_charsxp_bzip2 (SEXP/*STRSXP*/ path, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    return ufo_bzip2(UFO_CHAR, path, read_only, min_load_count);
}

// TODO probably doable
SEXP ufo_strsxp_bzip2 (SEXP/*STRSXP*/ path, SEXP/*LGLSXP*/ read_only, SEXP/*INTSXP*/ min_load_count) {
    SEXP string_vector;
    SEXP character_vector;
    PROTECT(string_vector = allocVector(STRSXP, 1));
    PROTECT(character_vector = ufo_charsxp_bzip2(path, read_only, min_load_count));
    SET_STRING_ELT(string_vector, 0, character_vector);
    UNPROTECT(2);
    return string_vector;
}