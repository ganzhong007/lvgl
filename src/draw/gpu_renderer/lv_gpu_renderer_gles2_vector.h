/**
 * @file lv_gpu_renderer_gles2_vector.h — GLES2 vector path fill/stroke (no ThorVG raster)
 */
#ifndef LV_GPU_RENDERER_GLES2_VECTOR_H
#define LV_GPU_RENDERER_GLES2_VECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_RENDERER && LV_USE_VECTOR_GRAPHIC

#include "../../include/lvgl/draw/lv_draw_vector.h"
#include "../../draw/lv_draw_vector_private.h"
#include "../../include/lvgl/core/lv_matrix.h"
#include "../../misc/lv_area.h"
#include <stdint.h>
#include <stdbool.h>

void lv_gpu_renderer_gles2_vector_init(void);
void lv_gpu_renderer_gles2_vector_deinit(void);

/** Draw lv_draw_vector_dsc_t subtasks into the currently bound color FBO. */
bool lv_gpu_renderer_gles2_vector_draw_dsc(int32_t dw, int32_t dh,
                                           const lv_draw_vector_dsc_t * dsc,
                                           const lv_area_t * clip);

/** Draw a single path (solid fill + optional stroke) with transforms applied. */
bool lv_gpu_renderer_gles2_vector_draw_path(int32_t dw, int32_t dh,
                                            const lv_vector_path_t * path,
                                            const lv_matrix_t * matrix,
                                            lv_color32_t fill_color, lv_opa_t fill_opa,
                                            lv_color32_t stroke_color, lv_opa_t stroke_opa,
                                            float stroke_width,
                                            const lv_area_t * clip);

#endif /*LV_USE_DRAW_GPU_RENDERER && LV_USE_VECTOR_GRAPHIC*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_GLES2_VECTOR_H*/
