#include <tol.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

tol_monolith_t* tol_load_monolith(char const* filename)
{
    tol_monolith_t* monolith = TOL_CALLOC(tol_monolith_t, 1);
    FILE* file = fopen(filename, "rb");
    assert(file && "Unable to open file");
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    monolith->data = TOL_MALLOC(uint8_t, fsize + 1);
    fread(monolith->data, fsize, 1, file);
    fclose(file);
    monolith->data[fsize] = 0;
    monolith->nclades = 0;
    for (long i = 0; i < fsize; i++) {
        if (monolith->data[i] == '\n') {
            monolith->nclades++;
            monolith->data[i] = 0;
        }
    }
    monolith->parents = TOL_MALLOC(int32_t, monolith->nclades);
    monolith->labels = TOL_MALLOC(char const*, monolith->nclades);
    uint8_t* data = monolith->data;
    for (long j = 0; j < monolith->nclades; j++) {
        assert(' ' == data[7]);
        data[7] = 0;
        monolith->parents[j] = atoi((const char*) data);
        monolith->labels[j] = (char const*) (data += 8);
        data += strlen(monolith->labels[j]) + 1;
    }
    return monolith;
}

void tol_free_monolith(tol_monolith_t* monolith)
{
    if (monolith) {
        TOL_FREE(monolith->data);
        TOL_FREE(monolith->parents);
        TOL_FREE(monolith->labels);
        TOL_FREE(monolith);
    }
}
