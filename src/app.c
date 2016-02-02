#define PAR_BUBBLES_IMPLEMENTATION
#define PAR_COLOR_IMPLEMENTATION

#include <tol.h>
#include <parg.h>
#include <parwin.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <par_bubbles.h>
#include <par_shapes.h>
#include <par_color.h>

#define TOKEN_TABLE(F)          \
    F(P_DISKS, "p_disks")       \
    F(P_LINES, "p_lines")       \
    F(A_POSITION, "a_position") \
    F(A_CENTER, "a_center")     \
    F(A_DEPTH, "a_depth")       \
    F(U_MVP, "u_mvp")           \
    F(U_SEL, "u_sel");          \
    F(U_CAMZ, "u_camz");

TOKEN_TABLE(PARG_TOKEN_DECLARE);

#define ASSET_TABLE(F) F(SHADER_SIMPLE, "app.glsl")
ASSET_TABLE(PARG_TOKEN_DECLARE);

const float FOVY = 32 * PARG_TWOPI / 180;
const float WORLDWIDTH = 3;

struct {
    int32_t nnodes;
    par_bubbles_t* bubbles;
    par_bubbles_t* culled;
    tol_monolith_t* monolith;
    parg_mesh* disk_mesh;
    parg_buffer* crosshairs_buffer;
    par_shapes_mesh* disk_unit;
    par_shapes_mesh* disk_shape;
    parg_buffer* instances;
    int32_t hover;
    int32_t potentially_clicking;
    double current_time;
    double winwidth;
    int32_t* tree;
    int32_t leaf;
    int32_t maxdepth;
    int32_t root;
    double crosshairs[2];
    double minradius;
} app = {0};

void cleanup()
{
    par_bubbles_free_result(app.bubbles);
    par_bubbles_free_result(app.culled);
    free(app.tree);
}

static void set_crosshairs(float x, float y)
{
    float* plines = parg_buffer_lock(app.crosshairs_buffer, PARG_WRITE);
    *plines++ = x;
    *plines++ = -1;
    *plines++ = x;
    *plines++ = 1;
    *plines++ = -1;
    *plines++ = y;
    *plines++ = 1;
    *plines++ = y;
    parg_buffer_unlock(app.crosshairs_buffer);
}

void generate(int32_t nnodes)
{
    if (nnodes == 0) {
        // Load the Tree of Life from a monolithic file if we haven't already.
        if (!app.monolith) {
            app.monolith = tol_load_monolith("monolith.0000.txt");
        }
        setlocale(LC_ALL, "");
        printf("Loaded %'d clades.\n", app.monolith->nclades);
        nnodes = app.monolith->nclades;
        app.tree = malloc(sizeof(int32_t) * nnodes);
        for (int32_t i = 0; i < nnodes; i++) {
            app.tree[i] = app.monolith->parents[i];
        }
    } else {
        // Generate a random tree.  Square the random parent pointers to make
        // the graph distribution a bit more interesting, and to make it easier
        // for humans to find deep portions of the tree.
        printf("Generating tree with %d nodes...\n", nnodes);
        app.tree = malloc(sizeof(int32_t) * nnodes);
        app.tree[0] = 0;
        for (int32_t i = 1; i < nnodes; i++) {
            float a = (float) rand() / RAND_MAX;
            float b = (float) rand() / RAND_MAX;
            app.tree[i] = i * a * b;
        }
    }
    app.nnodes = nnodes;

    // Perform circle packing.
    puts("Packing circles...");
    app.bubbles = par_bubbles_hpack_local(app.tree, nnodes);
    app.hover = -1;

    // Compute crosshairs position by finding the deepest leaf.
    par_bubbles_get_maxdepth(app.bubbles, &app.maxdepth, &app.leaf);
    printf("Node %d has depth %d\n", app.leaf, app.maxdepth);
    double xform[2];
    par_bubbles_transform_local(app.bubbles, xform, app.leaf, app.root);
    app.crosshairs[0] = xform[0];
    app.crosshairs[1] = xform[1];
    parg_zcam_touch();

    // Initialize the uniform array.
    parg_shader_bind(P_DISKS);
    const float a[3] = {170, 0.05, 0.05};
    const float b[3] = {100, 0.1, 0.2};
    const float freq = app.maxdepth / 2;
    float colors[32 * 3];
    for (int i = 0; i < 32; i++) {
        float* fresult = colors + i * 3;
        float t = i / 31.0f;
        t = 0.5 + 0.5 * sin(freq * t * PAR_PI / 2.0);
        par_color_mix_hcl(a, b, fresult, t);
        par_color_hcl_to_rgb(fresult, fresult);
    }
    parg_uniform3fv("u_colors[0]", 32, colors);
    float* bkgd = colors + 1 * 3;
    parg_state_clearcolor((Vector4){bkgd[0], bkgd[1], bkgd[2], 1.0});
}

