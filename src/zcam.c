#include <parg.h>

static DMatrix4 _projmat;
static DPoint3 _camerapos;
static double _maxcamz;
static double _mincamz;
static double _fovy;
static double _winaspect = 0;
static double _zplanes[2];
static DPoint3 _grabpt;
static int _grabbing = 0;
static int _dirty = 1;

#define MIN(a, b) (a > b ? b : a)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(v, lo, hi) MAX(lo, MIN(hi, v))

DPoint3 parg_zcam_from_world(DPoint3 worldpt)
{
    double vpheight = 2 * tan(_fovy / 2) * _camerapos.z;
    double vpwidth = vpheight * _winaspect;
    DPoint3 winpt = {0};
    winpt.y = 0.5 + (worldpt.y - _camerapos.y) / vpheight;
    winpt.x = 0.5 + (worldpt.x - _camerapos.x) / vpwidth;
    return winpt;
}

DPoint3 parg_zcam_to_world(float winx, float winy)
{
    DPoint3 worldspace;
    double vpheight = 2 * tan(_fovy / 2) * _camerapos.z;
    double vpwidth = vpheight * _winaspect;
    worldspace.y = _camerapos.y + vpheight * (winy - 0.5);
    worldspace.x = _camerapos.x + vpwidth * (winx - 0.5);
    worldspace.z = 0;
    return worldspace;
}

void parg_zcam_get_viewport(double* lbrt)
{
    double vpheight = 2 * tan(_fovy / 2) * _camerapos.z;
    double vpwidth = vpheight * _winaspect;
    double left = _camerapos.x - vpwidth * 0.5;
    double bottom = _camerapos.y - vpheight * 0.5;
    double right = _camerapos.x + vpwidth * 0.5;
    double top = _camerapos.y + vpheight * 0.5;
    *lbrt++ = left;
    *lbrt++ = bottom;
    *lbrt++ = right;
    *lbrt = top;
}

void parg_zcam_init(float worldwidth, float worldheight, float fovy)
{
    _maxcamz = 0.5 * worldheight / tan(fovy * 0.5);
    _camerapos = (DPoint3){0, 0, _maxcamz};
    _mincamz = 0;
    _zplanes[0] = _mincamz;
    _zplanes[1] = _maxcamz * 1.5;
    _fovy = fovy;
}

void parg_zcam_set_aspect(float winaspect)
{
    if (_winaspect != winaspect) {
        _winaspect = winaspect;
        double* z = _zplanes;
        _projmat = DM4MakePerspective(_fovy, _winaspect, z[0], z[1]);
        parg_zcam_touch();
    }
}

void parg_zcam_grab_begin(float winx, float winy)
{
    _grabbing = 1;
    _grabpt = parg_zcam_to_world(winx, winy);
}

void parg_zcam_grab_update(float winx, float winy, float scrolldelta)
{
    DPoint3 prev = _camerapos;
    if (_grabbing) {
        double vpheight = 2 * tan(_fovy / 2) * _camerapos.z;
        double vpwidth = vpheight * _winaspect;
        _camerapos.y = -vpheight * (winy - 0.5) + _grabpt.y;
        _camerapos.x = -vpwidth * (winx - 0.5) + _grabpt.x;
    } else if (scrolldelta) {
        DPoint3 focalpt = parg_zcam_to_world(winx, winy);
        _camerapos.z -= scrolldelta * _camerapos.z * 0.01;
        _camerapos.z = CLAMP(_camerapos.z, _mincamz, _maxcamz);
        double vpheight = 2 * tan(_fovy / 2) * _camerapos.z;
        double vpwidth = vpheight * _winaspect;
        _camerapos.y = -vpheight * (winy - 0.5) + focalpt.y;
        _camerapos.x = -vpwidth * (winx - 0.5) + focalpt.x;
    }
    _dirty |= prev.x != _camerapos.x || prev.y != _camerapos.y ||
        prev.z != _camerapos.z;
}

void parg_zcam_set_viewport(double const* xyw)
{
    DPoint3 previous;
    if (_grabbing) {
        previous = parg_zcam_from_world(_grabpt);
    }
    double vpheight = xyw[2] / _winaspect;
    _camerapos.x = xyw[0];
    _camerapos.y = xyw[1];
    _camerapos.z = 0.5 * vpheight / tan(_fovy / 2);
    _dirty = 1;
    if (_grabbing) {
        _grabpt = parg_zcam_to_world(previous.x, previous.y);
    }
}

void parg_zcam_grab_end() { _grabbing = 0; }

DPoint3 parg_zcam_get_camera(Matrix4* vp)
{
    DPoint3 origin = {0, 0, 0};
    DPoint3 target = {0, 0, -1};
    DVector3 up = {0, 1, 0};
    DMatrix4 view = DM4MakeLookAt(origin, target, up);
    if (vp) {
        *vp = M4MakeFromDM4(DM4Mul(_projmat, view));
    }
    return _camerapos;
}

int parg_zcam_has_moved()
{
    int retval = _dirty;
    _dirty = 0;
    return retval;
}

void parg_zcam_touch() { _dirty = 1; }

// Van Wijk Interpolation: consumes two 3-tuples and produces one 3-tuple.
// cameraA... XY starting center point and viewport size
// cameraB... XY ending center point and viewport size
// result...  XY computed center and viewport size
// t......... if -1, returns a recommended duration
void parg_zcam_blend(
    double const* cameraA, double const* cameraB, double* result, double t)
{
    double rho = sqrt(2.0), rho2 = 2, rho4 = 4, ux0 = cameraA[0],
        uy0 = cameraA[1], w0 = cameraA[2], ux1 = cameraB[0],
        uy1 = cameraB[1], w1 = cameraB[2], dx = ux1 - ux0, dy = uy1 - uy0,
        d2 = dx * dx + dy * dy, d1 = sqrt(d2),
        b0 = (w1 * w1 - w0 * w0 + rho4 * d2) / (2.0 * w0 * rho2 * d1),
        b1 = (w1 * w1 - w0 * w0 - rho4 * d2) / (2.0 * w1 * rho2 * d1),
        r0 = log(sqrt(b0 * b0 + 1.0) - b0), r1 = log(sqrt(b1 * b1 + 1) - b1),
        dr = r1 - r0;
    int validdr = (dr == dr) && dr != 0;
    double S = (validdr ? dr : log(w1 / w0)) / rho;
    if (t == -1) {
        result[0] = fabs(S * 1000.0);
        return;
    }
    double s = t * S;
    if (validdr) {
        double coshr0 = cosh(r0),
            u = w0 / (rho2 * d1) * (coshr0 * tanh(rho * s + r0) - sinh(r0));
        result[0] = ux0 + u * dx;
        result[1] = uy0 + u * dy;
        result[2] = w0 * coshr0 / cosh(rho * s + r0);
        return;
    }
    result[0] = ux0 + t * dx;
    result[1] = uy0 + t * dy;
    result[2] = w0 * exp(rho * s);
}
