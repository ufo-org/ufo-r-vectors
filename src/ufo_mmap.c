#include "../include/ufos.h"
#include "../include/ufos_writeback.h"

#include "ufo_mmap.h"
#include "safety_first.h"
#include "helpers.h"
#include "evil/bad_strings.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdio.h>

#include "Rinternals.h"

typedef struct ufo_mmap_data_t {
    const char **paths;
    size_t       n_paths;
    R_xlen_t    *offsets;
    R_xlen_t    *extents;
    size_t       length;
    char         fill;
} ufo_mmap_data_t;

ufo_mmap_data_t *ufo_mmap_data_create(
    SEXP/*STRSXP*/ paths_sexp, 
    SEXP/*LEN*/ offsets_sexp, 
    SEXP/*LEN*/ extents_sexp
) {
    make_sure(
        XLENGTH(offsets_sexp) == XLENGTH(extents_sexp),     
        "The number of offsets is not equal to the number of extents provided"
    );

    make_sure(
        XLENGTH(paths_sexp) > 0,
        "Provided an empty vector of paths"
    );

    make_sure(
        XLENGTH(paths_sexp) == 1 || (XLENGTH(paths_sexp) == XLENGTH(offsets_sexp)),
        "Must provide a single path or a path for every offset/extent pair"
    );

    ufo_mmap_data_t *data = (ufo_mmap_data_t*) malloc(sizeof(ufo_mmap_data_t));
    data->paths   = __extract_path_array_or_die(paths_sexp);       // FIXME this can blow up memory
    data->offsets = __extract_R_xlen_t_array_or_die(offsets_sexp); // FIXME this can blow up memory
    data->extents = __extract_R_xlen_t_array_or_die(extents_sexp); // FIXME this can blow up memory
    data->length  = XLENGTH(offsets_sexp);
    data->n_paths = XLENGTH(paths_sexp);
    return data;
}

void ufo_mmap_data_destroy(void *data) {
    ufo_mmap_data_t *ufo_mmap_data = (ufo_mmap_data_t*) data;
    free(ufo_mmap_data->offsets);
    free(ufo_mmap_data->extents);
    free(ufo_mmap_data);
}

const char *ufo_mmap_data_path(ufo_mmap_data_t *data, size_t index) {
    if (data->n_paths == 1) {
        return data->paths[0];
    } else {
        return data->paths[index];
    }
}

typedef struct {
    char  **paths;
    char  **contents;
    size_t *lengths;
    size_t  size;
    size_t  max_size;
} unique_maps_t;

unique_maps_t *unique_maps_new(size_t initial_size) {
    unique_maps_t *maps = (unique_maps_t *) malloc(sizeof(unique_maps_t));
    maps->contents = (char **)  malloc(sizeof(char *) * initial_size);
    maps->paths    = (char **)  malloc(sizeof(char *) * initial_size);
    maps->lengths  = (size_t *) malloc(sizeof(size_t) * initial_size);
    maps->size = 0;
    maps->max_size = initial_size;
    return maps;
}

void unique_maps_destroy(unique_maps_t* maps) {
    for (size_t i = 0; i < maps->size; i++) {
        free(maps->paths[i]);
        munmap(maps->contents[i], maps->lengths[i]);
    }
    free(maps->paths);
    free(maps->contents);
    free(maps->lengths);
    free(maps);
}

bool unique_maps_expand(unique_maps_t* maps) {
    maps->max_size *= 2;

    maps->contents = realloc(maps->contents, sizeof(char *) * maps->max_size);
    maps->paths    = realloc(maps->paths,    sizeof(char *) * maps->max_size);
    maps->lengths  = realloc(maps->paths,    sizeof(char *) * maps->max_size);

    return maps->contents != NULL && maps->paths != NULL && maps->lengths != NULL;
}

char *map_file(const char *path, size_t *length, bool read_only) {
    int fd = open(path, read_only ? O_RDONLY : O_RDWR);
    if (fd < 0) {
        perror("Cannot open file");
        return NULL;
    }
    struct stat sb;
    fstat(fd, &sb);
    char *contents = mmap(NULL, sb.st_size, read_only ? PROT_READ : PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);    
    if (contents == MAP_FAILED) {
        perror("Cannot map file");
        return NULL;
    }
    *length = sb.st_size;
    return contents;
}

char *unique_maps_open(unique_maps_t* maps, const char *path, bool read_only) {
    size_t path_length = strlen(path);

    for (size_t i = 0; i < maps->size; i++) {
        if (maps->lengths[i] == path_length) {
            if (0 == strncmp(maps->paths[i], path, path_length)) {
                return maps->contents[i];
            }
        }
    }

    if (maps->max_size <= maps->size) {
        // Warning: potentially invalidates existing pointers into maps->contents
        if (!unique_maps_expand(maps)) {
            printf("EXPANSION\n");
            return NULL;
        }
    }
    
    char *contents = map_file(path, &maps->lengths[maps->size], read_only);
    if (contents == NULL) {
        printf("CONTENTS\n");
        return NULL;
    }

    maps->contents[maps->size] = contents;
    maps->size++;

    return contents;
}

