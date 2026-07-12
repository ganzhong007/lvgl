/**
 * @file lv_3d_pick.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../include/lvgl/draw/lv_draw_3d_pick.h"
#include "lv_draw_private.h"

#if LV_USE_3D_DRAW_TASKS

#include <math.h>

/*********************
 *      DEFINES
 *********************/

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define EPSILON 1e-6f

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void normalize3(lv_3dpoint_t * v);
static void cross3(const lv_3dpoint_t * a, const lv_3dpoint_t * b, lv_3dpoint_t * out);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_3d_camera_get_ray(const lv_3d_camera_t * cam, int32_t w, int32_t h, int32_t px, int32_t py,
                          lv_3dray_t * ray)
{
    static bool s_pick_ready;
    if(!s_pick_ready) {
        s_pick_ready = true;
        LV_LOG_INFO("G100 3D pick ready (ray-mesh intersection)");
    }

    if(cam == NULL || ray == NULL || w <= 0 || h <= 0) return;

    lv_3d_camera_get_eye(cam, &ray->origin);

    lv_3dpoint_t forward = {
        cam->target.x - ray->origin.x,
        cam->target.y - ray->origin.y,
        cam->target.z - ray->origin.z,
    };
    normalize3(&forward);

    lv_3dpoint_t world_up = { 0.f, 1.f, 0.f };
    lv_3dpoint_t right;
    cross3(&forward, &world_up, &right);
    normalize3(&right);

    lv_3dpoint_t up;
    cross3(&right, &forward, &up);
    normalize3(&up);

    float u = ((float)px + 0.5f) / (float)w;
    float v = ((float)py + 0.5f) / (float)h;
    float ndc_x = u * 2.f - 1.f;
    float ndc_y = 1.f - v * 2.f;
    float aspect = (float)w / (float)h;
    float tan_half = tanf(cam->fov_y * 0.5f);
    float lx = ndc_x * aspect * tan_half;
    float ly = ndc_y * tan_half;

    ray->direction.x = forward.x + right.x * lx + up.x * ly;
    ray->direction.y = forward.y + right.y * lx + up.y * ly;
    ray->direction.z = forward.z + right.z * lx + up.z * ly;
    normalize3(&ray->direction);
}

bool lv_3d_ray_triangle(const lv_3dray_t * ray,
                        const lv_3dpoint_t * v0, const lv_3dpoint_t * v1, const lv_3dpoint_t * v2,
                        float * t_out)
{
    if(ray == NULL || v0 == NULL || v1 == NULL || v2 == NULL) return false;

    lv_3dpoint_t e1 = { v1->x - v0->x, v1->y - v0->y, v1->z - v0->z };
    lv_3dpoint_t e2 = { v2->x - v0->x, v2->y - v0->y, v2->z - v0->z };

    lv_3dpoint_t pvec;
    cross3(&ray->direction, &e2, &pvec);
    float det = e1.x * pvec.x + e1.y * pvec.y + e1.z * pvec.z;
    if(fabsf(det) < EPSILON) return false;

    float inv_det = 1.f / det;
    lv_3dpoint_t tvec = {
        ray->origin.x - v0->x,
        ray->origin.y - v0->y,
        ray->origin.z - v0->z,
    };
    float u = (tvec.x * pvec.x + tvec.y * pvec.y + tvec.z * pvec.z) * inv_det;
    if(u < 0.f || u > 1.f) return false;

    lv_3dpoint_t qvec;
    cross3(&tvec, &e1, &qvec);
    float v = (ray->direction.x * qvec.x + ray->direction.y * qvec.y + ray->direction.z * qvec.z) * inv_det;
    if(v < 0.f || u + v > 1.f) return false;

    float t = (e2.x * qvec.x + e2.y * qvec.y + e2.z * qvec.z) * inv_det;
    if(t < EPSILON) return false;

    if(t_out) *t_out = t;
    return true;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void normalize3(lv_3dpoint_t * v)
{
    float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if(len > EPSILON) {
        v->x /= len;
        v->y /= len;
        v->z /= len;
    }
}

static void cross3(const lv_3dpoint_t * a, const lv_3dpoint_t * b, lv_3dpoint_t * out)
{
    out->x = a->y * b->z - a->z * b->y;
    out->y = a->z * b->x - a->x * b->z;
    out->z = a->x * b->y - a->y * b->x;
}

#endif /*LV_USE_3D_DRAW_TASKS*/
