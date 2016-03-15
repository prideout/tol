#include "tol.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

tol_monolith_t* tol_load_monolith(uint8_t const* src, int nbytes)
{
    tol_monolith_t* monolith = TOL_CALLOC(tol_monolith_t, 1);
    monolith->buflen = nbytes;
    monolith->data = TOL_MALLOC(uint8_t, nbytes);
    memcpy(monolith->data, src, nbytes);
    monolith->nclades = 0;
    for (long i = 0; i < nbytes; i++) {
        if (monolith->data[i] == '\n') {
            monolith->nclades++;
            monolith->data[i] = 0;
        }
    }
    monolith->parents = TOL_MALLOC(int32_t, monolith->nclades);
    monolith->ids = TOL_MALLOC(int32_t, monolith->nclades);
    monolith->labels = TOL_MALLOC(char const*, monolith->nclades);
    monolith->maxid = 0;
    uint8_t* data = monolith->data;
    for (int32_t j = 0; j < monolith->nclades; j++) {
        data[6] = data[13] = 0;
        uint32_t id = strtol((const char*) data, 0, 16);
        uint32_t parent = strtol((const char*) data + 7, 0, 16);
        monolith->ids[j] = id;
        monolith->parents[j] = parent;
        monolith->maxid = TOL_MAX(monolith->maxid, id);
        monolith->maxid = TOL_MAX(monolith->maxid, parent);
        monolith->labels[j] = (char const*) (data + 14);
        data += strlen(monolith->labels[j]) + 15;
    }
    return monolith;
}

void tol_free_monolith(tol_monolith_t* monolith)
{
    if (monolith) {
        TOL_FREE(monolith->data);
        TOL_FREE(monolith->parents);
        TOL_FREE(monolith->ids);
        TOL_FREE(monolith->labels);
        TOL_FREE(monolith);
    }
}

tol_monolith_t* tol_monolith_pack(tol_monolith_t const* src)
{
    tol_monolith_t* dst = TOL_CALLOC(tol_monolith_t, 1);
    dst->nclades = src->nclades;
    dst->parents = TOL_MALLOC(int32_t, dst->nclades);
    dst->ids = TOL_MALLOC(int32_t, dst->nclades);
    dst->maxid = src->maxid;
    int32_t* mapping = TOL_MALLOC(int32_t, dst->maxid + 1);
    for (int32_t j = 0; j < dst->nclades; j++) {
        mapping[src->ids[j]] = j;
    }
    for (int32_t j = 0; j < dst->nclades; j++) {
        dst->ids[j] = mapping[src->ids[j]];
        dst->parents[j] = mapping[src->parents[j]];
    }
    TOL_FREE(mapping);
    return dst;
}

void tol_monolith_merge(tol_monolith_t* dst, tol_monolith_t const* src)
{
    int32_t nclades = dst->nclades + src->nclades;
    int32_t buflen = dst->buflen + src->buflen;
    dst->parents = TOL_REALLOC(int32_t, dst->parents, nclades);
    dst->ids = TOL_REALLOC(int32_t, dst->ids, nclades);
    dst->labels = TOL_REALLOC(char const*, dst->labels, nclades);
    uint8_t* data = TOL_MALLOC(uint8_t, buflen);
    for (int32_t i = 0; i < dst->nclades; i++) {
        intptr_t offset = (intptr_t) dst->labels[i] - (intptr_t) dst->data;
        dst->labels[i] = (char const*) (data + offset);
    }
    for (int32_t i = dst->nclades, j = 0; i < nclades; i++, j++) {
        dst->parents[i] = src->parents[j];
        dst->ids[i] = src->ids[j];
        intptr_t offset = (intptr_t) src->labels[j] - (intptr_t) src->data;
        dst->labels[i] = (char const*) (data + dst->buflen + offset);
    }
    memcpy(data, dst->data, dst->buflen);
    memcpy(data + dst->buflen, src->data, src->buflen);
    free(dst->data);
    dst->buflen = buflen;
    dst->data = data;
    dst->nclades = nclades;
    dst->maxid = TOL_MAX(dst->maxid, src->maxid);
}
