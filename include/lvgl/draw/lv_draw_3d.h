/**
 * @file lv_draw_3d.h
 *
 */

#ifndef LV_DRAW_3D_H
#define LV_DRAW_3D_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw.h"

#if LV_USE_3DTEXTURE || LV_USE_3D

#if LV_USE_3D
#include "../3d/lv_3d.h"
#endif

/*********************
 *      DEFINES
 *********************/

#if LV_USE_3D
typedef enum {
    LV_3D_DRAW_KIND_LEGACY_TEX = 0,
    LV_3D_DRAW_KIND_MESH,
    LV_3D_DRAW_KIND_VIEWPORT_PASS,
} lv_3d_draw_kind_t;
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    lv_draw_dsc_base_t base;
#if LV_USE_3D
    lv_3d_draw_kind_t kind;
    lv_3d_mesh_id_t mesh_id;
    lv_3d_material_t material;
    float model[16];
    lv_obj_t * camera;
    lv_obj_t * scene;
    lv_area_t viewport_area;
#endif
#if LV_USE_3DTEXTURE
    lv_3dtexture_id_t tex_id;
    bool h_flip;
    bool v_flip;
#endif
    lv_opa_t opa;
} lv_draw_3d_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_3d_dsc_init(lv_draw_3d_dsc_t * dsc);

lv_draw_3d_dsc_t * lv_draw_task_get_3d_dsc(lv_draw_task_t * task);

void lv_draw_3d(lv_layer_t * layer, const lv_draw_3d_dsc_t * dsc, const lv_area_t * coords);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3DTEXTURE || LV_USE_3D*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_H*/
