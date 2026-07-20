/**
 * @file lv_draw_3d_light.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_light.h"

#if LV_USE_3D_DRAW_TASKS

#if LV_USE_DRAW_EVGPU || LV_USE_DRAW_EVGPU_C_R_T
#include "lv_draw_3d_pass.h"
#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_3d_pass_reset_lights(lv_layer_t * pass_layer)
{
#if LV_USE_DRAW_EVGPU || LV_USE_DRAW_EVGPU_C_R_T
    if(!lv_evgpu_3d_pass_layer_is(pass_layer)) return;
    lv_3d_pass_layer_ud_t * ud = pass_layer->user_data;
    ud->light_count = 0;
#else
    LV_UNUSED(pass_layer);
#endif
}

bool lv_draw_3d_pass_add_light(lv_layer_t * pass_layer, const lv_3d_light_dsc_t * light)
{
#if LV_USE_DRAW_EVGPU || LV_USE_DRAW_EVGPU_C_R_T
    if(!lv_evgpu_3d_pass_layer_is(pass_layer) || light == NULL) return false;

    lv_3d_pass_layer_ud_t * ud = pass_layer->user_data;
    if(ud->light_count >= LV_3D_PASS_MAX_LIGHTS) return false;

    ud->lights[ud->light_count++] = *light;
    return true;
#else
    LV_UNUSED(pass_layer);
    LV_UNUSED(light);
    return false;
#endif
}

uint32_t lv_draw_3d_pass_get_light_count(const lv_layer_t * pass_layer)
{
#if LV_USE_DRAW_EVGPU || LV_USE_DRAW_EVGPU_C_R_T
    if(!lv_evgpu_3d_pass_layer_is(pass_layer)) return 0;
    lv_3d_pass_layer_ud_t * ud = pass_layer->user_data;
    return ud->light_count;
#else
    LV_UNUSED(pass_layer);
    return 0;
#endif
}

const lv_3d_light_dsc_t * lv_draw_3d_pass_get_lights(const lv_layer_t * pass_layer)
{
#if LV_USE_DRAW_EVGPU || LV_USE_DRAW_EVGPU_C_R_T
    if(!lv_evgpu_3d_pass_layer_is(pass_layer)) return NULL;
    lv_3d_pass_layer_ud_t * ud = pass_layer->user_data;
    return ud->lights;
#else
    LV_UNUSED(pass_layer);
    return NULL;
#endif
}

#endif /*LV_USE_3D_DRAW_TASKS*/
