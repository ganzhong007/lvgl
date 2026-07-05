/**
 * @file lv_3dcamera.c
 */

#include "lv_3dcamera_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../../core/lv_obj_class_private.h"
#include "../../3d/lv_3d_internal.h"

#define MY_CLASS (&lv_3dcamera_class)

static void lv_3dcamera_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);

const lv_obj_class_t lv_3dcamera_class = {
    .constructor_cb = lv_3dcamera_constructor,
    .instance_size = sizeof(lv_3dcamera_t),
    .base_class = &lv_obj_class,
    .name = "3dcamera",
};

lv_obj_t * lv_3dcamera_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_3dcamera_set_perspective(lv_obj_t * obj, float fov_deg, float near_z, float far_z)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dcamera_t * cam = (lv_3dcamera_t *)obj;
    cam->fov_deg = fov_deg;
    cam->near_z = near_z;
    cam->far_z = far_z;
    lv_3d_camera_mark_dirty(obj);
}

void lv_3dcamera_look_at(lv_obj_t * obj, lv_vec3_t eye, lv_vec3_t target, lv_vec3_t up)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dcamera_t * cam = (lv_3dcamera_t *)obj;
    cam->eye = eye;
    cam->target = target;
    cam->up = up;
    lv_3d_camera_mark_dirty(obj);
}

static void lv_3dcamera_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dcamera_t * cam = (lv_3dcamera_t *)obj;
    cam->eye = (lv_vec3_t) { 0, 200, 800 };
    cam->target = (lv_vec3_t) { 0, 0, 0 };
    cam->up = (lv_vec3_t) { 0, 1, 0 };
    cam->fov_deg = 45.0f;
    cam->near_z = 10.0f;
    cam->far_z = 5000.0f;
    lv_obj_set_style_bg_opa(obj, LV_OPA_0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/
