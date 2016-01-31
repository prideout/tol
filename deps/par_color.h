// COLOR :: https://github.com/prideout/par
// Simple C library for converting between RGB, LAB, and HCL color spaces.
//
// By design, this library has absolutely nothing to do with semitransparency.
// Alpha is a compositing parameter, not a component of human color perception.
//
// The MIT License
// Copyright (c) 2015 Philip Rideout

#ifndef PAR_COLOR_H
#define PAR_COLOR_H

#ifdef __cplusplus
extern "C" {
#endif

void par_color_rgb_to_hex(float const* src, char* result, int nchars);
void par_color_hex_to_rgb(char const* src, float* result);
void par_color_rgb_to_rgb8(float const* src, uint8_t* result);
void par_color_rgb8_to_rgb(uint8_t const* src, float* result);
void par_color_mix_hcl(float const* a, float const* b, float* result, float t);
void par_color_mix_rgb(float const* a, float const* b, float* result, float t);
void par_color_mix_lab(float const* a, float const* b, float* result, float t);
void par_color_hcl_to_lab(float const* src, float* result);
void par_color_lab_to_hcl(float const* src, float* result);
void par_color_hcl_to_rgb(float const* src, float* result);
void par_color_rgb_to_hcl(float const* src, float* result);
void par_color_lab_to_rgb(float const* src, float* result);
void par_color_rgb_to_lab(float const* src, float* result);

#ifndef PAR_PI
#define PAR_PI (3.14159265359)
#define PAR_MIN(a, b) (a > b ? b : a)
#define PAR_MAX(a, b) (a > b ? a : b)
#define PAR_CLAMP(v, lo, hi) PAR_MAX(lo, PAR_MIN(hi, v))
#define PAR_SWAP(T, A, B) { T tmp = B; B = A; A = tmp; }
#define PAR_SQR(a) ((a) * (a))
#endif

#ifdef __cplusplus
}
#endif
#endif // PAR_COLOR_H

#ifdef PAR_COLOR_IMPLEMENTATION

#define PAR_COLOR_TO_RADIANS (PAR_PI / 180.0)

void par_color_mix_hcl(float const* a, float const* b, float* result, float t)
{
    float ah = a[0],
        ac = a[1],
        al = a[2],
        bh = b[0] - ah,
        bc = b[1] - ac,
        bl = b[2] - al;
    if (bh > 180) {
        bh -= 360;
    } else if (bh < -180) {
        bh += 360;
    }
    result[0] = ah + bh * t;
    result[1] = ac + bc * t;
    result[2] = al + bl * t;
}

void par_color_hcl_to_lab(float const* src, float* result)
{
    float h = src[0] * PAR_COLOR_TO_RADIANS, c = src[1], l = src[2];
    result[0] = l;
    result[1] = c * cos(h);
    result[2] = c * sin(h);
}

static float par_color__lab_xyz(float x)
{
    return x > 0.206893034 ? (x * x * x) : (x - 4.0 / 29.0) / 7.787037;
}

static float par_color__xyz_rgb(float r)
{
    return 255.0 * (r <= 0.00304 ? 12.92 * r : 1.055 * pow(r, 1.0 / 2.4) - 0.055);
}

void par_color_lab_to_rgb(float const* src, float* result)
{
    float lab_X = 0.950470, lab_Y = 1.0, lab_Z = 1.088830;
    float y = (src[0] + 16.0) / 116.0;
    float x = y + src[1] / 500.0;
    float z = y - src[2] / 200.0;
    x = par_color__lab_xyz(x) * lab_X;
    y = par_color__lab_xyz(y) * lab_Y;
    z = par_color__lab_xyz(z) * lab_Z;
    result[0] = par_color__xyz_rgb( 3.2404542 * x - 1.5371385 * y - 0.4985314 * z);
    result[1] = par_color__xyz_rgb(-0.9692660 * x + 1.8760108 * y + 0.0415560 * z);
    result[2] = par_color__xyz_rgb( 0.0556434 * x - 0.2040259 * y + 1.0572252 * z);
}

void par_color_hcl_to_rgb(float const* src, float* result)
{
    par_color_hcl_to_lab(src, result);
    par_color_lab_to_rgb(result, result);
}

#endif // PAR_COLOR_IMPLEMENTATION
