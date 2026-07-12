/**
 * @file lv_draw_3d_light.h
 *
 */

#ifndef LV_DRAW_3D_LIGHT_H
#define LV_DRAW_3D_LIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw.h"

#if LV_USE_3D_DRAW_TASKS

/*********************
 *      DEFINES
 *********************/

#define LV_3D_PASS_MAX_LIGHTS 4

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    LV_3D_LIGHT_TYPE_DIRECTIONAL = 0,
    LV_3D_LIGHT_TYPE_POINT,
} lv_3d_light_type_t;

typedef struct {
    lv_3d_light_type_t type;
    lv_color32_t color;
    float intensity;
    float direction[3];
    float position[3];
    float range;
} lv_3d_light_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_3d_pass_reset_lights(lv_layer_t * pass_layer);

bool lv_draw_3d_pass_add_light(lv_layer_t * pass_layer, const lv_3d_light_dsc_t * light);

uint32_t lv_draw_3d_pass_get_light_count(const lv_layer_t * pass_layer);

const lv_3d_light_dsc_t * lv_draw_3d_pass_get_lights(const lv_layer_t * pass_layer);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_LIGHT_H*/
