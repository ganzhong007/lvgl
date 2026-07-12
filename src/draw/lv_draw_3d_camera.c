/**
 * @file lv_draw_3d_camera.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../include/lvgl/draw/lv_draw_3d_camera.h"
#include "lv_draw_private.h"

#if LV_USE_3D_DRAW_TASKS

#include <math.h>

/*********************
 *      DEFINES
 *********************/

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void mat4_identity(float m[16]);
static void mat4_mul(float out[16], const float a[16], const float b[16]);
static void mat4_perspective_rh(float m[16], float fov_y, float aspect, float z_near, float z_far);
static void mat4_look_at_rh(float m[16], const lv_3dpoint_t * eye, const lv_3dpoint_t * center,
                            const lv_3dpoint_t * up);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_3d_camera_init(lv_3d_camera_t * cam)
{
    lv_memzero(cam, sizeof(*cam));
    cam->yaw = 0.6f;
    cam->pitch = 0.35f;
    cam->distance = 4.5f;
    cam->target = (lv_3dpoint_t) { 0.f, 0.f, 0.f };
    cam->fov_y = (float)(45.0 * M_PI / 180.0);
    cam->near_z = 0.1f;
    cam->far_z = 100.f;
}

void lv_3d_camera_set_orbit(lv_3d_camera_t * cam, float yaw, float pitch, float distance)
{
    if(cam == NULL) return;
    cam->yaw = yaw;
    cam->pitch = pitch;
    if(distance > 0.1f) cam->distance = distance;
}

void lv_3d_camera_get_eye(const lv_3d_camera_t * cam, lv_3dpoint_t * eye)
{
    float cp = cosf(cam->pitch);
    eye->x = cam->target.x + cam->distance * cp * sinf(cam->yaw);
    eye->y = cam->target.y + cam->distance * sinf(cam->pitch);
    eye->z = cam->target.z + cam->distance * cp * cosf(cam->yaw);
}

void lv_3d_camera_compute_mvp(const lv_3d_camera_t * cam, int32_t w, int32_t h, float mvp_out[LV_3D_CAMERA_MVP_SIZE])
{
    lv_3dpoint_t eye;
    lv_3dpoint_t up = { 0.f, 1.f, 0.f };
    float view[16];
    float proj[16];

    lv_3d_camera_get_eye(cam, &eye);
    mat4_look_at_rh(view, &eye, &cam->target, &up);

    float aspect = (h > 0) ? ((float)w / (float)h) : 1.f;
    mat4_perspective_rh(proj, cam->fov_y, aspect, cam->near_z, cam->far_z);
    mat4_mul(mvp_out, proj, view);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void mat4_identity(float m[16])
{
    lv_memzero(m, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

static void mat4_mul(float out[16], const float a[16], const float b[16])
{
    float r[16];
    for(int col = 0; col < 4; col++) {
        for(int row = 0; row < 4; row++) {
            r[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0]
                               + a[1 * 4 + row] * b[col * 4 + 1]
                               + a[2 * 4 + row] * b[col * 4 + 2]
                               + a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    lv_memcpy(out, r, sizeof(r));
}

static void mat4_perspective_rh(float m[16], float fov_y, float aspect, float z_near, float z_far)
{
    mat4_identity(m);
    float t = tanf(fov_y * 0.5f);
    m[0] = 1.f / (aspect * t);
    m[5] = 1.f / t;
    m[10] = -(z_far + z_near) / (z_far - z_near);
    m[11] = -1.f;
    m[14] = -(2.f * z_far * z_near) / (z_far - z_near);
    m[15] = 0.f;
}

static void mat4_look_at_rh(float m[16], const lv_3dpoint_t * eye, const lv_3dpoint_t * center,
                            const lv_3dpoint_t * up)
{
    lv_3dpoint_t f = {
        center->x - eye->x,
        center->y - eye->y,
        center->z - eye->z,
    };
    float flen = sqrtf(f.x * f.x + f.y * f.y + f.z * f.z);
    if(flen > 0.f) {
        f.x /= flen;
        f.y /= flen;
        f.z /= flen;
    }

    lv_3dpoint_t s = {
        f.y * up->z - f.z * up->y,
        f.z * up->x - f.x * up->z,
        f.x * up->y - f.y * up->x,
    };
    float slen = sqrtf(s.x * s.x + s.y * s.y + s.z * s.z);
    if(slen > 0.f) {
        s.x /= slen;
        s.y /= slen;
        s.z /= slen;
    }

    lv_3dpoint_t u = {
        s.y * f.z - s.z * f.y,
        s.z * f.x - s.x * f.z,
        s.x * f.y - s.y * f.x,
    };

    mat4_identity(m);
    m[0 * 4 + 0] = s.x;
    m[0 * 4 + 1] = u.x;
    m[0 * 4 + 2] = -f.x;
    m[1 * 4 + 0] = s.y;
    m[1 * 4 + 1] = u.y;
    m[1 * 4 + 2] = -f.y;
    m[2 * 4 + 0] = s.z;
    m[2 * 4 + 1] = u.z;
    m[2 * 4 + 2] = -f.z;
    m[3 * 4 + 0] = -(s.x * eye->x + s.y * eye->y + s.z * eye->z);
    m[3 * 4 + 1] = -(u.x * eye->x + u.y * eye->y + u.z * eye->z);
    m[3 * 4 + 2] = (f.x * eye->x + f.y * eye->y + f.z * eye->z);
}

#endif /*LV_USE_3D_DRAW_TASKS*/
