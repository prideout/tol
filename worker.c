#include "tol.h"

#define PAR_SPRUNE_IMPLEMENTATION
#include "par/par_sprune.h"

#define PAR_BUBBLES_FLT float
#define PAR_BUBBLES_IMPLEMENTATION
#include "par/par_bubbles.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <locale.h>

struct {
    float* original_boxes;
    float* transformed_boxes;
    int nboxes;
    par_sprune_context* context;
    float viewport[4];
    float winsize[2];

    int32_t* tree;
    int32_t nnodes;
    par_bubbles_t* bubbles;
    par_bubbles_t* culled;

} app = {0};

void d3cpp_set_winsize(float const* data, int nbytes)
{
    assert(nbytes == 8);
    app.viewport[2] = app.winsize[0] = data[0];
    app.viewport[3] = app.winsize[1] = data[1];
}

void d3cpp_set_data(uint8_t const* data, int nbytes)
{
    if (app.original_boxes == 0) {
        app.original_boxes = malloc(nbytes);
        app.nboxes = nbytes / 16;
    }
    assert(app.nboxes == nbytes / 16);
    memcpy(app.original_boxes, data, nbytes);
}

static void do_collisions()
{
    if (app.transformed_boxes == 0) {
        app.transformed_boxes = malloc(app.nboxes * 16);
    }

    float sx = app.winsize[0] / (app.viewport[2] - app.viewport[0]);
    float sy = app.winsize[1] / (app.viewport[3] - app.viewport[1]);

    float const* src = (float const*) app.original_boxes;
    float* dst = (float*) app.transformed_boxes;
    for (int i = 0; i < app.nboxes * 4;) {
        float x0 = *src++;
        float y0 = *src++;
        float x1 = *src++;
        float y1 = *src++;
        float cx = sx * 0.5 * (x0 + x1);
        float cy = sy * 0.5 * (y0 + y1);
        float w = 0.5 * (x1 - x0);
        float h = 0.5 * (y1 - y0);
        dst[i++] = cx - w;
        dst[i++] = cy - h;
        dst[i++] = cx + w;
        dst[i++] = cy + h;
    }

    if (!app.context) {
        app.context = par_sprune_overlap(dst, app.nboxes, app.context);
    } else {
        par_sprune_update(app.context);
    }
    par_sprune_cull(app.context);

    uint8_t const* collisions = (uint8_t const*) app.context->collision_pairs;
    int ncollisions = app.context->ncollision_pairs;
    uint8_t const* culled = (uint8_t const*) app.context->culled;
    int nculled = app.context->nculled;

    EM_ASM_INT({
        postMessage({
            event: "collisions",
            collisions: new Uint8Array(HEAPU8.subarray($0, $1)),
            culled: new Uint8Array(HEAPU8.subarray($2, $3)),
        });
    }, collisions, collisions + ncollisions * 8, culled, culled + nculled * 4);
}

static void do_culling()
{
    float aabb[4];
    float w2 = 0.5f * (app.viewport[2] - app.viewport[0]);
    float h2 = 0.5f * (app.viewport[3] - app.viewport[1]);
    aabb[0] = -1;//app.viewport[0] - w2;
    aabb[1] = -1;//app.viewport[1] - h2;
    aabb[2] = 1;//app.viewport[2] - w2;
    aabb[3] = 1;//app.viewport[3] - h2;

    // Cull bubbles to viewport.
    float minradius = 0.01;
    int32_t root = 0;
    app.culled = par_bubbles_cull_local(app.bubbles, aabb, minradius,
        root, app.culled);

    // Send culled circles over the wire.
    uint8_t const* xyr_data = (uint8_t const*) app.culled->xyr;
    int nfloats = app.culled->count * 3;
    EM_ASM_INT({
        postMessage({
            event: "bubbles",
            bubbles: new Uint8Array(HEAPU8.subarray($0, $1))
        });
    }, xyr_data, xyr_data + nfloats * 4);
}

void d3cpp_set_viewport(float const* aabb, int nbytes)
{
    assert(nbytes == 16);
    app.viewport[0] = aabb[0];
    app.viewport[1] = aabb[1];
    app.viewport[2] = aabb[2];
    app.viewport[3] = aabb[3];
    #ifdef VERBOSE
    printf("%.2f %.2f %.2f %.2f\n", aabb[0], aabb[1], aabb[2], aabb[2]);
    #endif

    do_collisions();
    if (app.bubbles) {
        do_culling();
    }
}

void d3cpp_set_monolith(uint8_t const* data, int nbytes)
{
    // Parse the monolith and pack it so that the ids are not sparse.
    tol_monolith_t* monolith = tol_load_monolith(data, nbytes);
    setlocale(LC_ALL, "");
    printf("Loaded %'d clades.\n", monolith->nclades);
    int32_t nnodes = monolith->nclades;
    bool* parents = calloc(nnodes, sizeof(int32_t));
    tol_monolith_t* packed = tol_monolith_pack(monolith);
    tol_free_monolith(monolith);

    // Describe a tree by generating a list of integers.
    app.tree = malloc(sizeof(int32_t) * nnodes);
    int32_t nparents = 0;
    for (int32_t i = 0; i < nnodes; i++) {
        int parent = packed->parents[i];
        app.tree[i] = parent;
        if (!parents[parent]) {
            parents[parent] = true;
            nparents++;
        }
    }
    tol_free_monolith(packed);

    // Add an additional child to every non-leaf node.  This is used
    // to make space for a secondary label, and to prevent singly-nested
    // nodes from ever occuring.
    int nnewnodes = nnodes + nparents;
    app.tree = realloc(app.tree, sizeof(int32_t) * nnewnodes);
    int noldnodes = nnodes;
    for (int32_t i = 0; i < noldnodes; i++) {
        if (parents[i]) {
            app.tree[nnodes++] = i;
        }
    }
    free(parents);
    app.nnodes = nnodes;

    // Preferring vertical layout for 2-child families makes it less likely
    // for children labels to collide with one another.
    par_bubbles_set_orientation(PAR_BUBBLES_VERTICAL);

    // Perform circle packing.
    puts("Packing circles...");
    app.bubbles = par_bubbles_hpack_local(app.tree, nnodes);
    par_bubbles_set_filter(app.bubbles, PAR_BUBBLES_FILTER_DISCARD_LAST_CHILD);
    do_culling();
}
