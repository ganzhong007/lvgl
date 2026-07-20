#include "lv_draw_evgpu_c_r_t.h"
#if LV_USE_DRAW_EVGPU_C_R_T

#include "lv_draw_evgpu_c_r_t_private.h"
#include <math.h>

void lv_draw_evgpu_c_r_t_arc(lv_draw_task_t * t, const lv_draw_arc_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    uint32_t color = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->color, dsc->opa);

    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    float cx = (float)dsc->center.x;
    float cy = (float)dsc->center.y;
    float radius = (float)dsc->radius;
    float half_w = (float)dsc->width * 0.5f;
    float inner_r = radius - half_w;
    float outer_r = radius + half_w;

    if(inner_r < 0.0f) inner_r = 0.0f;

    double start = (double)dsc->start_angle;
    double end = (double)dsc->end_angle;

    double sweep = end - start;
    while(sweep <= 0.0) sweep += 360.0;
    while(sweep > 360.0) sweep -= 360.0;

    if(sweep < 0.001) {
        lv_evgpu_c_r_t_gl_disable_scissor();
        LV_PROFILER_DRAW_END;
        return;
    }

    int segments = (int)(sweep / 5.0);
    if(segments < 1) segments = 1;
    if(segments > 72) segments = 72;

    int vert_count = (segments + 1) * 2;
    float verts[292];

    double deg_to_rad = 3.141592653589793 / 180.0;

    for(int i = 0; i <= segments; i++) {
        double angle_deg = start + sweep * (double)i / (double)segments;
        double angle_rad = angle_deg * deg_to_rad;
        float c = (float)cos(angle_rad);
        float s = (float)sin(angle_rad);

        int vi = i * 4;
        verts[vi + 0] = cx + inner_r * c;
        verts[vi + 1] = cy + inner_r * s;
        verts[vi + 2] = cx + outer_r * c;
        verts[vi + 3] = cy + outer_r * s;
    }

    lv_evgpu_c_r_t_gl_draw_triangle_strip_solid(&u->gl, verts, vert_count, color, 255);

    lv_evgpu_c_r_t_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
