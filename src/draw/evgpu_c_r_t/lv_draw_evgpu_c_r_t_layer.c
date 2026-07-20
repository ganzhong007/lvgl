#include "lv_draw_evgpu_c_r_t_private.h"
#include "lv_evgpu_c_r_t_fbo.h"
#if LV_USE_DRAW_EVGPU_C_R_T

void lv_draw_evgpu_c_r_t_layer(lv_draw_task_t * t, const lv_draw_image_dsc_t * draw_dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_layer_t * src_layer = (lv_layer_t *)draw_dsc->src;
    if(!src_layer || !src_layer->user_data) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpu_c_r_t_fbo_t * fbo = (lv_evgpu_c_r_t_fbo_t *)src_layer->user_data;
    if(!fbo->tex) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    lv_evgpu_c_r_t_gl_draw_quad_tex(&u->gl,
                                     (float)coords->x1, (float)coords->y1,
                                     (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                     0.0f, 0.0f, 1.0f, 1.0f,
                                     fbo->tex,
                                     0, 0,
                                     draw_dsc->opa);

    lv_evgpu_c_r_t_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
