/**
 * @file lv_draw_3d_scene.h
 *
 */

#ifndef LV_DRAW_3D_SCENE_H
#define LV_DRAW_3D_SCENE_H

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

typedef void * lv_3d_scene_id_t;

typedef enum {
    LV_3D_SCENE_FLAG_NONE = 0,
} lv_3d_scene_flags_t;

typedef struct {
    lv_draw_dsc_base_t base;
    lv_3d_scene_id_t scene_id;
    lv_3d_scene_flags_t flags;
} lv_draw_3d_scene_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_3d_scene_dsc_init(lv_draw_3d_scene_dsc_t * dsc);

lv_draw_3d_scene_dsc_t * lv_draw_task_get_3d_scene_dsc(lv_draw_task_t * task);

void lv_draw_3d_scene(lv_layer_t * pass_layer, const lv_draw_3d_scene_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_SCENE_H*/
