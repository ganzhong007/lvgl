/**
 * @file lv_3dlight.h
 *
 */

#ifndef LV_3DLIGHT_H
#define LV_3DLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../config/lv_conf_internal.h"
#if LV_USE_3DLIGHT

#include "../core/lv_obj.h"
#include "../draw/lv_draw_3d_light.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_3dlight_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_obj_t * lv_3dlight_create(lv_obj_t * parent);

void lv_3dlight_set_directional(lv_obj_t * obj, float dx, float dy, float dz,
                                lv_color_t color, lv_opa_t opa, float intensity);

void lv_3dlight_set_point(lv_obj_t * obj, float x, float y, float z,
                          lv_color_t color, lv_opa_t opa, float intensity, float range);

void lv_3dlight_submit_tree(lv_obj_t * root, lv_layer_t * pass_layer);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3DLIGHT*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DLIGHT_H*/
