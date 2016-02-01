
// @program p_disks, disk.vertex, disk.fragment
// @program p_lines, line.vertex, line.fragment

#extension GL_OES_standard_derivatives : enable

uniform mat4 u_mvp;
uniform float u_sel;
uniform vec3 u_colors[32];
varying float v_rim;
varying vec3 v_fill;
varying vec3 v_background;
const float STROKEW = 0.99;
const float STROKEB = 0.70;

-- disk.vertex

attribute vec3 a_position;
attribute vec4 a_center;
attribute float a_depth;

void main()
{
    vec3 cen = a_center.xyz;
    vec3 pos = vec3(a_position.xy * cen.z + cen.xy, 0.0);
    int depth = int(mod(a_depth, 32.0));
    v_background = u_colors[depth > 0 ? depth - 1 : 0];
    v_fill = u_colors[depth];
    if (a_center.w == u_sel) {
        v_fill.rb = v_fill.rb * 0.8;
    }
    v_rim = a_position.z;
    pos.z = -1.0;
    gl_Position = u_mvp * vec4(pos, 1.0);
}

-- disk.fragment

void main()
{
    float fw = fwidth(v_rim);
    float e = smoothstep(STROKEW - 2.0 * fw, STROKEW, v_rim);
    vec3 s = mix(v_fill, v_background * STROKEB, e);
    e = smoothstep(1.0 - fw, 1.0, v_rim);
    s = mix(s, v_background, e);
    gl_FragColor = vec4(s, 1.0 - e);
}

-- line.vertex

attribute vec2 a_position;

void main()
{
    vec3 pos = vec3(a_position, -1.0);
    gl_Position = u_mvp * vec4(pos, 1.0);
}

-- line.fragment

void main()
{
    gl_FragColor = vec4(1.0, 1.0, 1.0, 0.3);
}
