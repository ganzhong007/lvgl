/**
 * @file lv_draw_3d_clear.h
 *
 */

#ifndef LV_DRAW_3D_CLEAR_H
#define LV_DRAW_3D_CLEAR_H

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

typedef struct {
    lv_draw_dsc_base_t base;
    lv_color32_t color;
    lv_opa_t opa;
    bool clear_depth;
    float depth_value;
} lv_draw_3d_clear_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_3d_clear_dsc_init(lv_draw_3d_clear_dsc_t * dsc);

lv_draw_3d_clear_dsc_t * lv_draw_task_get_3d_clear_dsc(lv_draw_task_t * task);

void lv_draw_3d_clear(lv_layer_t * pass_layer, const lv_draw_3d_clear_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_CLEAR_H*/
