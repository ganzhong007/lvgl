#include "lv_draw_evgpu_c_r_t.h"
#if LV_USE_DRAW_EVGPU_C_R_T

#include "lv_draw_evgpu_c_r_t_private.h"
#include <math.h>

void lv_draw_evgpu_c_r_t_line(lv_draw_task_t * t, const lv_draw_line_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;
    uint32_t color = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->color, dsc->opa);

    float dx = dsc->p2.x - dsc->p1.x;
    float dy = dsc->p2.y - dsc->p1.y;
    float len = sqrtf(dx * dx + dy * dy);
    if(len < 0.001f) {
        LV_PROFILER_DRAW_END;
        return;
    }

    float nx = -dy / len * (dsc->width / 2.0f);
    float ny = dx / len * (dsc->width / 2.0f);

    float verts[8];
    verts[0] = dsc->p1.x + nx;  verts[1] = dsc->p1.y + ny;
    verts[2] = dsc->p1.x - nx;  verts[3] = dsc->p1.y - ny;
    verts[4] = dsc->p2.x + nx;  verts[5] = dsc->p2.y + ny;
    verts[6] = dsc->p2.x - nx;  verts[7] = dsc->p2.y - ny;

    lv_evgpu_c_r_t_gl_draw_triangle_strip_solid(&u->gl, verts, 4, color, 255);

    LV_PROFILER_DRAW_END;
}

#endif
