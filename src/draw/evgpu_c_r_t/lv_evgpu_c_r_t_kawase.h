#ifndef LV_EVGPU_C_R_T_KAWASE_H
#define LV_EVGPU_C_R_T_KAWASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU_C_R_T

#include <stdint.h>

void lv_evgpu_c_r_t_kawase_init(void);
void lv_evgpu_c_r_t_kawase_deinit(void);
bool lv_evgpu_c_r_t_kawase_ready(void);

int lv_evgpu_c_r_t_kawase_blur_region(int dst_fb, int x, int y, int w, int h,
                                       int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha);

#endif

#ifdef __cplusplus
}
#endif

#endif
