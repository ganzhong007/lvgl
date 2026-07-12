/**
 * @file lv_3dmesh.h
 *
 */

#ifndef LV_3DMESH_H
#define LV_3DMESH_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../config/lv_conf_internal.h"
#if LV_USE_3DMESH

#include "../core/lv_obj.h"
#include "../draw/lv_draw_3d_mesh.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_3dmesh_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_obj_t * lv_3dmesh_create(lv_obj_t * parent);

void lv_3dmesh_set_color(lv_obj_t * obj, lv_color_t color, lv_opa_t opa);

void lv_3dmesh_set_transform(lv_obj_t * obj, float tx, float ty, float tz,
                             float rot_x, float rot_y, float rot_z,
                             float sx, float sy, float sz);

void lv_3dmesh_set_box(lv_obj_t * obj, float sx, float sy, float sz);

void lv_3dmesh_set_depth_test(lv_obj_t * obj, bool enable);

void lv_3dmesh_set_cull_face(lv_obj_t * obj, bool enable);

void lv_3dmesh_submit_tree(lv_obj_t * root, lv_layer_t * pass_layer);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3DMESH*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DMESH_H*/
