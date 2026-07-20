#include "lv_draw_evgpu_c_r_t.h"
#if LV_USE_DRAW_EVGPU_C_R_T

#include "lv_draw_evgpu_c_r_t_private.h"
#include "../lv_draw_label_private.h"
#include <GLES2/gl2.h>

static void draw_letter_cb(lv_draw_task_t * t, lv_draw_glyph_dsc_t * glyph_dsc,
                           lv_draw_fill_dsc_t * fill_dsc, const lv_area_t * fill_area);

void lv_draw_evgpu_c_r_t_letter(lv_draw_task_t * t, const lv_draw_letter_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    if(dsc->opa <= LV_OPA_MIN) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_glyph_dsc_t glyph_dsc;
    lv_draw_glyph_dsc_init(&glyph_dsc);
    glyph_dsc.opa = dsc->opa;
    glyph_dsc.bg_coords = NULL;
    glyph_dsc.color = dsc->color;
    glyph_dsc.rotation = dsc->rotation;
    glyph_dsc.pivot = dsc->pivot;

    lv_point_t pos = { .x = coords->x1, .y = coords->y1 };
    lv_draw_unit_draw_letter(t, &glyph_dsc, &pos, dsc->font, dsc->unicode, draw_letter_cb);

    if(glyph_dsc._draw_buf) {
        lv_draw_buf_destroy(glyph_dsc._draw_buf);
        glyph_dsc._draw_buf = NULL;
    }

    LV_PROFILER_DRAW_END;
}

void lv_draw_evgpu_c_r_t_label(lv_draw_task_t * t, const lv_draw_label_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_label_iterate_characters(t, dsc, coords, draw_letter_cb);

    LV_PROFILER_DRAW_END;
}

static void draw_letter_cb(lv_draw_task_t * t, lv_draw_glyph_dsc_t * glyph_dsc,
                           lv_draw_fill_dsc_t * fill_dsc, const lv_area_t * fill_area)
{
    LV_UNUSED(fill_dsc);
    LV_UNUSED(fill_area);

    if(!glyph_dsc) return;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;

    lv_font_glyph_dsc_t * g = glyph_dsc->g;
    if(!g) return;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, glyph_dsc->letter_coords, &t->clip_area)) {
        return;
    }

    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    if(g->format == LV_FONT_GLYPH_FORMAT_IMAGE) {
        lv_draw_image_dsc_t image_dsc;
        lv_draw_image_dsc_init(&image_dsc);
        image_dsc.opa = glyph_dsc->opa;
        image_dsc.src = glyph_dsc->glyph_data;
        image_dsc.rotation = glyph_dsc->rotation;
        lv_draw_evgpu_c_r_t_image(t, &image_dsc, glyph_dsc->letter_coords, -1);
        lv_evgpu_c_r_t_gl_disable_scissor();
        return;
    }

    switch(g->format) {
        case LV_FONT_GLYPH_FORMAT_A1:
        case LV_FONT_GLYPH_FORMAT_A2:
        case LV_FONT_GLYPH_FORMAT_A3:
        case LV_FONT_GLYPH_FORMAT_A4:
        case LV_FONT_GLYPH_FORMAT_A8:
            break;
        default:
            lv_evgpu_c_r_t_gl_disable_scissor();
            return;
    }

    const lv_draw_buf_t * bitmap_db =
        (const lv_draw_buf_t *)lv_font_get_glyph_bitmap(g, glyph_dsc->_draw_buf);
    if(!bitmap_db || !bitmap_db->data) {
        lv_evgpu_c_r_t_gl_disable_scissor();
        return;
    }

    int32_t bw = g->box_w;
    int32_t bh = g->box_h;
    if(bw == 0 || bh == 0) {
        lv_evgpu_c_r_t_gl_disable_scissor();
        return;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* lv_font_get_glyph_bitmap returns an lv_draw_buf_t*, not raw A8 bytes.
     * Copy out with stride into tightly packed RGBA (GLES2 has no ROW_LENGTH). */
    {
        const uint8_t * src_base = bitmap_db->data;
        uint32_t stride = bitmap_db->header.stride;
        if(stride < (uint32_t)bw) stride = (uint32_t)bw;
        uint8_t * rgba = (uint8_t *)lv_malloc((size_t)bw * (size_t)bh * 4u);
        if(!rgba) {
            glDeleteTextures(1, &texture);
            lv_evgpu_c_r_t_gl_disable_scissor();
            return;
        }
        for(int32_t y = 0; y < bh; y++) {
            const uint8_t * src = src_base + (uint32_t)y * stride;
            uint8_t * dst = rgba + (size_t)y * (size_t)bw * 4u;
            for(int32_t x = 0; x < bw; x++) {
                uint8_t a = src[x];
                dst[x * 4 + 0] = 255;
                dst[x * 4 + 1] = 255;
                dst[x * 4 + 2] = 255;
                dst[x * 4 + 3] = a;
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bw, bh, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        lv_free(rgba);
    }

    int32_t x1 = glyph_dsc->letter_coords->x1;
    int32_t y1 = glyph_dsc->letter_coords->y1;
    int32_t x2 = glyph_dsc->letter_coords->x2;
    int32_t y2 = glyph_dsc->letter_coords->y2;

    /* Recolor RGB from glyph color; alpha only via u_alpha (avoid double opa). */
    uint32_t recolor = lv_evgpu_c_r_t_color_to_gl(glyph_dsc->color);
    lv_evgpu_c_r_t_gl_draw_quad_tex(&u->gl,
                                     (float)x1, (float)y1,
                                     (float)(x2 + 1), (float)(y2 + 1),
                                     0.0f, 0.0f, 1.0f, 1.0f,
                                     texture, recolor, LV_OPA_COVER, glyph_dsc->opa);

    glDeleteTextures(1, &texture);

    lv_evgpu_c_r_t_gl_disable_scissor();
}

#endif
