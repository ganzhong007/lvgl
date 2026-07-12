/**
 * @file lv_draw_3d_callback.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_callback.h"

#if LV_USE_3D_DRAW_TASKS

void lv_draw_3d_callback_dsc_init(lv_draw_3d_callback_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_3d_callback_dsc_t));
    dsc->base.dsc_size = sizeof(lv_draw_3d_callback_dsc_t);
}

lv_draw_3d_callback_dsc_t * lv_draw_task_get_3d_callback_dsc(lv_draw_task_t * task)
{
    return task->type == LV_DRAW_TASK_TYPE_3D_CALLBACK ? (lv_draw_3d_callback_dsc_t *)task->draw_dsc : NULL;
}

void lv_draw_3d_callback(lv_layer_t * pass_layer, const lv_draw_3d_callback_dsc_t * dsc)
{
    if(pass_layer == NULL || dsc == NULL || dsc->cb == NULL) return;

    LV_PROFILER_DRAW_BEGIN;

    lv_area_t area = pass_layer->buf_area;
    lv_draw_task_t * t = lv_draw_add_task(pass_layer, &area, LV_DRAW_TASK_TYPE_3D_CALLBACK);
    lv_memcpy(t->draw_dsc, dsc, sizeof(*dsc));
    lv_draw_finalize_task_creation(pass_layer, t);

    LV_PROFILER_DRAW_END;
}

#endif /*LV_USE_3D_DRAW_TASKS*/
