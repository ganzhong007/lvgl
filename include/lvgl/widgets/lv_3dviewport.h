/**
 * @file lv_3dviewport.h
 */

#ifndef LV_3DVIEWPORT_H
#define LV_3DVIEWPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../3d/lv_3d.h"

lv_obj_t * lv_3dviewport_create(lv_obj_t * parent);
void lv_3dviewport_set_camera(lv_obj_t * vp, lv_obj_t * camera);
void lv_3dviewport_set_scene(lv_obj_t * vp, lv_obj_t * scene);
void lv_3dviewport_set_pickable(lv_obj_t * vp, bool en);
lv_obj_t * lv_3dviewport_pick_obj(lv_obj_t * vp, lv_point3d_t ray_origin, lv_vec3_t ray_dir);
lv_obj_t * lv_3dviewport_pick_at(lv_obj_t * vp, int32_t x, int32_t y);

#endif

#ifdef __cplusplus
}
#endif

#endif /*LV_3DVIEWPORT_H*/
