/**
 * @file lv_draw_3d_viewport.h
 *
 */

#ifndef LV_DRAW_3D_VIEWPORT_H
#define LV_DRAW_3D_VIEWPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw.h"

#if LV_USE_3D_DRAW_TASKS

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    LV_3D_RESOLVE_TO_LAYER,
    LV_3D_RESOLVE_TO_TEXTURE,
    LV_3D_DIRECT_TO_LAYER,
} lv_3d_resolve_mode_t;

typedef struct {
    lv_draw_dsc_base_t base;
    lv_3d_resolve_mode_t resolve_mode;
    lv_color_format_t color_format;
    uint8_t depth_bits;
    lv_opa_t opa;
    lv_layer_t * pass_layer;
} lv_draw_3d_viewport_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_3d_viewport_dsc_init(lv_draw_3d_viewport_dsc_t * dsc);

lv_draw_3d_viewport_dsc_t * lv_draw_task_get_3d_viewport_dsc(lv_draw_task_t * task);

lv_layer_t * lv_draw_3d_pass_layer_create(lv_layer_t * parent_layer, const lv_area_t * area);

void lv_draw_3d_viewport(lv_layer_t * layer, const lv_draw_3d_viewport_dsc_t * dsc, const lv_area_t * coords);

void lv_draw_3d_viewport_end(lv_layer_t * pass_layer);

void lv_draw_3d_pass_layer_destroy(lv_layer_t * pass_layer, lv_display_t * disp);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_VIEWPORT_H*/
