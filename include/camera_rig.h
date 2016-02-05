#ifndef TOL_CAMERA_RIG_H
#define TOL_CAMERA_RIG_H

#include <par_bubbles.h>

void camera_rig_init(par_bubbles_t* bubbles);

void camera_rig_tick(double current_time, int32_t root);

void camera_rig_zoom(double current_time, int32_t root, int32_t target,
    bool distant);

void camera_rig_free();

#endif // TOL_CAMERA_RIG_H
