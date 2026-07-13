/**
 * @file lv_draw_evgpu_box_shadow.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU

#include "lv_evgpu_utils.h"
#include "lv_evgpu_blur_kawase.h"
#include "lv_evgpu_fbo_pool.h"
#include "lv_evgpu_fbo_cache.h"
#include "../../libs/evgpu/evgpu_evgr_gl_utils.h"

/*********************
*      DEFINES
*********************/

#define EVGPU_BOX_SHADOW_BLUR_THRESHOLD 256

/**********************
*  STATIC PROTOTYPES
**********************/

static bool box_shadow_use_blur_pipeline(const lv_draw_box_shadow_dsc_t * dsc);
static void box_shadow_blur_pipeline(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc,
                                     const lv_area_t * core_area, const lv_area_t * shadow_area);

/**********************
*   GLOBAL FUNCTIONS
**********************/

void lv_draw_evgpu_box_shadow(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    /*Calculate the rectangle which is blurred to get the shadow in `shadow_area`*/
    lv_area_t core_area;
    core_area.x1 = coords->x1  + dsc->ofs_x - dsc->spread;
    core_area.x2 = coords->x2  + dsc->ofs_x + dsc->spread;
    core_area.y1 = coords->y1  + dsc->ofs_y - dsc->spread;
    core_area.y2 = coords->y2  + dsc->ofs_y + dsc->spread;

    /*Calculate the bounding box of the shadow*/
    lv_area_t shadow_area;
    shadow_area.x1 = core_area.x1 - dsc->width / 2 - 1;
    shadow_area.x2 = core_area.x2 + dsc->width / 2 + 1;
    shadow_area.y1 = core_area.y1 - dsc->width / 2 - 1;
    shadow_area.y2 = core_area.y2 + dsc->width / 2 + 1;

    /*Get clipped draw area which is the real draw area.
     *It is always the same or inside `shadow_area`*/
    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, &shadow_area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    if(box_shadow_use_blur_pipeline(dsc)) {
        box_shadow_blur_pipeline(t, dsc, &core_area, &shadow_area);
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;

    const EVGRcolor icol = lv_evgpu_color_convert(dsc->color, dsc->opa);
    const EVGRcolor ocol = lv_evgpu_color_convert(lv_color_black(), 0);

    const int32_t core_w = lv_area_get_width(&core_area);
    const int32_t core_h = lv_area_get_height(&core_area);
    const int32_t shadow_w = lv_area_get_width(&shadow_area);
    const int32_t shadow_h = lv_area_get_height(&shadow_area);

    EVGRpaint paint = evgrBoxGradient(
                         u->evgr,
                         core_area.x1, core_area.y1,
                         core_w, core_h,
                         dsc->radius, dsc->width, icol, ocol);

    evgrBeginPath(u->evgr);
    lv_evgpu_path_append_rect(u->evgr, shadow_area.x1, shadow_area.y1, shadow_w, shadow_h, dsc->radius);
    evgrFillPaint(u->evgr, paint);
    evgrFill(u->evgr);

    LV_PROFILER_DRAW_END;
}

/**********************
*   STATIC FUNCTIONS
**********************/

static bool box_shadow_use_blur_pipeline(const lv_draw_box_shadow_dsc_t * dsc)
{
    return dsc->width > EVGPU_BOX_SHADOW_BLUR_THRESHOLD
           && lv_evgpu_blur_kawase_ready()
           && lv_evgpu_fbo_pool_is_ok();
}

static void box_shadow_blur_pipeline(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc,
                                     const lv_area_t * core_area, const lv_area_t * shadow_area)
{
    static bool pipeline_logged;
    if(!pipeline_logged) {
        pipeline_logged = true;
        LV_LOG_INFO("EVGPU box_shadow blur pipeline ready");
    }

    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;
    lv_layer_t * layer = t->target_layer;

    const int32_t core_w = lv_area_get_width(core_area);
    const int32_t core_h = lv_area_get_height(core_area);

    evgrBeginPath(u->evgr);
    lv_evgpu_path_append_rect(u->evgr, core_area->x1, core_area->y1, core_w, core_h, dsc->radius);
    evgrFillColor(u->evgr, evgrRGBA(255, 255, 255, dsc->opa));
    evgrFill(u->evgr);

    lv_evgpu_end_frame(u);

    lv_area_t blur_clip;
    if(!lv_area_intersect(&blur_clip, shadow_area, &t->clip_area)) return;
    if(!lv_area_intersect(&blur_clip, &blur_clip, &layer->buf_area)) return;

    const int blur_w = lv_area_get_width(&blur_clip);
    const int blur_h = lv_area_get_height(&blur_clip);
    if(blur_w <= 0 || blur_h <= 0) return;

    const int layer_h = lv_area_get_height(&layer->buf_area);
    const int rel_x = blur_clip.x1 - layer->buf_area.x1;
    const int rel_y_top = blur_clip.y1 - layer->buf_area.y1;
    const int gl_y = layer_h - rel_y_top - blur_h;

    const bool is_root = (layer->user_data == NULL);
    EVGRLUframebuffer * src_fb = NULL;
    if(!is_root) {
        src_fb = lv_evgpu_fbo_cache_entry_to_fb(layer->user_data);
        if(src_fb == NULL) return;
    }

    const EVGRcolor recolor = lv_evgpu_color_convert(dsc->color, dsc->opa);
    const int blur_radius = dsc->width / 2;
    const int ret = lv_evgpu_blur_kawase_region(u, src_fb, rel_x, gl_y, blur_w, blur_h, blur_radius, &recolor);
    if(ret != 0) {
        LV_LOG_WARN("EVGPU box_shadow kawase blur failed (ret=%d)", ret);
    }
}

#endif /* LV_USE_DRAW_EVGPU */
