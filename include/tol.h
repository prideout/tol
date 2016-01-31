#include <stdint.h>

#ifndef TOL_MALLOC
#define TOL_MALLOC(T, N) ((T*) malloc(N * sizeof(T)))
#define TOL_CALLOC(T, N) ((T*) calloc(N * sizeof(T), 1))
#define TOL_REALLOC(T, BUF, N) ((T*) realloc(BUF, sizeof(T) * N))
#define TOL_FREE(BUF) free(BUF)
#endif

typedef struct {
    uint8_t* data;
    int32_t nclades;
    int32_t* parents;
    char const** labels;
} tol_monolith_t;

tol_monolith_t* tol_load_monolith(char const* filename);
void tol_free_monolith(tol_monolith_t* monolith);
