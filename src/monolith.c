#include <tol.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <parwin.h>

tol_monolith_t* tol_monolith_load(parg_token token)
{
    tol_monolith_t* monolith = TOL_CALLOC(tol_monolith_t, 1);
    monolith->buffer = parg_buffer_from_asset(token);
    long fsize = parg_buffer_length(monolith->buffer);
    assert(fsize > 1 && "Unable to load monolith buffer.");
    monolith->data = parg_buffer_lock(monolith->buffer, PARG_READ);
    monolith->nclades = 0;
    for (long i = 0; i < fsize; i++) {
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
        monolith->maxid = PARG_MAX(monolith->maxid, id);
        monolith->maxid = PARG_MAX(monolith->maxid, parent);
        monolith->labels[j] = (char const*) (data + 14);
        data += strlen(monolith->labels[j]) + 15;
    }
    parg_window_send_bytes("set_labels", monolith->data, fsize);
    return monolith;
}

void tol_monolith_free(tol_monolith_t* monolith)
{
    if (monolith) {
        parg_buffer_free(monolith->buffer);
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
    // TODO
}
