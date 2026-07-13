/**
 * @file lv_evgpu_grad.h
 * @brief EVGPU native GLES2 gradient fill (G1 CP-01b, decoupled from LV_USE_VECTOR_GRAPHIC).
 */

#ifndef LV_EVGPU_GRAD_H
#define LV_EVGPU_GRAD_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"
#include "../../../include/lvgl/draw/lv_grad.h"
#include "../../../include/lvgl/core/lv_area.h"
#include "../../../include/lvgl/core/lv_matrix.h"

#if LV_USE_DRAW_EVGPU

/**********************
 * GLOBAL PROTOTYPES
 **********************/

struct _lv_draw_evgpu_unit_t;

/**
 * Initialize native gradient resources (VBO etc.).
 */
void lv_evgpu_grad_init(struct _lv_draw_evgpu_unit_t * unit);

/**
 * Release native gradient resources.
 */
void lv_evgpu_grad_deinit(struct _lv_draw_evgpu_unit_t * unit);

/**
 * Draw a gradient-filled axis-aligned rectangle with optional corner radius.
 * @return true if drawn on GPU; false if native path unavailable (caller may fall back).
 */
bool lv_evgpu_grad_fill_rect(struct _lv_draw_evgpu_unit_t * unit,
                            const lv_area_t * coords,
                            const lv_grad_dsc_t * grad_dsc,
                            float radius,
                            const lv_area_t * clip_area,
                            const lv_matrix_t * matrix);

#endif /*LV_USE_DRAW_EVGPU*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_GRAD_H*/
