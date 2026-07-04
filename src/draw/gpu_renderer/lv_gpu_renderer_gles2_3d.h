/**
 * @file lv_gpu_renderer_gles2_3d.h
 */

#ifndef LV_GPU_RENDERER_GLES2_3D_H
#define LV_GPU_RENDERER_GLES2_3D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_RENDERER && LV_USE_3D

#include "../../3d/lv_3d_internal.h"

void lv_gpu_renderer_gles2_3d_init(void);
void lv_gpu_renderer_gles2_3d_deinit(void);
void lv_gpu_renderer_gles2_3d_set_skip_alpha_probe(bool skip);

void lv_gpu_renderer_gles2_render_viewport(unsigned int color_tex, unsigned int depth_rb,
                                            int32_t x, int32_t y, int32_t w, int32_t h,
                                            const float view[16], const float proj[16],
                                            const lv_3d_draw_item_t * items, uint32_t item_count,
                                            bool ar_passthrough, uint8_t * max_alpha_out);

#endif

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_GLES2_3D_H*/
