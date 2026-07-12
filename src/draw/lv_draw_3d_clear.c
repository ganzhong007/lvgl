/**
 * @file lv_draw_3d_clear.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_clear.h"

#if LV_USE_3D_DRAW_TASKS

/*********************
 *      DEFINES
 *********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_3d_clear_dsc_init(lv_draw_3d_clear_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_3d_clear_dsc_t));
    dsc->base.dsc_size = sizeof(lv_draw_3d_clear_dsc_t);
    dsc->color = lv_color32_make(0, 0, 0, 0);
    dsc->opa = LV_OPA_COVER;
    dsc->clear_depth = true;
    dsc->depth_value = 1.0f;
}

lv_draw_3d_clear_dsc_t * lv_draw_task_get_3d_clear_dsc(lv_draw_task_t * task)
{
    return task->type == LV_DRAW_TASK_TYPE_3D_CLEAR ? (lv_draw_3d_clear_dsc_t *)task->draw_dsc : NULL;
}

void lv_draw_3d_clear(lv_layer_t * pass_layer, const lv_draw_3d_clear_dsc_t * dsc)
{
    if(pass_layer == NULL) {
        LV_LOG_WARN("3D clear: pass_layer is NULL");
        return;
    }

    LV_PROFILER_DRAW_BEGIN;

    lv_area_t area = pass_layer->buf_area;
    lv_draw_task_t * t = lv_draw_add_task(pass_layer, &area, LV_DRAW_TASK_TYPE_3D_CLEAR);
    lv_memcpy(t->draw_dsc, dsc, sizeof(*dsc));
    lv_draw_finalize_task_creation(pass_layer, t);

    LV_PROFILER_DRAW_END;
}

#endif /*LV_USE_3D_DRAW_TASKS*/
