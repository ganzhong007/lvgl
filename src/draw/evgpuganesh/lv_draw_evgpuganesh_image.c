#include "lv_draw_evgpuganesh.h"
#if LV_USE_DRAW_EVGPUGANESH

#include "lv_draw_evgpuganesh_private.h"
#include "../lv_image_decoder_private.h"
#include "../lv_draw_image_private.h"
#include <GLES2/gl2.h>

#ifndef GL_BGRA
    #ifdef GL_BGRA_EXT
        #define GL_BGRA GL_BGRA_EXT
    #else
        #define GL_BGRA 0x80E1
    #endif
#endif

static void draw_core_cb(lv_draw_task_t * t, const lv_draw_image_dsc_t * draw_dsc,
                         const lv_image_decoder_dsc_t * decoder_dsc, lv_draw_image_sup_t * sup,
                         const lv_area_t * img_coords, const lv_area_t * clipped_img_area);

void lv_draw_evgpuganesh_image(lv_draw_task_t * t, const lv_draw_image_dsc_t * dsc,
                                const lv_area_t * coords, int image_handle)
{
    LV_PROFILER_DRAW_BEGIN;

    LV_UNUSED(image_handle);

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, &t->_real_area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_image_normal_helper(t, dsc, coords, draw_core_cb, NULL);

    LV_PROFILER_DRAW_END;
}

static void draw_core_cb(lv_draw_task_t * t, const lv_draw_image_dsc_t * draw_dsc,
                         const lv_image_decoder_dsc_t * decoder_dsc, lv_draw_image_sup_t * sup,
                         const lv_area_t * img_coords, const lv_area_t * clipped_img_area)
{
    LV_UNUSED(sup);

    lv_draw_evgpuganesh_unit_t * u = (lv_draw_evgpuganesh_unit_t *)t->draw_unit;

    if(!decoder_dsc || !decoder_dsc->decoded) {
        return;
    }

    lv_evgpuganesh_gl_set_scissor(clipped_img_area->x1, clipped_img_area->y1,
                                   lv_area_get_width(clipped_img_area),
                                   lv_area_get_height(clipped_img_area));

    const lv_draw_buf_t * decoded = decoder_dsc->decoded;
    int32_t img_w = (int32_t)decoded->header.w;
    int32_t img_h = (int32_t)decoded->header.h;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    lv_color_format_t cf = decoded->header.cf;
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;

    switch(cf) {
        case LV_COLOR_FORMAT_ARGB8888:
        case LV_COLOR_FORMAT_XRGB8888:
        case LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:
            format = GL_BGRA;
            break;
        case LV_COLOR_FORMAT_RGB888:
            format = GL_RGB;
            break;
        case LV_COLOR_FORMAT_RGB565:
            format = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
            break;
        case LV_COLOR_FORMAT_A8:
            format = GL_ALPHA;
            break;
        case LV_COLOR_FORMAT_L8:
            format = GL_LUMINANCE;
            break;
        default:
            format = GL_BGRA;
            break;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, (format == GL_BGRA) ? GL_BGRA : format,
                 img_w, img_h, 0, format, type, decoded->data);

    if(img_w > 256 || img_h > 256) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    uint32_t recolor = lv_evgpuganesh_color_to_gl_alpha(draw_dsc->recolor, draw_dsc->recolor_opa);
    uint8_t recolor_opa = draw_dsc->recolor_opa;

    lv_evgpuganesh_gl_draw_quad_tex(&u->gl,
                                     (float)img_coords->x1, (float)img_coords->y1,
                                     (float)(img_coords->x2 + 1), (float)(img_coords->y2 + 1),
                                     0.0f, 0.0f, 1.0f, 1.0f,
                                     texture, recolor, recolor_opa, draw_dsc->opa);

    glDeleteTextures(1, &texture);

    lv_evgpuganesh_gl_disable_scissor();
}

#endif
