/**
 * @file lv_g100_3d_pass.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_g100_3d_pass.h"

#if LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS

#include "../../display/lv_display_private.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void destroy_fbo_resources(lv_g100_3d_pass_fbo_t * fbo);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

bool lv_g100_3d_pass_layer_is(const lv_layer_t * layer)
{
    if(layer == NULL || layer->user_data == NULL) return false;

    const lv_3d_pass_layer_ud_t * ud = layer->user_data;
    return ud->magic == LV_3D_PASS_LAYER_MAGIC;
}

lv_3d_pass_t * lv_g100_3d_pass_from_layer(const lv_layer_t * layer)
{
    if(!lv_g100_3d_pass_layer_is(layer)) return NULL;
    lv_3d_pass_layer_ud_t * ud = layer->user_data;
    return &ud->pass;
}

lv_result_t lv_g100_3d_pass_ensure(lv_3d_pass_t * pass, int32_t w, int32_t h)
{
    if(pass == NULL || w <= 0 || h <= 0) return LV_RESULT_INVALID;

    if(pass->fbo.w == w && pass->fbo.h == h && pass->fbo.framebuffer != 0) {
        return LV_RESULT_OK;
    }

    destroy_fbo_resources(&pass->fbo);

    lv_g100_3d_pass_fbo_t * fbo = &pass->fbo;
    fbo->w = w;
    fbo->h = h;

    GL_CALL(glGenTextures(1, &fbo->color_tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, fbo->color_tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    GL_CALL(glGenTextures(1, &fbo->depth_tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, fbo->depth_tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, LV_GL_PREFERRED_DEPTH, w, h, 0, GL_DEPTH_COMPONENT,
                         GL_UNSIGNED_INT, NULL));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    GL_CALL(glGenFramebuffers(1, &fbo->framebuffer));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo->color_tex, 0));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fbo->depth_tex, 0));

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    if(status != GL_FRAMEBUFFER_COMPLETE) {
        LV_LOG_ERROR("3D pass FBO incomplete: 0x%x", status);
        destroy_fbo_resources(fbo);
        return LV_RESULT_INVALID;
    }

    return LV_RESULT_OK;
}

void lv_g100_3d_pass_destroy_fbo(lv_3d_pass_t * pass)
{
    if(pass == NULL) return;
    destroy_fbo_resources(&pass->fbo);
}

void lv_g100_3d_pass_layer_destroy(lv_layer_t * pass_layer, lv_display_t * disp)
{
    if(pass_layer == NULL) return;

    lv_3d_pass_layer_ud_t * ud = pass_layer->user_data;
    if(ud != NULL) {
        lv_g100_3d_pass_destroy_fbo(&ud->pass);
        lv_free(ud);
        pass_layer->user_data = NULL;
    }

    if(disp != NULL) {
        lv_layer_t * prev = disp->layer_head;
        while(prev != NULL && prev->next != pass_layer) {
            prev = prev->next;
        }
        if(prev != NULL) {
            prev->next = pass_layer->next;
        }
        else if(disp->layer_head == pass_layer) {
            disp->layer_head = pass_layer->next;
        }
    }

    lv_free(pass_layer);
}

void lv_g100_3d_pass_set_camera(lv_layer_t * pass_layer, const lv_3d_camera_t * camera)
{
    if(!lv_g100_3d_pass_layer_is(pass_layer) || camera == NULL) return;

    lv_3d_pass_layer_ud_t * ud = pass_layer->user_data;
    ud->camera = *camera;
    int32_t w = lv_area_get_width(&pass_layer->buf_area);
    int32_t h = lv_area_get_height(&pass_layer->buf_area);
    lv_3d_camera_compute_mvp(&ud->camera, w, h, ud->view_proj);
    ud->camera_valid = true;
}

const lv_3d_camera_t * lv_draw_3d_pass_get_camera(const lv_layer_t * pass_layer)
{
    if(!lv_g100_3d_pass_layer_is(pass_layer)) return NULL;
    lv_3d_pass_layer_ud_t * ud = pass_layer->user_data;
    return ud->camera_valid ? &ud->camera : NULL;
}

const float * lv_draw_3d_pass_get_view_proj(const lv_layer_t * pass_layer)
{
    if(!lv_g100_3d_pass_layer_is(pass_layer)) return NULL;
    lv_3d_pass_layer_ud_t * ud = pass_layer->user_data;
    return ud->camera_valid ? ud->view_proj : NULL;
}

bool lv_g100_3d_pass_bind_fbo(lv_layer_t * pass_layer, int32_t * w, int32_t * h, lv_g100_3d_pass_fbo_t ** fbo)
{
    lv_3d_pass_t * pass = lv_g100_3d_pass_from_layer(pass_layer);
    if(pass == NULL) return false;

    *w = lv_area_get_width(&pass_layer->buf_area);
    *h = lv_area_get_height(&pass_layer->buf_area);
    if(lv_g100_3d_pass_ensure(pass, *w, *h) != LV_RESULT_OK) return false;

    *fbo = &pass->fbo;
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, (*fbo)->framebuffer));
    GL_CALL(glViewport(0, 0, *w, *h));
    return true;
}

void lv_g100_3d_pass_unbind_fbo(void)
{
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void destroy_fbo_resources(lv_g100_3d_pass_fbo_t * fbo)
{
    if(fbo == NULL) return;

    if(fbo->framebuffer) {
        GL_CALL(glDeleteFramebuffers(1, &fbo->framebuffer));
        fbo->framebuffer = 0;
    }
    if(fbo->color_tex) {
        GL_CALL(glDeleteTextures(1, &fbo->color_tex));
        fbo->color_tex = 0;
    }
    if(fbo->depth_tex) {
        GL_CALL(glDeleteTextures(1, &fbo->depth_tex));
        fbo->depth_tex = 0;
    }
    fbo->w = 0;
    fbo->h = 0;
}

#endif /* LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS */
