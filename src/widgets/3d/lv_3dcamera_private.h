/**
 * @file lv_3dcamera_private.h
 */

#ifndef LV_3DCAMERA_PRIVATE_H
#define LV_3DCAMERA_PRIVATE_H

#include "../../lvgl_public.h"
#include "../../core/lv_obj_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

typedef struct {
    lv_obj_t obj;
    lv_vec3_t eye;
    lv_vec3_t target;
    lv_vec3_t up;
    float fov_deg;
    float near_z;
    float far_z;
} lv_3dcamera_t;

extern const lv_obj_class_t lv_3dcamera_class;

#endif

#endif /*LV_3DCAMERA_PRIVATE_H*/
