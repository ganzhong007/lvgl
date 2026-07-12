/**
 * @file lv_3dviewport.h
 *
 */

#ifndef LV_3DVIEWPORT_H
#define LV_3DVIEWPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../config/lv_conf_internal.h"
#if LV_USE_3DVIEWPORT

#include "../core/lv_obj.h"
#include "../draw/lv_draw_3d_camera.h"
#include "../draw/lv_draw_3d_callback.h"
#include "../draw/lv_draw_3d_pick.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_3dviewport_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_obj_t * lv_3dviewport_create(lv_obj_t * parent);

void lv_3dviewport_set_clear_color(lv_obj_t * obj, lv_color_t color, lv_opa_t opa);

void lv_3dviewport_set_clear_depth(lv_obj_t * obj, bool clear_depth);

void lv_3dviewport_set_grid_visible(lv_obj_t * obj, bool visible);

void lv_3dviewport_set_render_cb(lv_obj_t * obj, lv_draw_3d_cb_t cb, void * user_data);

lv_3d_camera_t * lv_3dviewport_get_camera(lv_obj_t * obj);

void lv_3dviewport_set_orbit(lv_obj_t * obj, float yaw, float pitch, float distance);

lv_3dray_t lv_3dviewport_get_ray_from_point(lv_obj_t * obj, int32_t x, int32_t y);

bool lv_3dviewport_pick_at(lv_obj_t * obj, int32_t x, int32_t y, lv_3d_pick_hit_t * hit);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3DVIEWPORT*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DVIEWPORT_H*/
