#include "lv_draw_evgpu_c_r_t.h"
#if LV_USE_DRAW_EVGPU_C_R_T
#include "lv_draw_evgpu_c_r_t_private.h"
#include "../../misc/lv_math.h"

static int32_t clamp_radius(int32_t radius, int32_t w, int32_t h)
{
    if(radius <= 0) return 0;
    int32_t max_r = LV_MIN(w, h) / 2;
    if(radius > max_r) return max_r;
    return radius;
}

static void draw_radial(lv_draw_evgpu_c_r_t_unit_t * u, const lv_draw_fill_dsc_t * dsc,
                        const lv_area_t * coords)
{
    const int32_t w = lv_area_get_width(coords);
    const int32_t h = lv_area_get_height(coords);
    const int32_t clip_r = clamp_radius(dsc->radius, w, h);

    const float fx = (float)(lv_pct_to_px(dsc->grad.params.radial.focal.x, w) + coords->x1);
    const float fy = (float)(lv_pct_to_px(dsc->grad.params.radial.focal.y, h) + coords->y1);
    const float fex = (float)(lv_pct_to_px(dsc->grad.params.radial.focal_extent.x, w) + coords->x1);
    const float fey = (float)(lv_pct_to_px(dsc->grad.params.radial.focal_extent.y, h) + coords->y1);
    const float eex = (float)(lv_pct_to_px(dsc->grad.params.radial.end_extent.x, w) + coords->x1);
    const float eey = (float)(lv_pct_to_px(dsc->grad.params.radial.end_extent.y, h) + coords->y1);

    float r0 = (float)lv_sqrt32((int32_t)lv_sqr((int32_t)(fex - fx)) + (int32_t)lv_sqr((int32_t)(fey - fy)));
    float r1 = (float)lv_sqrt32((int32_t)lv_sqr((int32_t)(eex - fx)) + (int32_t)lv_sqr((int32_t)(eey - fy)));
    if(r1 < r0 + 0.001f) r1 = r0 + 0.001f;

    float colors[EVGPU_C_R_T_RADIAL_MAX_STOPS * 4];
    float fracs[EVGPU_C_R_T_RADIAL_MAX_STOPS];
    int n = LV_MIN((int)dsc->grad.stops_count, EVGPU_C_R_T_RADIAL_MAX_STOPS);
    if(n < 1) {
        colors[0] = dsc->color.red / 255.f;
        colors[1] = dsc->color.green / 255.f;
        colors[2] = dsc->color.blue / 255.f;
        colors[3] = dsc->opa / 255.f;
        fracs[0] = 0.f;
        n = 1;
    }
    else {
        for(int i = 0; i < n; i++) {
            const lv_grad_stop_t * s = &dsc->grad.stops[i];
            lv_opa_t opa = LV_OPA_MIX2(s->opa, dsc->opa);
            colors[i * 4 + 0] = s->color.red / 255.f;
            colors[i * 4 + 1] = s->color.green / 255.f;
            colors[i * 4 + 2] = s->color.blue / 255.f;
            colors[i * 4 + 3] = opa / 255.f;
            fracs[i] = s->frac / 255.f;
        }
    }

    lv_evgpu_c_r_t_gl_draw_radial_grad(&u->gl,
                                        (float)coords->x1, (float)coords->y1,
                                        (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                        (float)clip_r,
                                        fx, fy, r0, r1,
                                        colors, fracs, n);
}

void lv_draw_evgpu_c_r_t_gradient(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords)
{
    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        return;
    }

    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    if(dsc->grad.dir == LV_GRAD_DIR_RADIAL) {
        draw_radial(u, dsc, coords);
        lv_evgpu_c_r_t_gl_flush(&u->gl);
        lv_evgpu_c_r_t_gl_disable_scissor();
        return;
    }

    if(dsc->grad.dir != LV_GRAD_DIR_HOR && dsc->grad.dir != LV_GRAD_DIR_VER) {
        /* Other complex dirs: flat stop0 fallback (keep ambient-safe). */
        lv_opa_t a0 = dsc->grad.stops[0].opa;
        lv_opa_t aN = dsc->grad.stops[dsc->grad.stops_count - 1].opa;
        if(aN <= LV_OPA_MIN && a0 < LV_OPA_80) {
            lv_evgpu_c_r_t_gl_disable_scissor();
            return;
        }
        lv_color_t c = dsc->grad.stops[0].color;
        lv_opa_t opa = LV_OPA_MIX2(a0, dsc->opa);
        if(dsc->grad.stops_count == 0) {
            c = dsc->color;
            opa = dsc->opa;
        }
        const int32_t w = lv_area_get_width(coords);
        const int32_t h = lv_area_get_height(coords);
        const int32_t radius = clamp_radius(dsc->radius, w, h);
        uint32_t color = lv_evgpu_c_r_t_color_to_gl_alpha(c, opa);
        if(radius > 0) {
            lv_evgpu_c_r_t_gl_draw_round_rect(&u->gl,
                                               (float)coords->x1, (float)coords->y1,
                                               (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                               (float)radius, 0.f, color, 255);
        }
        else {
            lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                               (float)coords->x1, (float)coords->y1,
                                               (float)(coords->x2 + 1), (float)(coords->y2 + 1),
                                               color, 255);
        }
        lv_evgpu_c_r_t_gl_flush(&u->gl);
        lv_evgpu_c_r_t_gl_disable_scissor();
        return;
    }

    uint32_t c1 = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->grad.stops[0].color, dsc->grad.stops[0].opa);
    uint32_t c2 = lv_evgpu_c_r_t_color_to_gl_alpha(dsc->grad.stops[dsc->grad.stops_count - 1].color,
                                                     dsc->grad.stops[dsc->grad.stops_count - 1].opa);
    int dir = (dsc->grad.dir == LV_GRAD_DIR_HOR) ? 0 : 1;

    lv_evgpu_c_r_t_gl_draw_quad_grad(&u->gl,
                                      coords->x1, coords->y1,
                                      coords->x2 + 1, coords->y2 + 1,
                                      c1, c2, dir);

    lv_evgpu_c_r_t_gl_disable_scissor();
}

#endif
