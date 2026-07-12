/**
 * @file lv_g100_context.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_g100_context.h"

#if LV_USE_DRAW_G100

#include "lv_draw_g100_private.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_g100_context_init(struct _lv_draw_g100_unit_t * unit, lv_g100_context_t * ctx)
{
    LV_ASSERT_NULL(unit);
    LV_ASSERT_NULL(ctx);

    lv_memzero(ctx, sizeof(*ctx));
    ctx->unit = unit;
    ctx->ready = true;
}

void lv_g100_context_deinit(lv_g100_context_t * ctx)
{
    if(!ctx) return;
    lv_memzero(ctx, sizeof(*ctx));
}

bool lv_g100_context_is_ready(const lv_g100_context_t * ctx)
{
    return ctx && ctx->ready;
}

void lv_g100_context_set_viewport(lv_g100_context_t * ctx, int32_t w, int32_t h)
{
    if(!ctx) return;
    ctx->viewport_w = w;
    ctx->viewport_h = h;
}

void lv_g100_context_set_bound_program(lv_g100_context_t * ctx, uint32_t program)
{
    if(!ctx) return;
    ctx->bound_program = program;
}

uint32_t lv_g100_context_get_bound_program(const lv_g100_context_t * ctx)
{
    if(!ctx) return 0;
    return ctx->bound_program;
}

void lv_g100_context_set_draw_state(lv_g100_context_t * ctx, const lv_matrix_t * matrix,
                                    const lv_area_t * clip, int32_t viewport_w, int32_t viewport_h)
{
    if(!ctx) return;
    if(matrix) ctx->matrix = *matrix;
    if(clip) ctx->clip = *clip;
    ctx->viewport_w = viewport_w;
    ctx->viewport_h = viewport_h;
    ctx->draw_state_valid = matrix && clip;
}

#endif /*LV_USE_DRAW_G100*/
