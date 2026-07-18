#include "lv_draw_evgpuganesh_private.h"
#if LV_USE_DRAW_EVGPUGANESH

void lv_draw_evgpuganesh_border(lv_draw_task_t * t, const lv_draw_border_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpuganesh_unit_t * u = (lv_draw_evgpuganesh_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpuganesh_gl_set_scissor(clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    uint32_t color = lv_evgpuganesh_color_to_gl_alpha(dsc->color, dsc->opa);
    int32_t x1 = coords->x1;
    int32_t y1 = coords->y1;
    int32_t x2 = coords->x2;
    int32_t y2 = coords->y2;
    int32_t w = dsc->width;

    if(dsc->radius == 0 && dsc->side == LV_BORDER_SIDE_FULL) {
        lv_evgpuganesh_gl_draw_quad_solid(&u->gl,
                                           (float)x1, (float)y1,
                                           (float)(x2 + 1), (float)(y1 + w),
                                           color, 255);

        lv_evgpuganesh_gl_draw_quad_solid(&u->gl,
                                           (float)x1, (float)(y2 - w + 1),
                                           (float)(x2 + 1), (float)(y2 + 1),
                                           color, 255);

        lv_evgpuganesh_gl_draw_quad_solid(&u->gl,
                                           (float)x1, (float)(y1 + w),
                                           (float)(x1 + w), (float)(y2 - w + 1),
                                           color, 255);

        lv_evgpuganesh_gl_draw_quad_solid(&u->gl,
                                           (float)(x2 - w + 1), (float)(y1 + w),
                                           (float)(x2 + 1), (float)(y2 - w + 1),
                                           color, 255);
    }
    else {
        lv_evgpuganesh_gl_draw_quad_solid(&u->gl,
                                           (float)x1, (float)y1,
                                           (float)(x2 + 1), (float)(y2 + 1),
                                           color, 255);
    }

    lv_evgpuganesh_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
