#include "lv_draw_evgpuganesh_private.h"
#if LV_USE_DRAW_EVGPUGANESH

void lv_draw_evgpuganesh_box_shadow(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpuganesh_unit_t * u = (lv_draw_evgpuganesh_unit_t *)t->draw_unit;

    lv_area_t shadow_area;
    shadow_area.x1 = coords->x1 - dsc->spread - dsc->width;
    shadow_area.x2 = coords->x2 + dsc->spread + dsc->width;
    shadow_area.y1 = coords->y1 - dsc->spread - dsc->width;
    shadow_area.y2 = coords->y2 + dsc->spread + dsc->width;

    lv_area_move(&shadow_area, dsc->ofs_x, dsc->ofs_y);

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, &shadow_area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpuganesh_gl_set_scissor(clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    lv_evgpuganesh_gl_draw_quad_solid(&u->gl,
                                       (float)shadow_area.x1, (float)shadow_area.y1,
                                       (float)(shadow_area.x2 + 1), (float)(shadow_area.y2 + 1),
                                       lv_evgpuganesh_color_to_gl_alpha(dsc->color, dsc->opa),
                                       LV_OPA_COVER);

    lv_evgpuganesh_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
