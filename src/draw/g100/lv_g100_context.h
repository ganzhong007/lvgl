/**
 * @file lv_g100_context.h
 * @brief G100 GLES2 draw context (layer/FBO/program tracking skeleton).
 */

#ifndef LV_G100_CONTEXT_H
#define LV_G100_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_draw_g100_unit_t;

typedef struct _lv_g100_context_t {
    struct _lv_draw_g100_unit_t * unit;
    bool ready;
    uint32_t bound_program;
    int32_t viewport_w;
    int32_t viewport_h;
} lv_g100_context_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_g100_context_init(struct _lv_draw_g100_unit_t * unit, lv_g100_context_t * ctx);
void lv_g100_context_deinit(lv_g100_context_t * ctx);

bool lv_g100_context_is_ready(const lv_g100_context_t * ctx);

void lv_g100_context_set_viewport(lv_g100_context_t * ctx, int32_t w, int32_t h);

void lv_g100_context_set_bound_program(lv_g100_context_t * ctx, uint32_t program);

uint32_t lv_g100_context_get_bound_program(const lv_g100_context_t * ctx);

#endif /*LV_USE_DRAW_G100*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_CONTEXT_H*/
