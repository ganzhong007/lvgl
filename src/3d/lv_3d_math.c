/**
 * @file lv_3d_math.c — OpenGL column-major matrices
 */

#include "lv_3d_internal.h"

#if LV_USE_3D

#include <math.h>

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

#endif /*LV_USE_3D*/
