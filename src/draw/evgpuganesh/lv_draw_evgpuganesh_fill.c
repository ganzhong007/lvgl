#include "lv_draw_evgpuganesh_private.h"
#if LV_USE_DRAW_EVGPUGANESH

void lv_draw_evgpuganesh_gradient(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords);

void lv_draw_evgpuganesh_fill(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpuganesh_unit_t * u = (lv_draw_evgpuganesh_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    if(dsc->grad.dir != LV_GRAD_DIR_NONE) {
        lv_draw_evgpuganesh_gradient(t, dsc, coords);
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpuganesh_gl_set_scissor(clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    lv_evgpuganesh_gl_draw_quad_solid(&u->gl,
                                       (float)coords->x1, (float)coords->y1,
                                       (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                       lv_evgpuganesh_color_to_gl_alpha(dsc->color, dsc->opa),
                                       255);

    lv_evgpuganesh_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
