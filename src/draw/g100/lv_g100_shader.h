/**
 * @file lv_g100_shader.h
 * @brief G100 native GLES2 shader programs (G1+ path, CP-01a skeleton).
 */

#ifndef LV_G100_SHADER_H
#define LV_G100_SHADER_H

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
struct _lv_g100_context_t;

typedef struct _lv_g100_shader_t {
    uint32_t solid_program;
    int32_t solid_loc_matrix;
    int32_t solid_loc_color;
    uint32_t grad_program;
    bool ready;
    bool grad_ready;
} lv_g100_shader_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_g100_shader_init(struct _lv_draw_g100_unit_t * unit, struct _lv_g100_context_t * ctx,
                         lv_g100_shader_t * shader);
void lv_g100_shader_deinit(lv_g100_shader_t * shader);

bool lv_g100_shader_is_ready(const lv_g100_shader_t * shader);

bool lv_g100_shader_bind_solid(struct _lv_g100_context_t * ctx, lv_g100_shader_t * shader);

void lv_g100_shader_unbind(struct _lv_g100_context_t * ctx);

uint32_t lv_g100_shader_get_solid_program(const lv_g100_shader_t * shader);

bool lv_g100_shader_bind_grad(struct _lv_g100_context_t * ctx, lv_g100_shader_t * shader);

bool lv_g100_shader_grad_is_ready(const lv_g100_shader_t * shader);

#endif /*LV_USE_DRAW_G100*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_SHADER_H*/
