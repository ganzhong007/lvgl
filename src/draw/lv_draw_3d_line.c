/**
 * @file lv_draw_3d_line.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_line.h"

#if LV_USE_3D_DRAW_TASKS

void lv_draw_3d_line_dsc_init(lv_draw_3d_line_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_3d_line_dsc_t));
    dsc->base.dsc_size = sizeof(lv_draw_3d_line_dsc_t);
    dsc->color = lv_color32_make(0xFF, 0xFF, 0xFF, LV_OPA_COVER);
    dsc->width = 1.f;
    dsc->depth_test = true;
}

lv_draw_3d_line_dsc_t * lv_draw_task_get_3d_line_dsc(lv_draw_task_t * task)
{
    return task->type == LV_DRAW_TASK_TYPE_3D_LINE ? (lv_draw_3d_line_dsc_t *)task->draw_dsc : NULL;
}

void lv_draw_3d_line(lv_layer_t * pass_layer, const lv_draw_3d_line_dsc_t * dsc)
{
    if(pass_layer == NULL || dsc == NULL || dsc->points == NULL || dsc->point_cnt < 2) return;

    LV_PROFILER_DRAW_BEGIN;

    lv_area_t area = pass_layer->buf_area;
    lv_draw_task_t * t = lv_draw_add_task(pass_layer, &area, LV_DRAW_TASK_TYPE_3D_LINE);
    lv_draw_3d_line_dsc_t * new_dsc = t->draw_dsc;

    new_dsc->base = dsc->base;
    new_dsc->base.dsc_size = sizeof(lv_draw_3d_line_dsc_t);
    new_dsc->color = dsc->color;
    new_dsc->width = dsc->width;
    new_dsc->depth_test = dsc->depth_test;
    new_dsc->point_cnt = dsc->point_cnt;
    new_dsc->points = lv_malloc(sizeof(lv_3dpoint_t) * dsc->point_cnt);
    if(new_dsc->points == NULL) {
        t->state = LV_DRAW_TASK_STATE_FINISHED;
        LV_PROFILER_DRAW_END;
        return;
    }
    lv_memcpy((void *)new_dsc->points, dsc->points, sizeof(lv_3dpoint_t) * dsc->point_cnt);

    lv_draw_finalize_task_creation(pass_layer, t);

    LV_PROFILER_DRAW_END;
}

#endif /*LV_USE_3D_DRAW_TASKS*/