int populate_strsxp_mmap(void* userData, uintptr_t startValueIdx, uintptr_t endValueIdx, unsigned char* target) {
    ufo_mmap_data_t * data = (ufo_mmap_data_t *) userData;

    unique_maps_t *maps = unique_maps_new(1);
    if (maps == NULL) return 1;

    for (size_t i = 0; i < endValueIdx - startValueIdx; i++) {

        const char *path = ufo_mmap_data_path(data, i);
        // printf("%ld :: path = %s\n", i, path);
        if (path == NULL) return 2;

        const char *map = unique_maps_open(maps, path, true);
        // printf("%ld :: map = %p\n", i, map);
        if (map == NULL) return 3;
        
        // printf("XXX %ld :: %p : %ld + %ld\n", i,  map, data->offsets[i], data->extents[i]);
        const char *string_ptr = map + data->offsets[i];                
        SEXP/*CHARSXP*/ string = mkBadCharN(string_ptr, data->extents[i]);
        // printf("XXX %ld :: '%s'\n", i, CHAR(string));
        // for (int i = 0; i < data->extents[i], )
        ((SEXP/*CHARSXP*/ *) target)[i] = string;
    }

    return 0;
}

void writeback_strsxp_mmap(void *userData, UfoWriteListenerEvent event) {

    printf("WRITEBACK EVENT %i\n", event.tag);

    if (event.tag != Writeback) return;
    
    uintptr_t startValueIdx = event.writeback.start_idx;
    uintptr_t endValueIdx = event.writeback.end_idx;

    ufo_mmap_data_t * data = (ufo_mmap_data_t *) userData;

    unique_maps_t *maps = unique_maps_new(1);
    if (maps == NULL) return; // ERROR

    for (size_t i = 0; i < endValueIdx - startValueIdx; i++) {
        const char *path = ufo_mmap_data_path(data, i);
        if (path == NULL) return; // ERROR

        char *map = unique_maps_open(maps, path, false);
        if (map == NULL) return; // ERROR

        SEXP *memory_contents = (SEXP *) event.writeback.data;
        SEXP /*CHARSXP*/ string_in_memory = memory_contents[i];
        const char *contents = CHAR(string_in_memory);

        R_xlen_t contents_length = XLENGTH(string_in_memory);
        R_xlen_t length_in_map = data->extents[i];               

        char *ptr_in_map = &map[data->offsets[i]];

        if (contents_length > length_in_map) {
            contents_length = length_in_map; // truncate
        }
        
        // printf("%ld :: overwriting offset %ld at '%s' by '%s'\n", i, data->offsets[i], ptr_in_map, contents);
        strncpy(ptr_in_map, contents, length_in_map);

        for (size_t j = contents_length; j < length_in_map; j++) {
            map[data->offsets[i] + j] = data->fill;
            // printf("%ld :: writing [%ld] '%c'\n", i, j, data->fill);
        }
    }
}

SEXP ufo_strsxp_mmap(
    SEXP/*STRSXP*/ path_sexp, 
    SEXP/*LEN*/ offsets_sexp, 
    SEXP/*LEN*/ extents_sexp, 
    SEXP/*STRSXP*/ fill_sexp,
    SEXP/*LGLSXP*/ read_only_sexp, 
    SEXP/*INTSXP*/ min_load_count_sexp
) {
    bool read_only     = __extract_boolean_or_die(read_only_sexp);
	int min_load_count = __extract_int_or_die(min_load_count_sexp);

    ufo_source_t *source = (ufo_source_t*) malloc(sizeof(ufo_source_t));
    source->data = (void*) ufo_mmap_data_create(path_sexp, offsets_sexp, extents_sexp);

    source->vector_type = UFO_STR;
    source->element_size = __get_element_size(UFO_STR);

    source->vector_size = XLENGTH(offsets_sexp);

    source->dimensions = NULL;
    source->dimensions_length = 0;
    source->read_only = read_only;
    source->min_load_count = __select_min_load_count(min_load_count, source->element_size);

    source->population_function = &populate_strsxp_mmap;
    source->destructor_function = &ufo_mmap_data_destroy;
    source->writeback_function = read_only ? NULL : writeback_strsxp_mmap;

    ufo_new_t ufo_new = (ufo_new_t) R_GetCCallable("ufos", "ufo_new");
    return ufo_new(source);
}

