/**
 * @file lv_3dviewport_private.h
 *
 */

#ifndef LV_3DVIEWPORT_PRIVATE_H
#define LV_3DVIEWPORT_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_3DVIEWPORT

#include "../../core/lv_obj_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_camera.h"
#include "../../include/lvgl/draw/lv_draw_3d_callback.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_3dviewport_t {
    lv_obj_t obj;
    lv_layer_t * pass_layer;
    lv_color32_t clear_color;
    bool clear_depth;
    lv_3d_camera_t camera;
    lv_draw_3d_cb_t render_cb;
    void * render_user_data;
    bool show_grid;
    lv_color32_t grid_color;
    lv_point_t last_drag;
    lv_point_t press_start;
    bool dragging;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3DVIEWPORT*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DVIEWPORT_PRIVATE_H*/
