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

#include "../core/lv_obj.h"

lv_obj_t * lv_3dviewport_create(lv_obj_t * parent);
void lv_3dviewport_set_camera(lv_obj_t * vp, lv_obj_t * camera);
void lv_3dviewport_set_scene(lv_obj_t * vp, lv_obj_t * scene);

#endif

#ifdef __cplusplus
}
#endif

#endif /*LV_3DVIEWPORT_H*/
