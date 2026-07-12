/**
 * @file lv_draw_3d_viewport.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_viewport.h"

#if LV_USE_3D_DRAW_TASKS

#if LV_USE_DRAW_G100
#include "g100/lv_g100_3d_pass.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_3d_viewport_dsc_init(lv_draw_3d_viewport_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_3d_viewport_dsc_t));
    dsc->base.dsc_size = sizeof(lv_draw_3d_viewport_dsc_t);
    dsc->resolve_mode = LV_3D_RESOLVE_TO_LAYER;
    dsc->color_format = LV_COLOR_FORMAT_ARGB8888;
    dsc->depth_bits = 16;
    dsc->opa = LV_OPA_COVER;
}

lv_draw_3d_viewport_dsc_t * lv_draw_task_get_3d_viewport_dsc(lv_draw_task_t * task)
{
    return task->type == LV_DRAW_TASK_TYPE_3D_VIEWPORT ? (lv_draw_3d_viewport_dsc_t *)task->draw_dsc : NULL;
}

lv_layer_t * lv_draw_3d_pass_layer_create(lv_layer_t * parent_layer, const lv_area_t * area)
{
#if LV_USE_DRAW_G100
    lv_layer_t * pass = lv_draw_layer_create(parent_layer, LV_COLOR_FORMAT_ARGB8888, area);
    if(pass == NULL) return NULL;

    lv_3d_pass_layer_ud_t * ud = lv_malloc_zeroed(sizeof(lv_3d_pass_layer_ud_t));
    if(ud == NULL) {
        lv_free(pass);
        return NULL;
    }

    ud->magic = LV_3D_PASS_LAYER_MAGIC;
    lv_3d_camera_init(&ud->camera);
    ud->camera_valid = true;
    pass->user_data = ud;
    return pass;
#else
    LV_UNUSED(parent_layer);
    LV_UNUSED(area);
    return NULL;
#endif
}

void lv_draw_3d_viewport(lv_layer_t * layer, const lv_draw_3d_viewport_dsc_t * dsc, const lv_area_t * coords)
{
    if(dsc->pass_layer == NULL) {
        LV_LOG_WARN("3D viewport: pass_layer is NULL");
        return;
    }

    LV_PROFILER_DRAW_BEGIN;

    lv_draw_task_t * t = lv_draw_add_task(layer, coords, LV_DRAW_TASK_TYPE_3D_VIEWPORT);
    lv_memcpy(t->draw_dsc, dsc, sizeof(*dsc));
    t->state = LV_DRAW_TASK_STATE_BLOCKED;
    lv_draw_finalize_task_creation(layer, t);

    LV_PROFILER_DRAW_END;
}

void lv_draw_3d_viewport_end(lv_layer_t * pass_layer)
{
    if(pass_layer == NULL) return;
    pass_layer->all_tasks_added = true;
}

void lv_draw_3d_pass_layer_destroy(lv_layer_t * pass_layer, lv_display_t * disp)
{
#if LV_USE_DRAW_G100
    lv_g100_3d_pass_layer_destroy(pass_layer, disp);
#else
    LV_UNUSED(pass_layer);
    LV_UNUSED(disp);
#endif
}

void lv_draw_3d_pass_set_camera(lv_layer_t * pass_layer, const lv_3d_camera_t * camera)
{
#if LV_USE_DRAW_G100
    lv_g100_3d_pass_set_camera(pass_layer, camera);
#else
    LV_UNUSED(pass_layer);
    LV_UNUSED(camera);
#endif
}

#endif /*LV_USE_3D_DRAW_TASKS*/
