#include <parg.h>
#include "pargl.h"

int _parg_depthtest = 0;

void parg_state_clearcolor(Vector4 color)
{
    glClearColor(color.x, color.y, color.z, color.w);
}

void parg_state_cullfaces(int enabled)
{
    (enabled ? glEnable : glDisable)(GL_CULL_FACE);
}

void parg_state_depthtest(int enabled)
{
    (enabled ? glEnable : glDisable)(GL_DEPTH_TEST);
    _parg_depthtest = enabled;
}

void parg_state_blending(int enabled)
{
    if (enabled == 1) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else if (enabled == 2) {
        glBlendFunc(GL_ONE, GL_ONE);
    }
    (enabled ? glEnable : glDisable)(GL_BLEND);
}
