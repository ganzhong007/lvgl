/**
 * @file lv_g100_blur_kawase.h
 * @brief Dual Kawase blur for large radii (G4 CP-04a).
 */

#ifndef LV_G100_BLUR_KAWASE_H
#define LV_G100_BLUR_KAWASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100

#include "../../libs/nanovg/nanovg.h"

struct NVGLUframebuffer;
struct _lv_draw_g100_unit_t;

void lv_g100_blur_kawase_init(struct _lv_draw_g100_unit_t * unit);
void lv_g100_blur_kawase_deinit(struct _lv_draw_g100_unit_t * unit);

bool lv_g100_blur_kawase_ready(void);

/**
 * Blur a region with Dual Kawase (radius may exceed 256).
 * @return 0 on success, -1 on failure.
 */
int lv_g100_blur_kawase_region(struct _lv_draw_g100_unit_t * unit, struct NVGLUframebuffer * fb,
                               int x, int y, int w, int h, int radius, const NVGcolor * recolor);

#endif /*LV_USE_DRAW_G100*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_BLUR_KAWASE_H*/
