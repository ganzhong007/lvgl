#include "lv_draw_evgpu_c_r_t.h"
#if LV_USE_DRAW_EVGPU_C_R_T

#include "lv_draw_evgpu_c_r_t_private.h"

void lv_draw_evgpu_c_r_t_triangle(lv_draw_task_t * t, const lv_draw_triangle_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_area_t tri_area;
    tri_area.x1 = (int32_t)LV_MIN(LV_MIN(dsc->p[0].x, dsc->p[1].x), dsc->p[2].x);
    tri_area.y1 = (int32_t)LV_MIN(LV_MIN(dsc->p[0].y, dsc->p[1].y), dsc->p[2].y);
    tri_area.x2 = (int32_t)LV_MAX(LV_MAX(dsc->p[0].x, dsc->p[1].x), dsc->p[2].x);
    tri_area.y2 = (int32_t)LV_MAX(LV_MAX(dsc->p[0].y, dsc->p[1].y), dsc->p[2].y);

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, &tri_area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    uint32_t color = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->color, dsc->opa);

    lv_evgpu_c_r_t_gl_set_scissor(clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    float verts[6];
    verts[0] = (float)dsc->p[0].x;
    verts[1] = (float)dsc->p[0].y;
    verts[2] = (float)dsc->p[1].x;
    verts[3] = (float)dsc->p[1].y;
    verts[4] = (float)dsc->p[2].x;
    verts[5] = (float)dsc->p[2].y;

    lv_evgpu_c_r_t_gl_draw_triangles_solid(&u->gl, verts, 3, color, 255);

    lv_evgpu_c_r_t_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
