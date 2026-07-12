/**
 * @file lv_g100_tex.h
 * @brief G100 native GLES2 textured quad draw (G1 CP-01c image path).
 */

#ifndef LV_G100_TEX_H
#define LV_G100_TEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100

#include "../../../include/lvgl/core/lv_area.h"
#include "../../../include/lvgl/core/lv_matrix.h"
#include "../lv_draw_image_private.h"

struct _lv_draw_g100_unit_t;

void lv_g100_tex_init(struct _lv_draw_g100_unit_t * unit);
void lv_g100_tex_deinit(struct _lv_draw_g100_unit_t * unit);

bool lv_g100_tex_draw_image(struct _lv_draw_g100_unit_t * unit,
                            const lv_draw_image_dsc_t * dsc,
                            const lv_area_t * coords,
                            int32_t rect_w,
                            int32_t rect_h,
                            int image_handle,
                            const lv_area_t * clip_area,
                            const lv_matrix_t * matrix);

#endif /*LV_USE_DRAW_G100*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_TEX_H*/