void init(float winwidth, float winheight, float pixratio)
{
    parg_state_depthtest(0);
    parg_state_cullfaces(1);
    parg_state_blending(0);
    parg_shader_load_from_asset(SHADER_SIMPLE);
    parg_zcam_init(WORLDWIDTH, WORLDWIDTH, FOVY);

    // Create a single-precision buffer of vec2's for the lines.
    int vstride = sizeof(float) * 2;
    app.crosshairs_buffer = parg_buffer_alloc(vstride * 4, PARG_GPU_ARRAY);

    // Create the initial bubble diagram.
    generate(2e4);

    // Create disk_unit shape.
    float normal[3] = {0, 0, 1};
    float center[3] = {0, 0, 1};
    app.disk_unit = par_shapes_create_disk(1.0, 64, center, normal);
    app.disk_unit->points[2] = 0;
    app.disk_shape = par_shapes_create_disk(1.0, 64, center, normal);
    app.disk_shape->points[2] = 0;

    // Create the vertex buffer for the disk_unit shape.
    app.disk_mesh = parg_mesh_from_shape(app.disk_unit);

    // Create the vertex buffer with instance-varying data.  We re-populate it
    // on every frame, growing it if necessary.  The starting size doesn't
    // matter much.
    app.instances = parg_buffer_alloc(512 * 5 * sizeof(float), PARG_GPU_ARRAY);
}

void draw()
{
    // Check if the "relative root" should be changed.
    double aabb[4];
    parg_zcam_get_viewport(aabb);
    int new_root = par_bubbles_find_local(app.bubbles, aabb, app.root);
    if (app.root == 0 && new_root == -1) {
        new_root = 0;
    }
    if (new_root == -1) {
        new_root = app.tree[app.root];
    }

    // If the relative root should be changed, then re-adjust the camera, etc.
    if (app.root != new_root) {
        double xform[3];
        par_bubbles_transform_local(app.bubbles, xform, app.root, new_root);
        app.root = new_root;
        double xyw[3] = {
            0.5 * (aabb[0] + aabb[2]),
            0.5 * (aabb[1] + aabb[3]),
            aabb[2] - aabb[0]
        };
        xyw[0] = xyw[0] * xform[2] + xform[0];
        xyw[1] = xyw[1] * xform[2] + xform[1];
        xyw[2] = xyw[2] * xform[2];
        parg_zcam_set_viewport(xyw);

        // Recompute crosshairs position by transforming the leaf position.
        par_bubbles_transform_local(app.bubbles, xform, app.leaf, app.root);
        app.crosshairs[0] = xform[0];
        app.crosshairs[1] = xform[1];
        parg_zcam_get_viewport(aabb);
    }

    // Obtain the camera position.
    Matrix4 vp;
    DPoint3 camera = parg_zcam_get_camera(&vp);
    parg_draw_clear();
    parg_shader_bind(P_DISKS);
    parg_uniform_matrix4f(U_MVP, &vp);
    parg_uniform1f(U_SEL, app.hover);
    parg_uniform1f(U_CAMZ, camera.z);

    // Bake the view transform into the disk VBO.
    float* dst = app.disk_shape->points;
    float const* src = app.disk_unit->points;
    for (int i = 0; i < app.disk_unit->npoints; i++) {
        dst[i * 3] = src[i * 3] / camera.z;
        dst[i * 3 + 1] = src[i * 3 + 1] / camera.z;
    }
    parg_mesh_update_from_shape(app.disk_mesh, app.disk_shape);

    // Bind the index buffer and verts for the circle shape at the origin.
    parg_varray_bind(parg_mesh_index(app.disk_mesh));
    parg_varray_enable(
        parg_mesh_coord(app.disk_mesh), A_POSITION, 3, PARG_FLOAT, 0, 0);

    // Bind the vertex buffer that contains all once-per-instance attributes.
    int stride = 5 * sizeof(float), offset = 4 * sizeof(float);
    parg_varray_instances(A_CENTER, 1);
    parg_varray_enable(app.instances, A_CENTER, 4, PARG_FLOAT, stride, 0);
    parg_varray_instances(A_DEPTH, 1);
    parg_varray_enable(app.instances, A_DEPTH, 1, PARG_FLOAT, stride, offset);

    // Perform frustum culling and min-size culling.
    app.minradius = (aabb[2] - aabb[0]) / app.winwidth;
    app.culled = par_bubbles_cull_local(app.bubbles, aabb, app.minradius,
        app.root, app.culled);

    // Next, re-populate all per-instance vertex buffer data.
    // This bakes the pan offset into the geometry because it allows
    // adding a double-precision number to a double-precision number.
    int32_t nbytes = app.culled->count * 5 * sizeof(float);
    float* fdisk = parg_buffer_lock_grow(app.instances, nbytes);
    double const* ddisk = app.culled->xyr;
    for (int32_t i = 0; i < app.culled->count; i++, fdisk += 5, ddisk += 3) {
        int32_t id = app.culled->ids[i];
        fdisk[0] = (ddisk[0] - camera.x) / camera.z;
        fdisk[1] = (ddisk[1] - camera.y) / camera.z;
        fdisk[2] = ddisk[2];
        fdisk[3] = id;
        fdisk[4] = par_bubbles_get_depth(app.bubbles, id);
    }
    parg_buffer_unlock(app.instances);

    // Draw all triangles in one fell swoop.
    parg_draw_instanced_triangles_u16(
        0, parg_mesh_ntriangles(app.disk_mesh), app.culled->count);

    // Draw crosshairs.
    double x = (app.crosshairs[0] - camera.x) / camera.z;
    double y = (app.crosshairs[1] - camera.y) / camera.z;
    set_crosshairs(x, y);
    parg_varray_disable(A_CENTER);
    parg_varray_disable(A_DEPTH);
    parg_shader_bind(P_LINES);
    parg_varray_enable(app.crosshairs_buffer, A_POSITION, 2, PARG_FLOAT, 0, 0);
    parg_uniform_matrix4f(U_MVP, &vp);
    parg_state_blending(1);
    parg_draw_lines(2);
    parg_state_blending(0);
}

