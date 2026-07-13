/**
 * @file lv_evgpu_fbo_pool.h
 * @brief Grow-only FBO pool for blur ping-pong (G4 CP-04b).
 */

#ifndef LV_EVGPU_FBO_POOL_H
#define LV_EVGPU_FBO_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

struct EVGRLUframebuffer;
struct _lv_draw_evgpu_unit_t;

void lv_evgpu_fbo_pool_init(struct _lv_draw_evgpu_unit_t * unit);
void lv_evgpu_fbo_pool_deinit(struct _lv_draw_evgpu_unit_t * unit);

bool lv_evgpu_fbo_pool_is_ok(void);

/**
 * Acquire a pooled FBO at least @p w x @p h (slot 0 or 1 for ping-pong).
 */
struct EVGRLUframebuffer * lv_evgpu_fbo_pool_acquire(struct _lv_draw_evgpu_unit_t * unit, int w, int h,
                                                   uint32_t slot);

#endif /*LV_USE_DRAW_EVGPU*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_FBO_POOL_H*/
