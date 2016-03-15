#include <stdint.h>

#ifndef TOL_MALLOC
#define TOL_MALLOC(T, N) ((T*) malloc(N * sizeof(T)))
#define TOL_CALLOC(T, N) ((T*) calloc(N * sizeof(T), 1))
#define TOL_REALLOC(T, BUF, N) ((T*) realloc(BUF, sizeof(T) * N))
#define TOL_FREE(BUF) free(BUF)
#define TOL_MAX(a, b) (a > b ? a : b)
#endif

typedef struct {
    uint8_t* data;
    int32_t buflen;
    int32_t nclades;
    int32_t* parents;
    int32_t* ids;
    int32_t maxid;
    char const** labels;
} tol_monolith_t;

tol_monolith_t* tol_load_monolith(uint8_t const* data, int nbytes);
void tol_free_monolith(tol_monolith_t* monolith);
tol_monolith_t* tol_monolith_pack(tol_monolith_t const* src);
void tol_monolith_merge(tol_monolith_t* dst, tol_monolith_t const* src);
