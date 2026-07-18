#ifndef LV_EVGPU_C_R_T_FBO_H
#define LV_EVGPU_C_R_T_FBO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU_C_R_T

#include <GLES2/gl2.h>

typedef struct {
    GLuint fbo;
    GLuint tex;
    int w;
    int h;
} lv_evgpu_c_r_t_fbo_t;

lv_evgpu_c_r_t_fbo_t * lv_evgpu_c_r_t_fbo_create(int w, int h);
void lv_evgpu_c_r_t_fbo_destroy(lv_evgpu_c_r_t_fbo_t * fbo);

#endif

#ifdef __cplusplus
}
#endif

#endif
