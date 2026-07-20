/**
 * @file lv_draw_3d_pass.h
 * Shared 3D pass-layer FBO helpers used by EVGPU and EVGPU_C_R_T DrawUnits.
 */

#ifndef LV_DRAW_3D_PASS_H
#define LV_DRAW_3D_PASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lvgl_public.h"
#include "../../include/lvgl/draw/lv_draw_3d_camera.h"
#include "../../include/lvgl/draw/lv_draw_3d_light.h"

#if (LV_USE_DRAW_EVGPU || LV_USE_DRAW_EVGPU_C_R_T) && LV_USE_3D_DRAW_TASKS

#define LV_3D_PASS_LAYER_MAGIC 0x3D505052u

typedef struct {
    uint32_t framebuffer;
    uint32_t color_tex;
    uint32_t depth_tex;
    int32_t w;
    int32_t h;
} lv_evgpu_3d_pass_fbo_t;

typedef struct {
    lv_evgpu_3d_pass_fbo_t fbo;
} lv_3d_pass_t;

typedef struct {
    uint32_t magic;
    lv_3d_pass_t pass;
    lv_3d_camera_t camera;
    float view_proj[LV_3D_CAMERA_MVP_SIZE];
    bool camera_valid;
    lv_3d_light_dsc_t lights[LV_3D_PASS_MAX_LIGHTS];
    uint32_t light_count;
} lv_3d_pass_layer_ud_t;

bool lv_evgpu_3d_pass_layer_is(const lv_layer_t * layer);

lv_3d_pass_t * lv_evgpu_3d_pass_from_layer(const lv_layer_t * layer);

lv_result_t lv_evgpu_3d_pass_ensure(lv_3d_pass_t * pass, int32_t w, int32_t h);

void lv_evgpu_3d_pass_destroy_fbo(lv_3d_pass_t * pass);

void lv_evgpu_3d_pass_layer_destroy(lv_layer_t * pass_layer, lv_display_t * disp);

void lv_evgpu_3d_pass_set_camera(lv_layer_t * pass_layer, const lv_3d_camera_t * camera);

const lv_3d_camera_t * lv_draw_3d_pass_get_camera(const lv_layer_t * pass_layer);

const float * lv_draw_3d_pass_get_view_proj(const lv_layer_t * pass_layer);

bool lv_evgpu_3d_pass_bind_fbo(lv_layer_t * pass_layer, int32_t * w, int32_t * h, lv_evgpu_3d_pass_fbo_t ** fbo);

void lv_evgpu_3d_pass_unbind_fbo(void);

#endif /* (LV_USE_DRAW_EVGPU || LV_USE_DRAW_EVGPU_C_R_T) && LV_USE_3D_DRAW_TASKS */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_PASS_H*/
