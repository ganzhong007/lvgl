/**
 * @file lv_evgpu_lib.h
 * @brief Optional EVGPU GLES2 runtime library (path B, CP-07b).
 */

#ifndef LV_EVGPU_LIB_H
#define LV_EVGPU_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU && LV_USE_EVGPU_LIB

struct _lv_draw_evgpu_unit_t;

typedef struct {
    bool context_ok;
    bool shader_ok;
} lv_evgpu_lib_caps_t;

void lv_evgpu_lib_init(struct _lv_draw_evgpu_unit_t * unit);
void lv_evgpu_lib_deinit(struct _lv_draw_evgpu_unit_t * unit);

bool lv_evgpu_lib_is_ready(void);

const lv_evgpu_lib_caps_t * lv_evgpu_lib_get_caps(void);

#endif /*LV_USE_DRAW_EVGPU && LV_USE_EVGPU_LIB*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_LIB_H*/
