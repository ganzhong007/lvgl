/**
 * @file lv_draw_evgpu_3d_cb.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU && LV_USE_3D_DRAW_TASKS

#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../include/lvgl/draw/lv_draw_3d_callback.h"
#include "lv_evgpu_3d_pass.h"
#include "lv_evgpu_utils.h"

/*********************
 *      DEFINES
 *********************/

static bool s_cb_ready;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_evgpu_3d_cb_init(void)
{
    if(s_cb_ready) return;
    s_cb_ready = true;
    LV_LOG_INFO("EVGPU 3D callback ready (3D_CALLBACK pass hook)");
}

void lv_draw_evgpu_3d_cb(lv_draw_task_t * t, const lv_draw_3d_callback_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    if(!s_cb_ready) lv_draw_evgpu_3d_cb_init();
    if(dsc == NULL || dsc->cb == NULL) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;
    lv_layer_t * pass_layer = t->target_layer;
    int32_t w, h;
    lv_evgpu_3d_pass_fbo_t * fbo;

    lv_evgpu_end_frame(u);
    lv_opengles_reinit_state();

    if(!lv_evgpu_3d_pass_bind_fbo(pass_layer, &w, &h, &fbo)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    GL_CALL(glEnable(GL_DEPTH_TEST));

    const lv_3d_camera_t * cam = lv_draw_3d_pass_get_camera(pass_layer);
    const float * mvp = lv_draw_3d_pass_get_view_proj(pass_layer);
    dsc->cb(pass_layer, cam, mvp, dsc->user_data);

    lv_evgpu_3d_pass_unbind_fbo();
    lv_opengles_reinit_state();

    LV_PROFILER_DRAW_END;
}

#endif /* LV_USE_DRAW_EVGPU && LV_USE_3D_DRAW_TASKS */
