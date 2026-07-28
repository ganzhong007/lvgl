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
    /* Clear+seed only on first bind. Re-binding mid-frame (parent←child)
     * must not wipe GPU content already drawn into this FBO — Unity attaches
     * an FBO to the display layer, which made every LAYER composite land on
     * a freshly cleared (black) parent. */
    bool needs_clear;
} lv_evgpu_c_r_t_fbo_t;

lv_evgpu_c_r_t_fbo_t * lv_evgpu_c_r_t_fbo_create(int w, int h);
void lv_evgpu_c_r_t_fbo_destroy(lv_evgpu_c_r_t_fbo_t * fbo);

#endif

#ifdef __cplusplus
}
#endif

#endif
