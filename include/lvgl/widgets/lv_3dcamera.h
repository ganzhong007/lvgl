/**
 * @file lv_3dcamera.h
 */

#ifndef LV_3DCAMERA_H
#define LV_3DCAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../core/lv_obj.h"
#include "../3d/lv_3d.h"

lv_obj_t * lv_3dcamera_create(lv_obj_t * parent);
void lv_3dcamera_set_perspective(lv_obj_t * cam, float fov_deg, float near_z, float far_z);
void lv_3dcamera_look_at(lv_obj_t * cam, lv_vec3_t eye, lv_vec3_t target, lv_vec3_t up);

#endif

#ifdef __cplusplus
}
#endif

#endif /*LV_3DCAMERA_H*/
