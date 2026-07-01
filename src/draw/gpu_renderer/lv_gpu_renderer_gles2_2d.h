/**
 * @file lv_gpu_composite_gles2_2d.h — GLES2 2D batch pass [GL2]
 */

#ifndef LV_GPU_COMPOSITE_GLES2_2D_H
#define LV_GPU_COMPOSITE_GLES2_2D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_COMPOSITE

#include "../../misc/lv_area.h"
#include "../../misc/lv_color.h"
#include "../../include/lvgl/draw/lv_draw_label.h"
#include "../../include/lvgl/draw/lv_draw_image.h"
#include "../../include/lvgl/draw/lv_draw_rect.h"

void lv_gpu_composite_gles2_2d_init(void);
void lv_gpu_composite_gles2_2d_deinit(void);

void lv_gpu_composite_gles2_2d_queue_reset(void);
uint32_t lv_gpu_composite_gles2_2d_queue_count(void);
/** Shader buckets (fill vs textured) in the current queue — for framegraph batch stats. */
uint32_t lv_gpu_composite_gles2_2d_count_shader_batches(void);
bool lv_gpu_composite_gles2_2d_is_raster_nest(void);

bool lv_gpu_composite_gles2_2d_queue_fill(const lv_area_t * area, const lv_area_t * clip,
                                          lv_color_t color, lv_opa_t opa, int32_t radius);
bool lv_gpu_composite_gles2_2d_queue_border(const lv_area_t * area, const lv_area_t * clip,
                                            lv_color_t color, lv_opa_t opa, int32_t width,
                                            int32_t radius, lv_border_side_t side);
bool lv_gpu_composite_gles2_2d_queue_label(const lv_area_t * area, const lv_area_t * clip,
                                             const lv_draw_label_dsc_t * dsc);
bool lv_gpu_composite_gles2_2d_queue_letter(const lv_area_t * area, const lv_area_t * clip,
                                              const lv_draw_letter_dsc_t * dsc);
bool lv_gpu_composite_gles2_2d_queue_image(const lv_area_t * area, const lv_area_t * clip,
                                           const lv_draw_image_dsc_t * dsc);

/** Render queued 2D commands into color_tex (full display size). Returns tasks rendered. */
uint32_t lv_gpu_composite_gles2_2d_render_batch(unsigned int color_tex, int32_t dw, int32_t dh,
                                                uint32_t * sw_raster_out);

#endif /*LV_USE_DRAW_GPU_COMPOSITE*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_COMPOSITE_GLES2_2D_H*/
