/**
 * @file lv_3d_math.c — OpenGL column-major matrices
 */

#include "lv_3d_internal.h"

#if LV_USE_3D

#include <math.h>
#include <float.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

void lv_3d_mat4_identity(float m[16])
{
    lv_memzero(m, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void lv_3d_mat4_mul(float out[16], const float a[16], const float b[16])
{
    float r[16];
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            r[i + j * 4] = a[i + 0 * 4] * b[0 + j * 4]
                         + a[i + 1 * 4] * b[1 + j * 4]
                         + a[i + 2 * 4] * b[2 + j * 4]
                         + a[i + 3 * 4] * b[3 + j * 4];
        }
    }
    lv_memcpy(out, r, sizeof(r));
}

void lv_3d_mat4_translate(float m[16], float x, float y, float z)
{
    lv_3d_mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

void lv_3d_mat4_scale(float m[16], float sx, float sy, float sz)
{
    lv_3d_mat4_identity(m);
    m[0] = sx;
    m[5] = sy;
    m[10] = sz;
}

void lv_3d_mat4_rotate_y(float m[16], float rad)
{
    lv_3d_mat4_identity(m);
    float c = cosf(rad);
    float s = sinf(rad);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

void lv_3d_mat4_perspective(float m[16], float fov_deg, float aspect, float near_z, float far_z)
{
    lv_memzero(m, sizeof(float) * 16);
    float tanHalf = tanf(fov_deg * 0.5f * (float)(M_PI / 180.0));
    m[0] = 1.0f / (aspect * tanHalf);
    m[5] = 1.0f / tanHalf;
    m[10] = -(far_z + near_z) / (far_z - near_z);
    m[11] = -1.0f;
    m[14] = -(2.0f * far_z * near_z) / (far_z - near_z);
}

void lv_3d_mat4_look_at(float m[16], lv_vec3_t eye, lv_vec3_t target, lv_vec3_t up)
{
    lv_vec3_t f = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
    float fl = sqrtf(f.x * f.x + f.y * f.y + f.z * f.z);
    if(fl > 0.0001f) { f.x /= fl; f.y /= fl; f.z /= fl; }

    lv_vec3_t s = { f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x };
    float sl = sqrtf(s.x * s.x + s.y * s.y + s.z * s.z);
    if(sl > 0.0001f) { s.x /= sl; s.y /= sl; s.z /= sl; }

    lv_vec3_t u = { s.y * f.z - s.z * f.y, s.z * f.x - s.x * f.z, s.x * f.y - s.y * f.x };

    lv_3d_mat4_identity(m);
    m[0] = s.x;  m[1] = s.y;  m[2] = s.z;
    m[4] = u.x;  m[5] = u.y;  m[6] = u.z;
    m[8] = -f.x; m[9] = -f.y; m[10] = -f.z;
    m[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    m[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    m[14] = (f.x * eye.x + f.y * eye.y + f.z * eye.z);
}

bool lv_3d_mat4_invert(float out[16], const float m[16])
{
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] -
             m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] +
             m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] -
             m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] +
              m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] +
             m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] -
             m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] +
             m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] -
              m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] -
             m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] +
             m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] -
              m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] +
              m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] +
             m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] -
             m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] +
              m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if(fabsf(det) < 1e-8f) return false;

    det = 1.0f / det;
    for(int i = 0; i < 16; i++) out[i] = inv[i] * det;
    return true;
}

void lv_3d_mat4_transform_point(const float m[16], lv_vec3_t in, lv_vec3_t * out)
{
    float x = m[0] * in.x + m[4] * in.y + m[8] * in.z + m[12];
    float y = m[1] * in.x + m[5] * in.y + m[9] * in.z + m[13];
    float z = m[2] * in.x + m[6] * in.y + m[10] * in.z + m[14];
    float w = m[3] * in.x + m[7] * in.y + m[11] * in.z + m[15];
    if(fabsf(w) > 1e-8f) {
        out->x = x / w;
        out->y = y / w;
        out->z = z / w;
    }
    else {
        out->x = x;
        out->y = y;
        out->z = z;
    }
}

void lv_3d_mat4_transform_dir(const float m[16], lv_vec3_t in, lv_vec3_t * out)
{
    out->x = m[0] * in.x + m[4] * in.y + m[8] * in.z;
    out->y = m[1] * in.x + m[5] * in.y + m[9] * in.z;
    out->z = m[2] * in.x + m[6] * in.y + m[10] * in.z;
}

static void unproject_ndc(const float inv_vp[16], float ndc_x, float ndc_y, float ndc_z, lv_vec3_t * out)
{
    float x = inv_vp[0] * ndc_x + inv_vp[4] * ndc_y + inv_vp[8] * ndc_z + inv_vp[12];
    float y = inv_vp[1] * ndc_x + inv_vp[5] * ndc_y + inv_vp[9] * ndc_z + inv_vp[13];
    float z = inv_vp[2] * ndc_x + inv_vp[6] * ndc_y + inv_vp[10] * ndc_z + inv_vp[14];
    float w = inv_vp[3] * ndc_x + inv_vp[7] * ndc_y + inv_vp[11] * ndc_z + inv_vp[15];
    if(fabsf(w) > 1e-8f) {
        out->x = x / w;
        out->y = y / w;
        out->z = z / w;
    }
    else {
        out->x = x;
        out->y = y;
        out->z = z;
    }
}

void lv_3d_ray_from_screen(int32_t x, int32_t y, int32_t w, int32_t h,
                           const float view[16], const float proj[16],
                           lv_vec3_t * origin, lv_vec3_t * dir)
{
    float vp[16], inv_vp[16];
    lv_3d_mat4_mul(vp, proj, view);
    if(!lv_3d_mat4_invert(inv_vp, vp)) {
        origin->x = origin->y = origin->z = 0.0f;
        dir->x = dir->y = 0.0f;
        dir->z = -1.0f;
        return;
    }

    if(w < 1) w = 1;
    if(h < 1) h = 1;

    float ndc_x = 2.0f * ((float)x + 0.5f) / (float)w - 1.0f;
    float ndc_y = 1.0f - 2.0f * ((float)y + 0.5f) / (float)h;

    lv_vec3_t near_p, far_p;
    unproject_ndc(inv_vp, ndc_x, ndc_y, -1.0f, &near_p);
    unproject_ndc(inv_vp, ndc_x, ndc_y, 1.0f, &far_p);

    origin->x = near_p.x;
    origin->y = near_p.y;
    origin->z = near_p.z;
    dir->x = far_p.x - near_p.x;
    dir->y = far_p.y - near_p.y;
    dir->z = far_p.z - near_p.z;
    float dl = sqrtf(dir->x * dir->x + dir->y * dir->y + dir->z * dir->z);
    if(dl > 1e-8f) {
        dir->x /= dl;
        dir->y /= dl;
        dir->z /= dl;
    }
}

#endif /*LV_USE_3D*/
