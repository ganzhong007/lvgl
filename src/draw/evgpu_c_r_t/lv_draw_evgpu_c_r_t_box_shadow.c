#include "lv_draw_evgpu_c_r_t_private.h"
#include "lv_evgpu_c_r_t_kawase.h"
#include "lv_evgpu_c_r_t_fbo.h"
#if LV_USE_DRAW_EVGPU_C_R_T

#include <GLES2/gl2.h>

#define CRT_KAWASE_BLUR_THRESHOLD 64

static int32_t clamp_radius(int32_t radius, int32_t w, int32_t h)
{
    if(radius <= 0) return 0;
    int32_t max_r = LV_MIN(w, h) / 2;
    if(radius > max_r) return max_r;
    return radius;
}

/** Soft shadow via stacked expanding rounded rects (no FBO). */
static void soft_shadow_layers(lv_draw_evgpu_c_r_t_unit_t * u,
                               const lv_draw_box_shadow_dsc_t * dsc,
                               const lv_area_t * core_area)
{
    const int32_t core_w = lv_area_get_width(core_area);
    const int32_t core_h = lv_area_get_height(core_area);
    int32_t radius = clamp_radius(dsc->radius, core_w, core_h);

    const int blur = LV_MAX(dsc->width / 2, 1);
    int layers = blur;
    if(layers > 16) layers = 16;
    if(layers < 4) layers = 4;

    /* Outer → inner so later (more opaque) layers sit on top of soft fringe. */
    for(int i = layers; i >= 0; i--) {
        float t = (float)i / (float)layers; /* 1 = outer, 0 = core */
        float expand = (float)blur * t;
        /* Approximate Gaussian falloff; scale so stacked alpha stays near dsc->opa. */
        float fall = 1.f - t;
        float a = (dsc->opa / 255.f) * fall * fall * (2.5f / (float)(layers + 1));
        if(a < 0.002f) continue;
        uint8_t opa = (uint8_t)LV_CLAMP(0, (int)(a * 255.f + 0.5f), 255);

        float x1 = (float)core_area->x1 - expand;
        float y1 = (float)core_area->y1 - expand;
        float x2 = (float)(core_area->x2 + 1) + expand;
        float y2 = (float)(core_area->y2 + 1) + expand;
        float r = (float)radius + expand;

        lv_evgpu_c_r_t_gl_draw_round_rect(&u->gl, x1, y1, x2, y2, r, 0.f,
                                           lv_evgpu_c_r_t_color_to_gl_alpha(dsc->color, opa),
                                           255);
    }
}

void lv_draw_evgpu_c_r_t_box_shadow(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_area_t core_area;
    core_area.x1 = coords->x1 + dsc->ofs_x - dsc->spread;
    core_area.x2 = coords->x2 + dsc->ofs_x + dsc->spread;
    core_area.y1 = coords->y1 + dsc->ofs_y - dsc->spread;
    core_area.y2 = coords->y2 + dsc->ofs_y + dsc->spread;

    lv_area_t shadow_area;
    shadow_area.x1 = core_area.x1 - dsc->width / 2 - 1;
    shadow_area.x2 = core_area.x2 + dsc->width / 2 + 1;
    shadow_area.y1 = core_area.y1 - dsc->width / 2 - 1;
    shadow_area.y2 = core_area.y2 + dsc->width / 2 + 1;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, &shadow_area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    if(dsc->width >= CRT_KAWASE_BLUR_THRESHOLD && lv_evgpu_c_r_t_kawase_ready()) {
        const int blur_radius = dsc->width / 2;
        if(blur_radius <= 0) {
            LV_PROFILER_DRAW_END;
            return;
        }

        lv_layer_t * layer = t->target_layer;
        const int32_t layer_h = lv_area_get_height(&layer->buf_area);

        lv_evgpu_c_r_t_gl_flush(&u->gl);

        GLint prev_fbo, prev_vp[4];
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        glGetIntegerv(GL_VIEWPORT, prev_vp);

        const int32_t fbo_w = LV_MAX(lv_area_get_width(&core_area), 1);
        const int32_t fbo_h = LV_MAX(lv_area_get_height(&core_area), 1);

        lv_evgpu_c_r_t_fbo_t * fbo = lv_evgpu_c_r_t_fbo_create(fbo_w, fbo_h);
        if(!fbo) {
            glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
            LV_PROFILER_DRAW_END;
            return;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
        glViewport(0, 0, fbo_w, fbo_h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        const int32_t saved_view_h = u->gl.view_h;
        u->gl.view_h = fbo_h;
        lv_evgpu_c_r_t_gl_set_scissor(&u->gl, 0, 0, fbo_w, fbo_h);
        lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                           (float)core_area.x1, (float)core_area.y1,
                                           (float)(core_area.x2 + 1), (float)(core_area.y2 + 1),
                                           0xFFFFFFFF, LV_OPA_COVER);
        lv_evgpu_c_r_t_gl_disable_scissor();
        u->gl.view_h = saved_view_h;
        lv_evgpu_c_r_t_gl_flush(&u->gl);

        const int32_t dst_x = shadow_area.x1 - layer->buf_area.x1;
        const int32_t dst_y = layer_h - (shadow_area.y2 - layer->buf_area.y1) - fbo_h;

        const int kawase_dst_fbo = (layer->user_data != NULL)
            ? (int)((lv_evgpu_c_r_t_fbo_t *)layer->user_data)->fbo
            : 0;

        lv_evgpu_c_r_t_kawase_blur_region(kawase_dst_fbo, dst_x, dst_y,
                                           lv_area_get_width(&shadow_area),
                                           lv_area_get_height(&shadow_area),
                                           blur_radius,
                                           dsc->color.red, dsc->color.green, dsc->color.blue,
                                           dsc->opa);

        glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);

        lv_evgpu_c_r_t_fbo_destroy(fbo);

        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    soft_shadow_layers(u, dsc, &core_area);
    lv_evgpu_c_r_t_gl_flush(&u->gl);

    lv_evgpu_c_r_t_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
