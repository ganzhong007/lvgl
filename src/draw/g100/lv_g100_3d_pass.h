/**
 * @file lv_g100_3d_pass.h
 *
 */

#ifndef LV_G100_3D_PASS_H
#define LV_G100_3D_PASS_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS

/*********************
 *      DEFINES
 *********************/

#define LV_3D_PASS_LAYER_MAGIC 0x3D505052u

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    uint32_t framebuffer;
    uint32_t color_tex;
    uint32_t depth_tex;
    int32_t w;
    int32_t h;
} lv_g100_3d_pass_fbo_t;

typedef struct {
    lv_g100_3d_pass_fbo_t fbo;
} lv_3d_pass_t;

typedef struct {
    uint32_t magic;
    lv_3d_pass_t pass;
} lv_3d_pass_layer_ud_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

bool lv_g100_3d_pass_layer_is(const lv_layer_t * layer);

lv_3d_pass_t * lv_g100_3d_pass_from_layer(const lv_layer_t * layer);

lv_result_t lv_g100_3d_pass_ensure(lv_3d_pass_t * pass, int32_t w, int32_t h);

void lv_g100_3d_pass_destroy_fbo(lv_3d_pass_t * pass);

void lv_g100_3d_pass_layer_destroy(lv_layer_t * pass_layer, lv_display_t * disp);

/**********************
 *      MACROS
 **********************/

#endif /* LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_3D_PASS_H*/
