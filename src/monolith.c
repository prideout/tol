#include <tol.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <parwin.h>

tol_monolith_t* tol_load_monolith(parg_token token)
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
    monolith->labels = TOL_MALLOC(char const*, monolith->nclades);
    uint8_t* data = monolith->data;
    for (long j = 0; j < monolith->nclades; j++) {
        data[6] = data[13] = 0;
        monolith->parents[j] = strtol((const char*) data + 7, 0, 16);
        data += 14;
        monolith->labels[j] = (char const*) data;
        data += strlen(monolith->labels[j]) + 1;
    }
    parg_window_send_bytes("set_labels", monolith->data, fsize);
    return monolith;
}

void tol_free_monolith(tol_monolith_t* monolith)
{
    if (monolith) {
        parg_buffer_free(monolith->buffer);
        TOL_FREE(monolith->parents);
        TOL_FREE(monolith->labels);
        TOL_FREE(monolith);
    }
}
