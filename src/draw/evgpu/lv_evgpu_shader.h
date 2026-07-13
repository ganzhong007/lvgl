/**
 * @file lv_evgpu_shader.h
 * @brief EVGPU native GLES2 shader programs (G1+ path, CP-01a skeleton).
 */

#ifndef LV_EVGPU_SHADER_H
#define LV_EVGPU_SHADER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_draw_evgpu_unit_t;
struct _lv_evgpu_context_t;

typedef struct _lv_evgpu_shader_t {
    uint32_t solid_program;
    int32_t solid_loc_matrix;
    int32_t solid_loc_color;
    uint32_t grad_program;
    uint32_t tex_program;
    bool ready;
    bool grad_ready;
    bool tex_ready;
} lv_evgpu_shader_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_evgpu_shader_init(struct _lv_draw_evgpu_unit_t * unit, struct _lv_evgpu_context_t * ctx,
                         lv_evgpu_shader_t * shader);
void lv_evgpu_shader_deinit(lv_evgpu_shader_t * shader);

bool lv_evgpu_shader_is_ready(const lv_evgpu_shader_t * shader);

bool lv_evgpu_shader_bind_solid(struct _lv_evgpu_context_t * ctx, lv_evgpu_shader_t * shader);

void lv_evgpu_shader_unbind(struct _lv_evgpu_context_t * ctx);

uint32_t lv_evgpu_shader_get_solid_program(const lv_evgpu_shader_t * shader);

bool lv_evgpu_shader_bind_grad(struct _lv_evgpu_context_t * ctx, lv_evgpu_shader_t * shader);

bool lv_evgpu_shader_grad_is_ready(const lv_evgpu_shader_t * shader);

bool lv_evgpu_shader_tex_is_ready(const lv_evgpu_shader_t * shader);

bool lv_evgpu_shader_bind_tex(struct _lv_evgpu_context_t * ctx, lv_evgpu_shader_t * shader);

#endif /*LV_USE_DRAW_EVGPU*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_SHADER_H*/