int tick(float winwidth, float winheight, float pixratio, float seconds)
{
    app.current_time = seconds;
    app.winwidth = winwidth;
    parg_zcam_set_aspect(winwidth / winheight);
    return parg_zcam_has_moved();
}

void dispose()
{
    tol_free_monolith(app.monolith);
    parg_shader_free(P_DISKS);
    parg_shader_free(P_LINES);
    parg_mesh_free(app.disk_mesh);
    parg_buffer_free(app.instances);
    parg_buffer_free(app.crosshairs_buffer);
    par_shapes_free_mesh(app.disk_unit);
    par_shapes_free_mesh(app.disk_shape);
    cleanup();
}

static void zoom_to_node(int32_t i)
{
    printf("Zooming to depth %d.\n", par_bubbles_get_depth(app.bubbles, i));
    app.root = i;
    double xyw[] = {0, 0, 2.5};
    parg_zcam_set_viewport(xyw);
}

void message(const char* msg)
{
    if (!strcmp(msg, "0")) {
        generate(0);
    } else if (!strcmp(msg, "20K")) {
        generate(2e4);
    } else if (!strcmp(msg, "200K")) {
        generate(2e5);
    } else if (!strcmp(msg, "2M")) {
        generate(2e6);
    } else if (!strcmp(msg, "L")) {
        zoom_to_node(app.leaf);
    } else if (!strcmp(msg, "H")) {
        zoom_to_node(0);
    }
}

void input(parg_event evt, float x, float y, float z)
{
    DPoint3 p = parg_zcam_to_world(x, y);
    int key = (char) x;
    switch (evt) {
    case PARG_EVENT_KEYPRESS:
        if (key == '0') {
            message("0");
        } else if (key == '1') {
            message("20K");
        } else if (key == '2') {
            message("200K");
        } else if (key == '3') {
            message("2M");
        } else if (key == 'L') {
            message("L");
        } else if (key == 'H') {
            message("H");
        }
        break;
    case PARG_EVENT_DOWN:
        app.potentially_clicking = 1;
        parg_zcam_grab_begin(x, y);
        break;
    case PARG_EVENT_UP:
        parg_zcam_grab_update(x, y, z);
        parg_zcam_grab_end();
        if (app.potentially_clicking == 1) {
            int32_t i = par_bubbles_pick_local(app.bubbles, p.x, p.y, app.root,
                app.minradius);
            if (i > -1) {
                zoom_to_node(i);
            }
        }
        app.potentially_clicking = 0;
        break;
    case PARG_EVENT_MOVE: {
        app.potentially_clicking = 0;
        int32_t picked = par_bubbles_pick_local(app.bubbles, p.x, p.y, app.root,
            app.minradius);
        if (picked != app.hover) {
            parg_zcam_touch();
            app.hover = picked;
        }
        parg_zcam_grab_update(x, y, z);
        break;
    }
    default:
        break;
    }
}

int main(int argc, char* argv[])
{
    puts("Press 1,2,3 to regenerate 20K, 200K or 2M nodes.");
    puts("Press L to zoom to one of the deepest leaf nodes.");
    puts("Press H to return to the home view.");
    srand(1);
    TOKEN_TABLE(PARG_TOKEN_DEFINE);
    ASSET_TABLE(PARG_ASSET_TABLE);
    parg_window_setargs(argc, argv);
    parg_window_oninit(init);
    parg_window_ontick(tick);
    parg_window_ondraw(draw);
    parg_window_onexit(dispose);
    parg_window_oninput(input);
    parg_window_onmessage(message);
    return parg_window_exec(600, 600, 1, 0);
}
