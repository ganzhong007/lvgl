/**
 * @file lv_draw_3d_callback.h
 *
 */

#ifndef LV_DRAW_3D_CALLBACK_H
#define LV_DRAW_3D_CALLBACK_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw.h"
#include "lv_draw_3d_camera.h"

#if LV_USE_3D_DRAW_TASKS

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef void (*lv_draw_3d_cb_t)(lv_layer_t * pass_layer, const lv_3d_camera_t * cam,
                                const float * view_proj, void * user_data);

typedef struct {
    lv_draw_dsc_base_t base;
    lv_draw_3d_cb_t cb;
    void * user_data;
} lv_draw_3d_callback_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_3d_callback_dsc_init(lv_draw_3d_callback_dsc_t * dsc);

lv_draw_3d_callback_dsc_t * lv_draw_task_get_3d_callback_dsc(lv_draw_task_t * task);

void lv_draw_3d_callback(lv_layer_t * pass_layer, const lv_draw_3d_callback_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_CALLBACK_H*/
