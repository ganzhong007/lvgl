/**
 * @file lv_g100_fbo_pool.h
 * @brief Grow-only FBO pool for blur ping-pong (G4 CP-04b).
 */

#ifndef LV_G100_FBO_POOL_H
#define LV_G100_FBO_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100

struct NVGLUframebuffer;
struct _lv_draw_g100_unit_t;

void lv_g100_fbo_pool_init(struct _lv_draw_g100_unit_t * unit);
void lv_g100_fbo_pool_deinit(struct _lv_draw_g100_unit_t * unit);

bool lv_g100_fbo_pool_is_ok(void);

/**
 * Acquire a pooled FBO at least @p w x @p h (slot 0 or 1 for ping-pong).
 */
struct NVGLUframebuffer * lv_g100_fbo_pool_acquire(struct _lv_draw_g100_unit_t * unit, int w, int h,
                                                   uint32_t slot);

#endif /*LV_USE_DRAW_G100*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_FBO_POOL_H*/
