/**
 * @file lv_g100_grad.h
 * @brief G100 native GLES2 gradient fill (G1 CP-01b, decoupled from LV_USE_VECTOR_GRAPHIC).
 */

#ifndef LV_G100_GRAD_H
#define LV_G100_GRAD_H

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

#if LV_USE_DRAW_G100

/**********************
 * GLOBAL PROTOTYPES
 **********************/

struct _lv_draw_g100_unit_t;

/**
 * Initialize native gradient resources (VBO etc.).
 */
void lv_g100_grad_init(struct _lv_draw_g100_unit_t * unit);

/**
 * Release native gradient resources.
 */
void lv_g100_grad_deinit(struct _lv_draw_g100_unit_t * unit);

/**
 * Draw a gradient-filled axis-aligned rectangle with optional corner radius.
 * @return true if drawn on GPU; false if native path unavailable (caller may fall back).
 */
bool lv_g100_grad_fill_rect(struct _lv_draw_g100_unit_t * unit,
                            const lv_area_t * coords,
                            const lv_grad_dsc_t * grad_dsc,
                            float radius,
                            const lv_area_t * clip_area,
                            const lv_matrix_t * matrix);

#endif /*LV_USE_DRAW_G100*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_GRAD_H*/
