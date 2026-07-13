/**
 * @file lv_evgpu_context.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_evgpu_context.h"

#if LV_USE_DRAW_EVGPU

#include "lv_draw_evgpu_private.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_evgpu_context_init(struct _lv_draw_evgpu_unit_t * unit, lv_evgpu_context_t * ctx)
{
    LV_ASSERT_NULL(unit);
    LV_ASSERT_NULL(ctx);

    lv_memzero(ctx, sizeof(*ctx));
    ctx->unit = unit;
    ctx->ready = true;
}

void lv_evgpu_context_deinit(lv_evgpu_context_t * ctx)
{
    if(!ctx) return;
    lv_memzero(ctx, sizeof(*ctx));
}

bool lv_evgpu_context_is_ready(const lv_evgpu_context_t * ctx)
{
    return ctx && ctx->ready;
}

void lv_evgpu_context_set_viewport(lv_evgpu_context_t * ctx, int32_t w, int32_t h)
{
    if(!ctx) return;
    ctx->viewport_w = w;
    ctx->viewport_h = h;
}

void lv_evgpu_context_set_bound_program(lv_evgpu_context_t * ctx, uint32_t program)
{
    if(!ctx) return;
    ctx->bound_program = program;
}

uint32_t lv_evgpu_context_get_bound_program(const lv_evgpu_context_t * ctx)
{
    if(!ctx) return 0;
    return ctx->bound_program;
}

void lv_evgpu_context_set_draw_state(lv_evgpu_context_t * ctx, const lv_matrix_t * matrix,
                                    const lv_area_t * clip, int32_t viewport_w, int32_t viewport_h)
{
    if(!ctx) return;
    if(matrix) ctx->matrix = *matrix;
    if(clip) ctx->clip = *clip;
    ctx->viewport_w = viewport_w;
    ctx->viewport_h = viewport_h;
    ctx->draw_state_valid = matrix && clip;
}

#endif /*LV_USE_DRAW_EVGPU*/
