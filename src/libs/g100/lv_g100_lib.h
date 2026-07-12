/**
 * @file lv_g100_lib.h
 * @brief Optional G100 GLES2 runtime library (path B, CP-07b).
 */

#ifndef LV_G100_LIB_H
#define LV_G100_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100 && LV_USE_G100_LIB

struct _lv_draw_g100_unit_t;

typedef struct {
    bool context_ok;
    bool shader_ok;
} lv_g100_lib_caps_t;

void lv_g100_lib_init(struct _lv_draw_g100_unit_t * unit);
void lv_g100_lib_deinit(struct _lv_draw_g100_unit_t * unit);

bool lv_g100_lib_is_ready(void);

const lv_g100_lib_caps_t * lv_g100_lib_get_caps(void);

#endif /*LV_USE_DRAW_G100 && LV_USE_G100_LIB*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_LIB_H*/
