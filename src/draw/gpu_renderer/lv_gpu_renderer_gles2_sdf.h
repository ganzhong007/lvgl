/**
 * @file lv_gpu_renderer_gles2_sdf.h — GPU signed-distance field from A8 glyphs
 */
#ifndef LV_GPU_RENDERER_GLES2_SDF_H
#define LV_GPU_RENDERER_GLES2_SDF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_RENDERER

#include <stdint.h>
#include <stdbool.h>

/**
 * Build 8-bit SDF (128=edge) from an A8 glyph bitmap entirely on GPU and upload
 * into atlas_tex at (dst_x, dst_y) with size sdf_w x sdf_h.
 */
bool lv_gpu_renderer_gles2_sdf_upload_atlas(unsigned int atlas_tex, int32_t dst_x, int32_t dst_y,
                                            const uint8_t * bitmap, int32_t bw, int32_t bh, int32_t stride,
                                            int32_t sdf_w, int32_t sdf_h);

void lv_gpu_renderer_gles2_sdf_init(void);
void lv_gpu_renderer_gles2_sdf_deinit(void);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_GLES2_SDF_H*/
