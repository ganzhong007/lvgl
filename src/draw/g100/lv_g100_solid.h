/**
 * @file lv_g100_solid.h
 * @brief G100 native GLES2 solid-color fill (G1 CP-01c).
 */

#ifndef LV_G100_SOLID_H
#define LV_G100_SOLID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100

#include "../../../include/lvgl/core/lv_area.h"
#include "../../../include/lvgl/core/lv_matrix.h"

struct _lv_draw_g100_unit_t;

void lv_g100_solid_init(struct _lv_draw_g100_unit_t * unit);
void lv_g100_solid_deinit(struct _lv_draw_g100_unit_t * unit);

bool lv_g100_solid_fill_rect(struct _lv_draw_g100_unit_t * unit,
                             const lv_area_t * coords,
                             lv_color_t color,
                             lv_opa_t opa,
                             float radius,
                             const lv_area_t * clip_area,
                             const lv_matrix_t * matrix);

#endif /*LV_USE_DRAW_G100*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_SOLID_H*/
