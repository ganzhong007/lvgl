/**
 * @file lv_draw_3d_mesh.h
 *
 */

#ifndef LV_DRAW_3D_MESH_H
#define LV_DRAW_3D_MESH_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw.h"
#include "lv_draw_3d_camera.h"

#if LV_USE_3D_DRAW_TASKS

/*********************
 *      DEFINES
 *********************/

#define LV_3D_MESH_MODEL_SIZE 16

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    LV_3D_MESH_FLAG_NONE       = 0,
    LV_3D_MESH_FLAG_DEPTH_TEST = 1 << 0,
    LV_3D_MESH_FLAG_CULL_FACE  = 1 << 1,
    LV_3D_MESH_FLAG_PHONG      = 1 << 2,
} lv_draw_3d_mesh_flags_t;

typedef struct {
    lv_draw_dsc_base_t base;
    const float * vertices;
    uint32_t vertex_count;
    const float * normals;
    const uint16_t * indices;
    uint32_t index_count;
    lv_color32_t color;
    float model_matrix[LV_3D_MESH_MODEL_SIZE];
    lv_draw_3d_mesh_flags_t flags;
    float shininess;
    float ambient;
} lv_draw_3d_mesh_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_3d_mesh_dsc_init(lv_draw_3d_mesh_dsc_t * dsc);

lv_draw_3d_mesh_dsc_t * lv_draw_task_get_3d_mesh_dsc(lv_draw_task_t * task);

void lv_draw_3d_mesh(lv_layer_t * pass_layer, const lv_draw_3d_mesh_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_3D_MESH_H*/
