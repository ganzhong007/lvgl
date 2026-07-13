/**
 * @file lv_draw_evgpu_3d_viewport.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU && LV_USE_3D_DRAW_TASKS

#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_viewport.h"
#include "lv_evgpu_3d_pass.h"
#include "lv_evgpu_utils.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_evgpu_3d_viewport(lv_draw_task_t * t, const lv_draw_3d_viewport_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;
    lv_layer_t * pass_layer = dsc->pass_layer;
    lv_3d_pass_t * pass = lv_evgpu_3d_pass_from_layer(pass_layer);
    if(pass == NULL || pass->fbo.color_tex == 0) {
        LV_LOG_WARN("3D viewport resolve: pass FBO not ready");
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpu_end_frame(u);

    lv_layer_t * layer = t->target_layer;
    int32_t layer_w = lv_area_get_width(&layer->buf_area);
    int32_t layer_h = lv_area_get_height(&layer->buf_area);

    lv_area_t dest_area = *coords;
    lv_area_move(&dest_area, -layer->buf_area.x1, -layer->buf_area.y1);

    lv_area_t clip_area = t->clip_area;
    lv_area_move(&clip_area, -layer->buf_area.x1, -layer->buf_area.y1);

    lv_opengles_reinit_state();
    lv_opengles_viewport(0, 0, layer_w, layer_h);

    lv_opengles_render_params_t params;
    lv_opengles_render_params_init(&params);
    params.texture = pass->fbo.color_tex;
    params.texture_area = &dest_area;
    params.opa = dsc->opa;
    params.disp_w = layer_w;
    params.disp_h = layer_h;
    params.texture_clip_area = &clip_area;
    /* FBO pass is rendered as GL RGBA (not NanoVG BGR); match gltf display orientation. */
    params.h_flip = false;
    params.v_flip = true;
    params.rb_swap = false;
#if LV_DRAW_TRANSFORM_USE_MATRIX
    params.matrix = &layer->matrix;
#endif
    lv_opengles_render(&params);

    LV_PROFILER_DRAW_END;
}

#endif /* LV_USE_DRAW_EVGPU && LV_USE_3D_DRAW_TASKS */
