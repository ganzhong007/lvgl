/**
 * @file lv_draw_3d_line.h
 *
 */

#ifndef LV_DRAW_3D_LINE_H
#define LV_DRAW_3D_LINE_H

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

typedef struct {
    lv_draw_dsc_base_t base;
    const lv_3dpoint_t * points;
    uint32_t point_cnt;
    lv_color32_t color;
    float width;
    bool depth_test;
} lv_draw_3d_line_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_3d_line_dsc_init(lv_draw_3d_line_dsc_t * dsc);

lv_draw_3d_line_dsc_t * lv_draw_task_get_3d_line_dsc(lv_draw_task_t * task);

void lv_draw_3d_line(lv_layer_t * pass_layer, const lv_draw_3d_line_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_LINE_H*/
