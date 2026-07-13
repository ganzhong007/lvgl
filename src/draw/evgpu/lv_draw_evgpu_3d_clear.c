/**
 * @file lv_draw_evgpu_3d_clear.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU && LV_USE_3D_DRAW_TASKS

#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../include/lvgl/draw/lv_draw_3d_clear.h"
#include "lv_evgpu_3d_pass.h"
#include "lv_evgpu_utils.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_evgpu_3d_clear(lv_draw_task_t * t, const lv_draw_3d_clear_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;
    LV_UNUSED(t);

    lv_layer_t * pass_layer = t->target_layer;
    lv_3d_pass_t * pass = lv_evgpu_3d_pass_from_layer(pass_layer);
    if(pass == NULL) {
        LV_LOG_WARN("3D clear on non-pass layer");
        LV_PROFILER_DRAW_END;
        return;
    }

    int32_t w = lv_area_get_width(&pass_layer->buf_area);
    int32_t h = lv_area_get_height(&pass_layer->buf_area);
    if(lv_evgpu_3d_pass_ensure(pass, w, h) != LV_RESULT_OK) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;
    lv_evgpu_end_frame(u);
    lv_opengles_reinit_state();

    lv_evgpu_3d_pass_fbo_t * fbo = &pass->fbo;
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer));
    GL_CALL(glViewport(0, 0, w, h));

    lv_color32_t c = dsc->color;
    lv_opa_t opa = dsc->opa == LV_OPA_COVER ? c.alpha : LV_OPA_MIX2(c.alpha, dsc->opa);
    GL_CALL(glClearColor(c.red / 255.0f, c.green / 255.0f, c.blue / 255.0f, opa / 255.0f));

    GLbitfield mask = GL_COLOR_BUFFER_BIT;
    if(dsc->clear_depth) {
        GL_CALL(glClearDepthf(dsc->depth_value));
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    GL_CALL(glClear(mask));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    LV_PROFILER_DRAW_END;
}

#endif /* LV_USE_DRAW_EVGPU && LV_USE_3D_DRAW_TASKS */
