#include "lv_evgpuganesh_fbo.h"
#if LV_USE_DRAW_EVGPUGANESH

#include <GLES2/gl2.h>
#include <stdlib.h>

lv_evgpuganesh_fbo_t * lv_evgpuganesh_fbo_create(int w, int h)
{
    lv_evgpuganesh_fbo_t * fbo = lv_malloc_zeroed(sizeof(lv_evgpuganesh_fbo_t));
    if(!fbo) return NULL;

    fbo->w = w;
    fbo->h = h;

    glGenFramebuffers(1, &fbo->fbo);
    glGenTextures(1, &fbo->tex);

    glBindTexture(GL_TEXTURE_2D, fbo->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo->tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return fbo;
}

void lv_evgpuganesh_fbo_destroy(lv_evgpuganesh_fbo_t * fbo)
{
    if(!fbo) return;
    glDeleteFramebuffers(1, &fbo->fbo);
    glDeleteTextures(1, &fbo->tex);
    lv_free(fbo);
}

#endif
