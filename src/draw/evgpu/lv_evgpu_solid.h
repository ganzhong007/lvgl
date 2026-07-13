/**
 * @file lv_evgpu_solid.h
 * @brief EVGPU native GLES2 solid-color fill (G1 CP-01c).
 */

#ifndef LV_EVGPU_SOLID_H
#define LV_EVGPU_SOLID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

#include "../../../include/lvgl/core/lv_area.h"
#include "../../../include/lvgl/core/lv_matrix.h"

struct _lv_draw_evgpu_unit_t;

void lv_evgpu_solid_init(struct _lv_draw_evgpu_unit_t * unit);
void lv_evgpu_solid_deinit(struct _lv_draw_evgpu_unit_t * unit);

bool lv_evgpu_solid_fill_rect(struct _lv_draw_evgpu_unit_t * unit,
                             const lv_area_t * coords,
                             lv_color_t color,
                             lv_opa_t opa,
                             float radius,
                             const lv_area_t * clip_area,
                             const lv_matrix_t * matrix);

#endif /*LV_USE_DRAW_EVGPU*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_SOLID_H*/
