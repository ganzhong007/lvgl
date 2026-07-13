/**
 * @file lv_draw_evgpu_blur.c
 *
 * NanoVG blur draw task handler. Translates LVGL blur tasks into
 * evgrluBlurRegion() calls — the actual shader/FBO logic lives in
 * nanovg_gl_utils.h for backend portability.
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU

#include "lv_evgpu_utils.h"
#include "lv_evgpu_blur_kawase.h"
#include "lv_evgpu_fbo_cache.h"
#include "../../libs/evgpu/evgpu_evgr_gl_utils.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

#define EVGPU_BLUR_KAWASE_THRESHOLD 256

void lv_draw_evgpu_blur_init(lv_draw_evgpu_unit_t * u)
{
    LV_ASSERT_NULL(u);
    u->blur_state = evgrluCreateBlurState();
    if(u->blur_state == NULL) {
        LV_LOG_WARN("nanovg blur: failed to create blur state (FBO not supported?)");
    }
    lv_evgpu_blur_kawase_init(u);
}

void lv_draw_evgpu_blur_deinit(lv_draw_evgpu_unit_t * u)
{
    LV_ASSERT_NULL(u);
    lv_evgpu_blur_kawase_deinit(u);
    if(u->blur_state) {
        evgrluDeleteBlurState(u->blur_state, u->evgr);
        u->blur_state = NULL;
    }
}

void lv_draw_evgpu_blur(lv_draw_task_t * t, const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords)
{
    if(dsc->blur_radius <= 0) return;

    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;
    lv_layer_t * layer = t->target_layer;

    /* Clip the blur area to clip_area and layer extent */
    lv_area_t clip;
    if(!lv_area_intersect(&clip, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }
    if(!lv_area_intersect(&clip, &clip, &layer->buf_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    const int blur_w = lv_area_get_width(&clip);
    const int blur_h = lv_area_get_height(&clip);
    if(blur_w <= 0 || blur_h <= 0) {
        LV_PROFILER_DRAW_END;
        return;
    }

    /* Convert LVGL top-down coords to GL bottom-up coords */
    const int layer_h   = lv_area_get_height(&layer->buf_area);
    const int rel_x     = clip.x1 - layer->buf_area.x1;
    const int rel_y_top = clip.y1 - layer->buf_area.y1;
    const int gl_y      = layer_h - rel_y_top - blur_h;

    const bool is_root = (layer->user_data == NULL);
    EVGRLUframebuffer * src_fb = NULL;
    if(!is_root) {
        src_fb = lv_evgpu_fbo_cache_entry_to_fb(layer->user_data);
        if(src_fb == NULL) {
            LV_PROFILER_DRAW_END;
            return;
        }
    }

    /* Flush pending NanoVG draws before raw GL operations */
    lv_evgpu_end_frame(u);

    EVGRcolor recolor = evgrRGBA(0, 0, 0, 0);
    if(dsc->base.drop_shadow_opa > 0) {
        recolor = evgrRGBA(dsc->base.drop_shadow_color.red,
                          dsc->base.drop_shadow_color.green,
                          dsc->base.drop_shadow_color.blue,
                          255);
    }

    if(dsc->blur_radius > EVGPU_BLUR_KAWASE_THRESHOLD && lv_evgpu_blur_kawase_ready()) {
        const int ret = lv_evgpu_blur_kawase_region(u, src_fb, rel_x, gl_y, blur_w, blur_h,
                                                   dsc->blur_radius, &recolor);
        if(ret != 0) {
            LV_LOG_WARN("EVGPU kawase blur failed (ret=%d)", ret);
        }
        LV_PROFILER_DRAW_END;
        return;
    }

    EVGRLUblurState * state = u->blur_state;
    if(state == NULL) {
        LV_LOG_WARN("nanovg blur: state not initialized (FBO not supported?), skipping");
        LV_PROFILER_DRAW_END;
        return;
    }

    /* Build blur params */
    EVGRLUblurParams params;
    params.radius  = dsc->blur_radius;
    params.quality = (dsc->quality == LV_BLUR_QUALITY_SPEED)
                     ? EVGRLU_BLUR_QUALITY_SPEED : EVGRLU_BLUR_QUALITY_NORMAL;
    params.recolor = recolor;

    /* Call the NanoVG blur utility */
    int ret = evgrluBlurRegion(state, u->evgr, src_fb, rel_x, gl_y, blur_w, blur_h, &params);
    if(ret != 0) {
        LV_LOG_WARN("nanovg blur: evgrluBlurRegion failed (ret=%d)", ret);
    }

    LV_PROFILER_DRAW_END;
}

#endif /* LV_USE_DRAW_EVGPU */
