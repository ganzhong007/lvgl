/**
 * @file lv_3dmesh.h
 */

#ifndef LV_3DMESH_H
#define LV_3DMESH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../core/lv_obj.h"
#include "../3d/lv_3d.h"

lv_obj_t * lv_3dmesh_create(lv_obj_t * parent);
void lv_3dmesh_set_box(lv_obj_t * mesh, float w, float h, float d);
void lv_3dmesh_set_wireframe(lv_obj_t * mesh, bool en);
void lv_3dmesh_set_color(lv_obj_t * mesh, lv_color_t c);
void lv_3dmesh_set_material(lv_obj_t * mesh, const lv_3d_material_t * mat);
void lv_3dmesh_set_position(lv_obj_t * mesh, float x, float y, float z);
void lv_3dmesh_set_rotation_y(lv_obj_t * mesh, float yaw_deg);
void lv_3dmesh_set_scale(lv_obj_t * mesh, float sx, float sy, float sz);

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/

#ifdef __cplusplus
}
#endif

#endif /*LV_3DMESH_H*/
