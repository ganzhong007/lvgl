/**
 * @file lv_draw_g100_3d_scene.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_g100_private.h"

#if LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS && LV_USE_GLTF

#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../include/lvgl/draw/lv_draw_3d_scene.h"
#include "../../include/lvgl/widgets/lv_gltf.h"
#include "lv_g100_3d_pass.h"
#include "lv_g100_utils.h"

/*********************
 *      DEFINES
 *********************/

static bool s_scene_ready;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_g100_3d_scene_init(void)
{
    if(s_scene_ready) return;
    s_scene_ready = true;
    LV_LOG_INFO("G100 3D scene ready (SCENE pass + gltf dispatch)");
}

void lv_draw_g100_3d_scene_deinit(void)
{
    s_scene_ready = false;
}

void lv_draw_g100_3d_scene(lv_draw_task_t * t, const lv_draw_3d_scene_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    if(!s_scene_ready) lv_draw_g100_3d_scene_init();
    if(dsc == NULL || dsc->scene_id == NULL) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_obj_t * viewer_obj = (lv_obj_t *)dsc->scene_id;
    lv_draw_g100_unit_t * u = (lv_draw_g100_unit_t *)t->draw_unit;
    lv_layer_t * pass_layer = t->target_layer;
    int32_t w, h;
    lv_g100_3d_pass_fbo_t * fbo;

    lv_g100_end_frame(u);
    lv_opengles_reinit_state();

    if(!lv_g100_3d_pass_bind_fbo(pass_layer, &w, &h, &fbo)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    uint32_t tex = lv_gltf_render_scene(viewer_obj);
    if(tex == 0) {
        lv_g100_3d_pass_unbind_fbo();
        LV_PROFILER_DRAW_END;
        return;
    }

    if(!lv_g100_3d_pass_bind_fbo(pass_layer, &w, &h, &fbo)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    bool h_flip = false;
    bool v_flip = true;
    lv_gltf_get_texture_flip(viewer_obj, &h_flip, &v_flip);

    lv_area_t area = {0, 0, w - 1, h - 1};
    lv_opengles_render_params_t params;
    lv_opengles_render_params_init(&params);
    params.texture = tex;
    params.texture_area = &area;
    params.texture_clip_area = &area;
    params.disp_w = w;
    params.disp_h = h;
    params.opa = LV_OPA_COVER;
    params.h_flip = h_flip;
    params.v_flip = v_flip;
    params.rb_swap = true;
    lv_opengles_render(&params);

    lv_g100_3d_pass_unbind_fbo();
    lv_opengles_reinit_state();

    LV_PROFILER_DRAW_END;
}

#endif /* LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS && LV_USE_GLTF */
