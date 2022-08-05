#include "ufo_sqlite.h"

#include "sqlite/sqlite.h"


SEXP ufo_sqlite_test() {

    char **column_names;
    size_t column_count;
    sqlite_table_columns("test.db", "sharks", &column_names, &column_count);

    printf("Columns: %ld\n", column_count);
    for (size_t i = 0; i < column_count; i++) {
        printf("  - column [%ld]: %s\n", i, column_names[i]);
    }    


    for (size_t i = 0; i < column_count; i++) {
        free(column_names[i]);
    }
    free(column_names);

    return R_NilValue;
}