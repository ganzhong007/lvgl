#include "lv_draw_evgpuganesh.h"
#if LV_USE_DRAW_EVGPUGANESH

#include "lv_draw_evgpuganesh_private.h"

void lv_draw_evgpuganesh_mask_rect(lv_draw_task_t * t, const lv_draw_mask_rect_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;
    LV_UNUSED(dsc);

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, &dsc->area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpuganesh_gl_set_scissor(clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    LV_PROFILER_DRAW_END;
}

#endif
