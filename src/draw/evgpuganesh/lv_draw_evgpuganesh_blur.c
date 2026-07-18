#include "lv_draw_evgpuganesh_private.h"
#if LV_USE_DRAW_EVGPUGANESH

void lv_draw_evgpuganesh_blur(lv_draw_task_t * t, const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpuganesh_unit_t * u = (lv_draw_evgpuganesh_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    LV_UNUSED(dsc);

    lv_evgpuganesh_gl_set_scissor(clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    lv_evgpuganesh_gl_draw_quad_solid(&u->gl,
                                       (float)coords->x1, (float)coords->y1,
                                       (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                       0x000000BB, 80);

    lv_evgpuganesh_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
