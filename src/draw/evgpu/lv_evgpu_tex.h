/**
 * @file lv_evgpu_tex.h
 * @brief EVGPU native GLES2 textured quad draw (G1 CP-01c image path).
 */

#ifndef LV_EVGPU_TEX_H
#define LV_EVGPU_TEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

#include "../../../include/lvgl/core/lv_area.h"
#include "../../../include/lvgl/core/lv_matrix.h"
#include "../lv_draw_image_private.h"

struct _lv_draw_evgpu_unit_t;

void lv_evgpu_tex_init(struct _lv_draw_evgpu_unit_t * unit);
void lv_evgpu_tex_deinit(struct _lv_draw_evgpu_unit_t * unit);

bool lv_evgpu_tex_draw_image(struct _lv_draw_evgpu_unit_t * unit,
                            const lv_draw_image_dsc_t * dsc,
                            const lv_area_t * coords,
                            int32_t rect_w,
                            int32_t rect_h,
                            int image_handle,
                            const lv_area_t * clip_area,
                            const lv_matrix_t * matrix);

#endif /*LV_USE_DRAW_EVGPU*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_TEX_H*/
