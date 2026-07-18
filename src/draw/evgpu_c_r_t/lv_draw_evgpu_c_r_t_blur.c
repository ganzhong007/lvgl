#include "lv_draw_evgpu_c_r_t_private.h"
#if LV_USE_DRAW_EVGPU_C_R_T

void lv_draw_evgpu_c_r_t_blur(lv_draw_task_t * t, const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    LV_UNUSED(dsc);

    lv_evgpu_c_r_t_gl_set_scissor(clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                       (float)coords->x1, (float)coords->y1,
                                       (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                       0x000000BB, 80);

    lv_evgpu_c_r_t_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
