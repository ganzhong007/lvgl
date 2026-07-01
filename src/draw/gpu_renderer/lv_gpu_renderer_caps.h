/**
 * @file lv_gpu_renderer_caps.h
 */

#ifndef LV_GPU_RENDERER_CAPS_H
#define LV_GPU_RENDERER_CAPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_RENDERER

typedef struct {
    uint8_t gles_major;
    bool has_fbo;
    bool has_depth16;
    int32_t max_texture_size;
} lv_gpu_renderer_caps_t;

void lv_gpu_renderer_caps_probe(lv_gpu_renderer_caps_t * caps);
void lv_gpu_renderer_caps_log(const lv_gpu_renderer_caps_t * caps);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_CAPS_H*/
