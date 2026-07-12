/**
 * @file lv_draw_3d_camera.h
 *
 */

#ifndef LV_DRAW_3D_CAMERA_H
#define LV_DRAW_3D_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../config/lv_conf_internal.h"
#include "../lv_types.h"

#if LV_USE_3D_DRAW_TASKS

#if LV_USE_GLTF
#include "../3d/lv_3dmath.h"
#else

typedef struct {
    float x;
    float y;
    float z;
} lv_3dpoint_t;

#endif

#define LV_3D_CAMERA_MVP_SIZE 16

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    float yaw;
    float pitch;
    float distance;
    lv_3dpoint_t target;
    float fov_y;
    float near_z;
    float far_z;
} lv_3d_camera_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_3d_camera_init(lv_3d_camera_t * cam);

void lv_3d_camera_set_orbit(lv_3d_camera_t * cam, float yaw, float pitch, float distance);

void lv_3d_camera_get_eye(const lv_3d_camera_t * cam, lv_3dpoint_t * eye);

void lv_3d_camera_compute_mvp(const lv_3d_camera_t * cam, int32_t w, int32_t h, float mvp_out[LV_3D_CAMERA_MVP_SIZE]);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_CAMERA_H*/
