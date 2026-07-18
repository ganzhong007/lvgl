#include "lv_draw_evgpu_c_r_t.h"
#if LV_USE_DRAW_EVGPU_C_R_T
#include "lv_draw_evgpu_c_r_t_private.h"

void lv_draw_evgpu_c_r_t_gradient(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords)
{
    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_evgpu_c_r_t_gl_set_scissor(coords->x1, coords->y1,
                                   lv_area_get_width(coords), lv_area_get_height(coords));

    uint32_t c1 = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->grad.stops[0].color, dsc->grad.stops[0].opa);
    uint32_t c2 = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->grad.stops[dsc->grad.stops_count - 1].color,
                                                     dsc->grad.stops[dsc->grad.stops_count - 1].opa);
    int dir = (dsc->grad.dir == LV_GRAD_DIR_HOR) ? 0 : 1;

    lv_evgpu_c_r_t_gl_draw_quad_grad(&u->gl,
                                      coords->x1, coords->y1,
                                      coords->x2 + 1, coords->y2 + 1,
                                      c1, c2, dir);
}

#endif
