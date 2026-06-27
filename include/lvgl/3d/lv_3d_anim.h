/**
 * @file lv_3d_anim.h — 3D mesh property animations
 */

#ifndef LV_3D_ANIM_H
#define LV_3D_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../core/lv_obj.h"

void lv_anim_3d_position(lv_obj_t * obj, float x, float y, float z, uint32_t ms);
void lv_anim_3d_rotation(lv_obj_t * obj, float pitch, float yaw, float roll, uint32_t ms);
void lv_anim_3d_scale(lv_obj_t * obj, float sx, float sy, float sz, uint32_t ms);
void lv_anim_3d_opa(lv_obj_t * obj, lv_opa_t opa, uint32_t ms);

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/

#ifdef __cplusplus
}
#endif

#endif /*LV_3D_ANIM_H*/
