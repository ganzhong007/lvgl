#include "lv_draw_evgpu_c_r_t_private.h"
#if LV_USE_DRAW_EVGPU_C_R_T

void lv_draw_evgpu_c_r_t_gradient(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords);

static int32_t clamp_radius(int32_t radius, int32_t w, int32_t h)
{
    if(radius <= 0) return 0;
    int32_t max_r = LV_MIN(w, h) / 2;
    if(radius > max_r) return max_r;
    return radius;
}

void lv_draw_evgpu_c_r_t_fill(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    if(dsc->grad.dir != LV_GRAD_DIR_NONE) {
        lv_draw_evgpu_c_r_t_gradient(t, dsc, coords);
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    const int32_t w = lv_area_get_width(coords);
    const int32_t h = lv_area_get_height(coords);
    const int32_t radius = clamp_radius(dsc->radius, w, h);
    const uint32_t color = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->color, dsc->opa);

    if(radius > 0) {
        lv_evgpu_c_r_t_gl_draw_round_rect(&u->gl,
                                           (float)coords->x1, (float)coords->y1,
                                           (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                           (float)radius, 0.f,
                                           color, 255);
    }
    else {
        lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                           (float)coords->x1, (float)coords->y1,
                                           (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                           color, 255);
    }
    lv_evgpu_c_r_t_gl_flush(&u->gl);

    lv_evgpu_c_r_t_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
