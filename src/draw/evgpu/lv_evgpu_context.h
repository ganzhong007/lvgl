/**
 * @file lv_evgpu_context.h
 * @brief EVGPU GLES2 draw context (layer/FBO/program tracking skeleton).
 */

#ifndef LV_EVGPU_CONTEXT_H
#define LV_EVGPU_CONTEXT_H

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

typedef struct _lv_evgpu_context_t {
    struct _lv_draw_evgpu_unit_t * unit;
    bool ready;
    uint32_t bound_program;
    int32_t viewport_w;
    int32_t viewport_h;
    lv_matrix_t matrix;
    lv_area_t clip;
    bool draw_state_valid;
} lv_evgpu_context_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_evgpu_context_init(struct _lv_draw_evgpu_unit_t * unit, lv_evgpu_context_t * ctx);
void lv_evgpu_context_deinit(lv_evgpu_context_t * ctx);

bool lv_evgpu_context_is_ready(const lv_evgpu_context_t * ctx);

void lv_evgpu_context_set_viewport(lv_evgpu_context_t * ctx, int32_t w, int32_t h);

void lv_evgpu_context_set_bound_program(lv_evgpu_context_t * ctx, uint32_t program);

uint32_t lv_evgpu_context_get_bound_program(const lv_evgpu_context_t * ctx);

void lv_evgpu_context_set_draw_state(lv_evgpu_context_t * ctx, const lv_matrix_t * matrix,
                                    const lv_area_t * clip, int32_t viewport_w, int32_t viewport_h);

#endif /*LV_USE_DRAW_EVGPU*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_CONTEXT_H*/
