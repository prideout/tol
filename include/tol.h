#include <stdint.h>
#include <parg.h>

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
    parg_buffer* buffer;
} tol_monolith_t;

tol_monolith_t* tol_load_monolith(parg_token);
void tol_free_monolith(tol_monolith_t* monolith);
