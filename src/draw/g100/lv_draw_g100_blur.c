/**
 * @file lv_draw_g100_blur.c
 *
 * NanoVG blur draw task handler. Translates LVGL blur tasks into
 * nvgluBlurRegion() calls — the actual shader/FBO logic lives in
 * nanovg_gl_utils.h for backend portability.
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_g100_private.h"

#if LV_USE_DRAW_G100

#include "lv_g100_utils.h"
#include "lv_g100_blur_kawase.h"
#include "lv_g100_fbo_cache.h"
#include "../../libs/nanovg/nanovg_gl_utils.h"

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

#define G100_BLUR_KAWASE_THRESHOLD 256

void lv_draw_g100_blur_init(lv_draw_g100_unit_t * u)
{
    LV_ASSERT_NULL(u);
    u->blur_state = nvgluCreateBlurState();
    if(u->blur_state == NULL) {
        LV_LOG_WARN("nanovg blur: failed to create blur state (FBO not supported?)");
    }
    lv_g100_blur_kawase_init(u);
}

void lv_draw_g100_blur_deinit(lv_draw_g100_unit_t * u)
{
    LV_ASSERT_NULL(u);
    lv_g100_blur_kawase_deinit(u);
    if(u->blur_state) {
        nvgluDeleteBlurState(u->blur_state, u->vg);
        u->blur_state = NULL;
    }
}

void lv_draw_g100_blur(lv_draw_task_t * t, const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords)
{
    if(dsc->blur_radius <= 0) return;

    LV_PROFILER_DRAW_BEGIN;

    lv_draw_g100_unit_t * u = (lv_draw_g100_unit_t *)t->draw_unit;
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
    NVGLUframebuffer * src_fb = NULL;
    if(!is_root) {
        src_fb = lv_g100_fbo_cache_entry_to_fb(layer->user_data);
        if(src_fb == NULL) {
            LV_PROFILER_DRAW_END;
            return;
        }
    }

    /* Flush pending NanoVG draws before raw GL operations */
    lv_g100_end_frame(u);

    NVGcolor recolor = nvgRGBA(0, 0, 0, 0);
    if(dsc->base.drop_shadow_opa > 0) {
        recolor = nvgRGBA(dsc->base.drop_shadow_color.red,
                          dsc->base.drop_shadow_color.green,
                          dsc->base.drop_shadow_color.blue,
                          255);
    }

    if(dsc->blur_radius > G100_BLUR_KAWASE_THRESHOLD && lv_g100_blur_kawase_ready()) {
        const int ret = lv_g100_blur_kawase_region(u, src_fb, rel_x, gl_y, blur_w, blur_h,
                                                   dsc->blur_radius, &recolor);
        if(ret != 0) {
            LV_LOG_WARN("G100 kawase blur failed (ret=%d)", ret);
        }
        LV_PROFILER_DRAW_END;
        return;
    }

    NVGLUblurState * state = u->blur_state;
    if(state == NULL) {
        LV_LOG_WARN("nanovg blur: state not initialized (FBO not supported?), skipping");
        LV_PROFILER_DRAW_END;
        return;
    }

    /* Build blur params */
    NVGLUblurParams params;
    params.radius  = dsc->blur_radius;
    params.quality = (dsc->quality == LV_BLUR_QUALITY_SPEED)
                     ? NVGLU_BLUR_QUALITY_SPEED : NVGLU_BLUR_QUALITY_NORMAL;
    params.recolor = recolor;

    /* Call the NanoVG blur utility */
    int ret = nvgluBlurRegion(state, u->vg, src_fb, rel_x, gl_y, blur_w, blur_h, &params);
    if(ret != 0) {
        LV_LOG_WARN("nanovg blur: nvgluBlurRegion failed (ret=%d)", ret);
    }

    LV_PROFILER_DRAW_END;
}

#endif /* LV_USE_DRAW_G100 */
