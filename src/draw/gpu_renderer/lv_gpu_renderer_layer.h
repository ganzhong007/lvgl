/**
 * @file lv_gpu_renderer_layer.h — GPU FBO targets for offscreen layers
 */

#ifndef LV_GPU_RENDERER_LAYER_H
#define LV_GPU_RENDERER_LAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "lv_gpu_renderer_gles2_2d.h"
#include "../lv_draw_private.h"

void lv_gpu_renderer_layer_init(void);
void lv_gpu_renderer_layer_deinit(void);

void lv_gpu_renderer_layer_on_created(lv_layer_t * layer);
void lv_gpu_renderer_layer_on_deleted(lv_layer_t * layer);

bool lv_gpu_renderer_layer_is_target(const lv_layer_t * layer);
unsigned int lv_gpu_renderer_layer_tex(const lv_layer_t * layer);

bool lv_gpu_renderer_layer_push_cmd(lv_layer_t * layer, const lv_gpu_renderer_gles2_cmd_t * cmd);
bool lv_gpu_renderer_layer_flush(lv_layer_t * layer);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_LAYER_H*/
