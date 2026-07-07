/**
 * @file lv_gpu_renderer_glyph_atlas.h — dynamic glyph texture atlas for GPU text
 */

#ifndef LV_GPU_RENDERER_GLYPH_ATLAS_H
#define LV_GPU_RENDERER_GLYPH_ATLAS_H

#ifdef __cplusplus
extern "C" {
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "../../include/lvgl/font/lv_font.h"

typedef struct {
    unsigned int tex;
    float u0;
    float v0;
    float u1;
    float v1;
    /** 1 when atlas stores 8-bit SDF (128=edge), 0 for raw A8 alpha. */
    uint8_t sdf;
} lv_gpu_glyph_atlas_uv_t;

void lv_gpu_glyph_atlas_init(void);
void lv_gpu_glyph_atlas_deinit(void);

/** Upload or fetch cached A8 glyph bitmap region. Returns false on atlas overflow. */
bool lv_gpu_glyph_atlas_acquire(const lv_font_t * font, uint32_t glyph_id,
                                 const uint8_t * bitmap, int32_t bw, int32_t bh, int32_t stride,
                                 lv_gpu_glyph_atlas_uv_t * out);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_GLYPH_ATLAS_H*/
