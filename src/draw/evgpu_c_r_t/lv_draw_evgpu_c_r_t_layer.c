#include "lv_draw_evgpu_c_r_t_private.h"
#include "lv_evgpu_c_r_t_fbo.h"
#if LV_USE_DRAW_EVGPU_C_R_T

#include "../lv_image_decoder_private.h"
#include <GLES2/gl2.h>

GLuint lv_evgpu_c_r_t_upload_bitmap_mask(const void * mask_src, const lv_area_t * coords,
                                         const lv_area_t * align_area,
                                         float * mu1, float * mv1, float * mu2, float * mv2,
                                         bool * out_visible)
{
    *out_visible = true;
    if(mask_src == NULL) return 0;

    lv_image_decoder_dsc_t mask_dsc;
    lv_result_t res = lv_image_decoder_open(&mask_dsc, mask_src, NULL);
    if(res != LV_RESULT_OK || mask_dsc.decoded == NULL) {
        if(res == LV_RESULT_OK) lv_image_decoder_close(&mask_dsc);
        LV_LOG_WARN("CRT: could not open bitmap mask; drawing unmasked");
        return 0;
    }

    const lv_draw_buf_t * mask_buf = mask_dsc.decoded;
    if(mask_buf->header.cf != LV_COLOR_FORMAT_A8 && mask_buf->header.cf != LV_COLOR_FORMAT_L8) {
        lv_image_decoder_close(&mask_dsc);
        LV_LOG_WARN("CRT: bitmap mask is not A8/L8; drawing unmasked");
        return 0;
    }

    const lv_area_t * align = align_area;
    lv_area_t tmp_align;
    if(lv_area_get_width(align) < 0) {
        tmp_align = *coords;
        align = &tmp_align;
    }

    lv_area_t mask_area;
    lv_area_set(&mask_area, 0, 0, (int32_t)mask_buf->header.w - 1, (int32_t)mask_buf->header.h - 1);
    lv_area_align(align, &mask_area, LV_ALIGN_CENTER, 0, 0);

    lv_area_t masked;
    if(!lv_area_intersect(&masked, &mask_area, coords)) {
        lv_image_decoder_close(&mask_dsc);
        *out_visible = false;
        return 0;
    }

    float mw = (float)mask_buf->header.w;
    float mh = (float)mask_buf->header.h;
    *mu1 = ((float)coords->x1 - (float)mask_area.x1) / mw;
    *mv1 = ((float)coords->y1 - (float)mask_area.y1) / mh;
    *mu2 = ((float)(coords->x2 + 1) - (float)mask_area.x1) / mw;
    *mv2 = ((float)(coords->y2 + 1) - (float)mask_area.y1) / mh;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    const int32_t w = (int32_t)mask_buf->header.w;
    const int32_t h = (int32_t)mask_buf->header.h;
    const uint32_t stride = mask_buf->header.stride;
    if(stride == (uint32_t)w) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, mask_buf->data);
    }
    else {
        uint8_t * packed = lv_malloc((size_t)w * (size_t)h);
        if(packed == NULL) {
            glDeleteTextures(1, &tex);
            lv_image_decoder_close(&mask_dsc);
            return 0;
        }
        for(int32_t y = 0; y < h; y++) {
            lv_memcpy(packed + (size_t)y * (size_t)w,
                      mask_buf->data + (size_t)y * stride,
                      (size_t)w);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, packed);
        lv_free(packed);
    }

    lv_image_decoder_close(&mask_dsc);
    return tex;
}

static void layer_build_corners(const lv_area_t * coords, const lv_draw_image_dsc_t * draw_dsc,
                                lv_point_t out[4])
{
    /* Exclusive bottom-right edge so scaled/rotated edges match AABB blit. */
    out[0].x = coords->x1;
    out[0].y = coords->y1;
    out[1].x = coords->x2 + 1;
    out[1].y = coords->y1;
    out[2].x = coords->x1;
    out[2].y = coords->y2 + 1;
    out[3].x = coords->x2 + 1;
    out[3].y = coords->y2 + 1;

    if(draw_dsc->rotation == 0 &&
       draw_dsc->scale_x == LV_SCALE_NONE &&
       draw_dsc->scale_y == LV_SCALE_NONE) {
        return;
    }

    lv_point_t pivot = {
        coords->x1 + draw_dsc->pivot.x,
        coords->y1 + draw_dsc->pivot.y
    };
    /* Match SW / EVGPU: scale first, then rotate. */
    lv_point_array_transform(out, 4, draw_dsc->rotation, draw_dsc->scale_x, draw_dsc->scale_y,
                             &pivot, true);
}

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

    /* Clip against transformed bounds (task _real_area), not untransformed coords. */
    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, &t->_real_area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    float mu1 = 0, mv1 = 0, mu2 = 1, mv2 = 1;
    bool visible = true;
    GLuint mask_tex = 0;
    if(draw_dsc->bitmap_mask_src) {
        /* Mask lives in layer/buffer space (same as SW apply_mask), then rides
         * the transformed verts via per-corner UVs. */
        mask_tex = lv_evgpu_c_r_t_upload_bitmap_mask(draw_dsc->bitmap_mask_src, coords, &draw_dsc->image_area,
                                                     &mu1, &mv1, &mu2, &mv2, &visible);
        if(!visible) {
            LV_PROFILER_DRAW_END;
            return;
        }
    }

    lv_point_t corners[4];
    layer_build_corners(coords, draw_dsc, corners);

    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip_area.x1, clip_area.y1,
                                   lv_area_get_width(&clip_area),
                                   lv_area_get_height(&clip_area));

    /* FBO color: flip V (GL v=0 = bottom of attachment). Mask: top-first upload. */
    const float cu[4] = {0.f, 1.f, 0.f, 1.f};
    const float cv[4] = {1.f, 1.f, 0.f, 0.f};
    const float mu[4] = {mu1, mu2, mu1, mu2};
    const float mv[4] = {mv1, mv1, mv2, mv2};

    if(mask_tex) {
        float verts[24];
        for(int i = 0; i < 4; i++) {
            verts[i * 6 + 0] = (float)corners[i].x;
            verts[i * 6 + 1] = (float)corners[i].y;
            verts[i * 6 + 2] = cu[i];
            verts[i * 6 + 3] = cv[i];
            verts[i * 6 + 4] = mu[i];
            verts[i * 6 + 5] = mv[i];
        }
        lv_evgpu_c_r_t_gl_draw_quad_tex_mask_strip(&u->gl, verts,
                                                   fbo->tex, mask_tex,
                                                   0, 0, draw_dsc->opa);
        glDeleteTextures(1, &mask_tex);
    }
    else {
        float verts[16];
        for(int i = 0; i < 4; i++) {
            verts[i * 4 + 0] = (float)corners[i].x;
            verts[i * 4 + 1] = (float)corners[i].y;
            verts[i * 4 + 2] = cu[i];
            verts[i * 4 + 3] = cv[i];
        }
        lv_evgpu_c_r_t_gl_draw_quad_tex_strip(&u->gl, verts,
                                              fbo->tex, 0, 0, draw_dsc->opa);
    }

    lv_evgpu_c_r_t_gl_disable_scissor();

    LV_PROFILER_DRAW_END;
}

#endif
