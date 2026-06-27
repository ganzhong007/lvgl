/**
 * @file lv_3d_transform.c
 */

#include "lv_3d_internal.h"

#if LV_USE_3D

#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

void lv_3d_transform_init(lv_3d_transform_t * t)
{
    lv_3d_mat4_identity(t->local);
    lv_3d_mat4_identity(t->world);
    t->dirty = true;
}

void lv_3d_transform_set_local_trs(lv_3d_transform_t * t, float tx, float ty, float tz,
                                   float rx, float ry, float rz, float sx, float sy, float sz)
{
    float mt[16], ms[16], mrx[16], mry[16], mrz[16], tmp[16];
    lv_3d_mat4_translate(mt, tx, ty, tz);
    lv_3d_mat4_scale(ms, sx, sy, sz);

    lv_3d_mat4_identity(mrx);
    mrx[5] = cosf(rx); mrx[6] = sinf(rx); mrx[9] = -sinf(rx); mrx[10] = cosf(rx);
    lv_3d_mat4_identity(mry);
    mry[0] = cosf(ry); mry[2] = -sinf(ry); mry[8] = sinf(ry); mry[10] = cosf(ry);
    lv_3d_mat4_identity(mrz);
    mrz[0] = cosf(rz); mrz[1] = -sinf(rz); mrz[4] = sinf(rz); mrz[5] = cosf(rz);

    lv_3d_mat4_mul(tmp, mrx, ms);
    lv_3d_mat4_mul(t->local, mry, tmp);
    lv_3d_mat4_mul(tmp, mrz, t->local);
    lv_3d_mat4_mul(t->local, mt, tmp);
    t->dirty = true;
}

void lv_3d_transform_update_world(lv_3d_transform_t * t, const float parent_world[16])
{
    lv_3d_mat4_mul(t->world, parent_world, t->local);
    t->dirty = false;
}

#endif /*LV_USE_3D*/
