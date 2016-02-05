#include <camera_rig.h>
#include <pa.h>
#include <parg.h>

static struct {
    bool active;
    double start_time;
    double initial_viewport[4];   // left-bottom-right-top
    double final_viewport[4];     // left-bottom-right-top
    int32_t* root_sequence;       // pliable array of bubble indices
    int32_t current_root_target;  // index into the sequence
    int32_t target_node;
    par_bubbles_t* bubbles;
} camrig = {0};

void camera_rig_init(par_bubbles_t* bubbles)
{
    camrig.bubbles = bubbles;
}

void camera_rig_free()
{
    pa_free(camrig.root_sequence);
}

void camera_rig_tick(double current_time, int32_t root)
{
    if (!camrig.active) {
        return;
    }
    const double durationPerStep = 0.5;
    double elapsed = current_time - camrig.start_time;
    int32_t const* seq = camrig.root_sequence;
    int32_t nseq = pa_count(seq);

    // Check if ready to move to the next phase, or terminate the animation.
    if (elapsed >= durationPerStep) {
        if (++camrig.current_root_target >= nseq) {
            double const* dst_lbrt = camrig.final_viewport;
            double dst_xyw[3] = {
                0.5 * (dst_lbrt[0] + dst_lbrt[2]),
                0.5 * (dst_lbrt[1] + dst_lbrt[3]),
                dst_lbrt[2] - dst_lbrt[0]
            };
            double xform[3];
            par_bubbles_transform_local(camrig.bubbles, xform, seq[nseq - 1],
                root);
            dst_xyw[0] = dst_xyw[0] * xform[2] + xform[0];
            dst_xyw[1] = dst_xyw[1] * xform[2] + xform[1];
            dst_xyw[2] = dst_xyw[2] * xform[2];
            parg_zcam_set_viewport(dst_xyw);
            camrig.active = false;
            return;
        }
        camrig.start_time = current_time;
        elapsed = 0;
    }

    // Compute the position of the crosshairs in the current coordsys.
    double crosshairs[3];
    int32_t anim_root = seq[camrig.current_root_target];
    par_bubbles_transform_local(camrig.bubbles, crosshairs,
        camrig.target_node, anim_root);

    // Find the "source viewport" in the coordsys of current_root_target.
    double src_lbrt[4];
    if (camrig.current_root_target == 0) {
        src_lbrt[0] = camrig.initial_viewport[0];
        src_lbrt[1] = camrig.initial_viewport[1];
        src_lbrt[2] = camrig.initial_viewport[2];
        src_lbrt[3] = camrig.initial_viewport[3];
    } else {
        double xform[3];
        par_bubbles_transform_local(camrig.bubbles,
            xform, seq[camrig.current_root_target - 1], anim_root);
        if (camrig.target_node == 0) {
            crosshairs[0] = xform[0];
            crosshairs[1] = xform[1];
        }
        src_lbrt[0] = -xform[2] + crosshairs[0];
        src_lbrt[1] = -xform[2] + crosshairs[1];
        src_lbrt[2] = xform[2] + crosshairs[0];
        src_lbrt[3] = xform[2] + crosshairs[1];
    }

    // The "destination viewport" is centered on the crosshair unless
    // we're in the last phase of animation, or if we're zooming out.
    double dst_xyw[3] = { crosshairs[0], crosshairs[1], 2 };
    if (camrig.target_node == 0) {
        dst_xyw[0] = dst_xyw[1] = 0;
    }
    if (camrig.current_root_target == nseq - 1) {
        double const* dst_lbrt = camrig.final_viewport;
        dst_xyw[0] = 0.5 * (dst_lbrt[0] + dst_lbrt[2]);
        dst_xyw[1] = 0.5 * (dst_lbrt[1] + dst_lbrt[3]);
        dst_xyw[2] = dst_lbrt[2] - dst_lbrt[0];
    }

    // Use Wijk interpolation to compute the desired viewport for this frame.
    double desired_xyw[3];
    double src_xyw[3] = {
        0.5 * (src_lbrt[0] + src_lbrt[2]),
        0.5 * (src_lbrt[1] + src_lbrt[3]),
        src_lbrt[2] - src_lbrt[0]
    };
    parg_zcam_blend(src_xyw, dst_xyw, desired_xyw, elapsed / durationPerStep);

    // Transform the desired viewport from the coordsys of curr_root_target
    // to the coordsys of the current app root.
    double xform[3];
    par_bubbles_transform_local(camrig.bubbles, xform, anim_root, root);
    desired_xyw[0] = desired_xyw[0] * xform[2] + xform[0];
    desired_xyw[1] = desired_xyw[1] * xform[2] + xform[1];
    desired_xyw[2] = desired_xyw[2] * xform[2];

    // Finally, set the viewport.  We'll allow the draw function re-adjust the
    // app root if necessary.
    parg_zcam_set_viewport(desired_xyw);
}

void camera_rig_zoom(double current_time, int32_t root, int32_t target,
    bool distant)
{
    if (camrig.active) {
        return;
    }

    // Initialize all members of the animation structure except the sequence.
    camrig.active = true;
    camrig.start_time = current_time;
    camrig.current_root_target = 0;
    camrig.target_node = target;
    parg_zcam_get_viewport(camrig.initial_viewport);

    // The zoom destination is a viewport centered at the target node, where the
    // viewport width is 2.5x the radius of the target node. The root node for
    // this viewport is called the "target root", and it's an ancestor of the
    // target node.
    double aabb[4] = {-1.25, -1.25, 1.25, 1.25};
    int32_t target_root = par_bubbles_find_local(camrig.bubbles, aabb, target);
    target_root = PAR_MAX(0, target_root);
    int32_t lca = par_bubbles_lowest_common_ancestor(camrig.bubbles, root,
        target_root);
    double xform[3];
    par_bubbles_transform_local(camrig.bubbles, xform, target, target_root);
    camrig.final_viewport[0] = aabb[0] * xform[2] + xform[0];
    camrig.final_viewport[1] = aabb[1] * xform[2] + xform[1];
    camrig.final_viewport[2] = aabb[2] * xform[2] + xform[0];
    camrig.final_viewport[3] = aabb[3] * xform[2] + xform[1];

    // Finally, build the root sequence.
    pa_clear(camrig.root_sequence);
    if (!distant) {
        par_bubbles_transform_local(camrig.bubbles, xform, target, root);
        pa_push(camrig.root_sequence, root);
        camrig.final_viewport[0] = aabb[0] * xform[2] + xform[0];
        camrig.final_viewport[1] = aabb[1] * xform[2] + xform[1];
        camrig.final_viewport[2] = aabb[2] * xform[2] + xform[0];
        camrig.final_viewport[3] = aabb[3] * xform[2] + xform[1];
        return;
    }
    int32_t node = root;
    while (true) {
        pa_push(camrig.root_sequence, node);
        if (node == lca) {
            break;
        }
        node = par_bubbles_get_parent(camrig.bubbles, node);
    }
    node = target_root;
    while (true) {
        if (node == lca) {
            break;
        }
        pa_push(camrig.root_sequence, -1);
        node = par_bubbles_get_parent(camrig.bubbles, node);
    }
    int nsteps = pa_count(camrig.root_sequence) - 1;
    node = target_root;
    while (true) {
        if (node == lca) {
            break;
        }
        camrig.root_sequence[nsteps--] = node;
        node = par_bubbles_get_parent(camrig.bubbles, node);
    }

    // By design, the last node appears twice.
    int32_t last = pa_count(camrig.root_sequence) - 1;
    pa_push(camrig.root_sequence,
        camrig.root_sequence[last]);
}
