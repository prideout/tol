
// @program p_simple, vertex, fragment

#extension GL_OES_standard_derivatives : enable

uniform mat4 u_mvp;
uniform float u_sel;
uniform vec3 u_eyepos;
uniform vec3 u_eyepos_lowpart;
uniform vec3 u_colors[32];
varying float v_rim;
varying vec3 v_fill;
const float STROKEW = 0.98;
const vec3 STROKEC = vec3(0);

-- vertex

attribute vec3 a_position;
attribute vec4 a_center;
attribute float a_depth;

void main()
{
    vec3 cen = a_center.xyz;
    vec3 pos = vec3(a_position.xy * cen.z + cen.xy, 0.0);
    v_fill = u_colors[int(a_depth * 32.0)];
    v_fill *= (a_center.w == u_sel) ? 1.0 : 1.25;
    v_rim = a_position.z;

    #ifdef SINGLE_PRECISION
        pos -= u_eyepos;
    #else
        vec3 poslow = vec3(0);
        vec3 t1 = poslow - u_eyepos_lowpart;
        vec3 e = t1 - poslow;
        vec3 t2 = ((-u_eyepos_lowpart - e) + (poslow - (t1 - e))) + pos - u_eyepos;
        vec3 high_delta = t1 + t2;
        vec3 low_delta = t2 - (high_delta - t1);
        pos = high_delta + low_delta;
    #endif

    gl_Position = u_mvp * vec4(pos, 1.0);
}

-- fragment

void main()
{
    float fw = fwidth(v_rim);
    float e = smoothstep(STROKEW - fw, STROKEW + fw, v_rim);
    gl_FragColor = vec4(mix(v_fill, STROKEC, e), 1.0);
}
