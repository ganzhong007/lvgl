/**
 * @file lv_3dlight_private.h
 *
 */

#ifndef LV_3DLIGHT_PRIVATE_H
#define LV_3DLIGHT_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_3DLIGHT

#include "../../core/lv_obj_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_light.h"

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_3dlight_t {
    lv_obj_t obj;
    lv_3d_light_dsc_t dsc;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_3dlight_submit(lv_obj_t * obj, lv_layer_t * pass_layer);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3DLIGHT*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DLIGHT_PRIVATE_H*/
