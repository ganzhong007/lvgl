/**
 * @file lv_evgpu_blur_kawase.h
 * @brief Dual Kawase blur for large radii (G4 CP-04a).
 */

#ifndef LV_EVGPU_BLUR_KAWASE_H
#define LV_EVGPU_BLUR_KAWASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

#include "../../libs/evgpu/evgpu_evgr.h"

struct EVGRLUframebuffer;
struct _lv_draw_evgpu_unit_t;

void lv_evgpu_blur_kawase_init(struct _lv_draw_evgpu_unit_t * unit);
void lv_evgpu_blur_kawase_deinit(struct _lv_draw_evgpu_unit_t * unit);

bool lv_evgpu_blur_kawase_ready(void);

/**
 * Blur a region with Dual Kawase (radius may exceed 256).
 * @return 0 on success, -1 on failure.
 */
int lv_evgpu_blur_kawase_region(struct _lv_draw_evgpu_unit_t * unit, struct EVGRLUframebuffer * fb,
                               int x, int y, int w, int h, int radius, const EVGRcolor * recolor);

#endif /*LV_USE_DRAW_EVGPU*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_BLUR_KAWASE_H*/
