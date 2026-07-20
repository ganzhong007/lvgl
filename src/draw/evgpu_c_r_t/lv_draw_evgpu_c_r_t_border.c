#include "lv_draw_evgpu_c_r_t_private.h"
#if LV_USE_DRAW_EVGPU_C_R_T

static int32_t clamp_radius(int32_t radius, int32_t w, int32_t h)
{
    if(radius <= 0) return 0;
    int32_t max_r = LV_MIN(w, h) / 2;
    if(radius > max_r) return max_r;
    return radius;
}

void lv_draw_evgpu_c_r_t_border(lv_draw_task_t * t, const lv_draw_border_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    uint32_t color = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->color, dsc->opa);
    int32_t x1 = coords->x1;
    int32_t y1 = coords->y1;
    int32_t x2 = coords->x2;
    int32_t y2 = coords->y2;
    int32_t bw = dsc->width;
    if(bw < 1) {
        lv_evgpu_c_r_t_gl_disable_scissor();
        LV_PROFILER_DRAW_END;
        return;
    }

    const int32_t aw = lv_area_get_width(coords);
    const int32_t ah = lv_area_get_height(coords);
    const int32_t radius = clamp_radius(dsc->radius, aw, ah);
    const bool full_sides = (dsc->side == LV_BORDER_SIDE_FULL) ||
                            ((dsc->side & LV_BORDER_SIDE_TOP) &&
                             (dsc->side & LV_BORDER_SIDE_BOTTOM) &&
                             (dsc->side & LV_BORDER_SIDE_LEFT) &&
                             (dsc->side & LV_BORDER_SIDE_RIGHT));

    if(radius > 0 && full_sides) {
        lv_evgpu_c_r_t_gl_draw_round_rect(&u->gl,
                                           (float)x1, (float)y1,
                                           (float)(x2 + 1), (float)(y2 + 1),
                                           (float)radius, (float)bw,
                                           color, 255);
        lv_evgpu_c_r_t_gl_flush(&u->gl);
        lv_evgpu_c_r_t_gl_disable_scissor();
        LV_PROFILER_DRAW_END;
        return;
    }

    /* Square ring for sharp / partial-side borders. */
    if(dsc->side & LV_BORDER_SIDE_TOP) {
        lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                           (float)x1, (float)y1,
                                           (float)(x2 + 1), (float)(y1 + bw),
                                           color, 255);
    }
    if(dsc->side & LV_BORDER_SIDE_BOTTOM) {
        lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                           (float)x1, (float)(y2 - bw + 1),
                                           (float)(x2 + 1), (float)(y2 + 1),
                                           color, 255);
    }
    if(dsc->side & LV_BORDER_SIDE_LEFT) {
        lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                           (float)x1, (float)(y1 + bw),
                                           (float)(x1 + bw), (float)(y2 - bw + 1),
                                           color, 255);
    }
    if(dsc->side & LV_BORDER_SIDE_RIGHT) {
        lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                           (float)(x2 - bw + 1), (float)(y1 + bw),
                                           (float)(x2 + 1), (float)(y2 - bw + 1),
                                           color, 255);
    }

    lv_evgpu_c_r_t_gl_flush(&u->gl);
    lv_evgpu_c_r_t_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
