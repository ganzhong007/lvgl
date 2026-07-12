/**
 * @file lv_draw_3d_pick.h
 *
 */

#ifndef LV_DRAW_3D_PICK_H
#define LV_DRAW_3D_PICK_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../config/lv_conf_internal.h"

#if LV_USE_3D_DRAW_TASKS

#include "../draw/lv_draw_3d_camera.h"

/*********************
 *      DEFINES
 *********************/

#if !LV_USE_GLTF
typedef struct {
    lv_3dpoint_t origin;
    lv_3dpoint_t direction;
} lv_3dray_t;
#endif

typedef struct {
    lv_obj_t * target;
    lv_3dpoint_t point;
    float distance;
} lv_3d_pick_hit_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_3d_camera_get_ray(const lv_3d_camera_t * cam, int32_t w, int32_t h, int32_t px, int32_t py,
                          lv_3dray_t * ray);

bool lv_3d_ray_triangle(const lv_3dray_t * ray,
                        const lv_3dpoint_t * v0, const lv_3dpoint_t * v1, const lv_3dpoint_t * v2,
                        float * t_out);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_PICK_H*/
