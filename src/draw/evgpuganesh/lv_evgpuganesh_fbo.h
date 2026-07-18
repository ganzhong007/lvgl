#ifndef LV_EVGPUGANESH_FBO_H
#define LV_EVGPUGANESH_FBO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPUGANESH

#include <GLES2/gl2.h>

typedef struct {
    GLuint fbo;
    GLuint tex;
    int w;
    int h;
} lv_evgpuganesh_fbo_t;

lv_evgpuganesh_fbo_t * lv_evgpuganesh_fbo_create(int w, int h);
void lv_evgpuganesh_fbo_destroy(lv_evgpuganesh_fbo_t * fbo);

#endif

#ifdef __cplusplus
}
#endif

#endif
