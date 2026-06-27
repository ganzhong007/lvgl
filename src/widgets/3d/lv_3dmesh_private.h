/**
 * @file lv_3dmesh_private.h
 */

#ifndef LV_3DMESH_PRIVATE_H
#define LV_3DMESH_PRIVATE_H

#include "../../lvgl_public.h"
#include "../../core/lv_obj_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../../3d/lv_3d_internal.h"

typedef struct {
    lv_obj_t obj;
    lv_3d_mesh_id_t mesh_id;
    lv_3d_material_t material;
    float pos[3];
    float rot[3];
    float scale[3];
} lv_3dmesh_t;

extern const lv_obj_class_t lv_3dmesh_class;

#endif

#endif /*LV_3DMESH_PRIVATE_H*/
