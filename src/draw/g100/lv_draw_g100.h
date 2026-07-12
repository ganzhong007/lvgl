/**
 * @file lv_draw_g100.h
 *
 */

#ifndef LV_DRAW_G100_H
#define LV_DRAW_G100_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100

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
 * Initialize DrawUnitG100 for G100 GLES2.0 hardware GPU.
 * Called from lv_opengles_init() after EGL context is ready.
 */
void lv_draw_g100_init(void);

/**********************
 *      MACROS
 **********************/

#endif /* LV_USE_DRAW_G100 */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_G100_H*/
