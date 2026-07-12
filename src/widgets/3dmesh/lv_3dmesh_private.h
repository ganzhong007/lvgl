/**
 * @file lv_3dmesh_private.h
 *
 */

#ifndef LV_3DMESH_PRIVATE_H
#define LV_3DMESH_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_3DMESH

#include "../../core/lv_obj_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_mesh.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_3dmesh_t {
    lv_obj_t obj;
    float * vertices;
    float * normals;
    uint32_t vertex_count;
    uint16_t * indices;
    uint32_t index_count;
    lv_color32_t color;
    float translation[3];
    float rotation[3];
    float scale[3];
    lv_draw_3d_mesh_flags_t flags;
    bool phong;
    float shininess;
    float ambient;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_3dmesh_submit(lv_obj_t * obj, lv_layer_t * pass_layer);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3DMESH*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DMESH_PRIVATE_H*/
