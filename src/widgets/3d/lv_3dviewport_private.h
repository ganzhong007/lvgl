/**
 * @file lv_3dviewport_private.h
 */

#ifndef LV_3DVIEWPORT_PRIVATE_H
#define LV_3DVIEWPORT_PRIVATE_H

#include "../../lvgl_public.h"
#include "../../core/lv_obj_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

typedef struct {
    lv_obj_t obj;
    lv_obj_t * camera;
    lv_obj_t * scene;
    bool pickable;
    bool input_route;
    lv_obj_t * pressed_obj;
    lv_obj_t * hovered_obj;
} lv_3dviewport_t;

extern const lv_obj_class_t lv_3dviewport_class;

#endif

#endif /*LV_3DVIEWPORT_PRIVATE_H*/
