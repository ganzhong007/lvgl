/**
 * @file lv_draw_evgpu.h
 *
 */

#ifndef LV_DRAW_EVGPU_H
#define LV_DRAW_EVGPU_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize DrawUnitEVGPU for EVGPU GLES2.0 hardware GPU.
 * Called from lv_opengles_init() after EGL context is ready.
 */
void lv_draw_evgpu_init(void);

/**********************
 *      MACROS
 **********************/

#endif /* LV_USE_DRAW_EVGPU */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_EVGPU_H*/
