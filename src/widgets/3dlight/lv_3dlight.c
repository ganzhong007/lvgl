/**
 * @file lv_3dlight.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_3dlight_private.h"
#include "../../lvgl_public.h"

#if LV_USE_3DLIGHT

#include "../../core/lv_obj_class_private.h"

#include <math.h>

/*********************
 *      DEFINES
 *********************/

#define MY_CLASS (&lv_3dlight_class)

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void lv_3dlight_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void normalize3(float v[3]);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t lv_3dlight_class = {
    .constructor_cb = lv_3dlight_constructor,
    .width_def = 0,
    .height_def = 0,
    .instance_size = sizeof(lv_3dlight_t),
    .base_class = &lv_obj_class,
    .name = "3dlight",
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_3dlight_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(obj, 1, 1);
    return obj;
}

void lv_3dlight_set_directional(lv_obj_t * obj, float dx, float dy, float dz,
                                lv_color_t color, lv_opa_t opa, float intensity)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dlight_t * light = (lv_3dlight_t *)obj;
    light->dsc.type = LV_3D_LIGHT_TYPE_DIRECTIONAL;
    light->dsc.direction[0] = dx;
    light->dsc.direction[1] = dy;
    light->dsc.direction[2] = dz;
    normalize3(light->dsc.direction);
    light->dsc.color = lv_color32_make(color.red, color.green, color.blue, opa);
    light->dsc.intensity = intensity;
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dlight_set_point(lv_obj_t * obj, float x, float y, float z,
                          lv_color_t color, lv_opa_t opa, float intensity, float range)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dlight_t * light = (lv_3dlight_t *)obj;
    light->dsc.type = LV_3D_LIGHT_TYPE_POINT;
    light->dsc.position[0] = x;
    light->dsc.position[1] = y;
    light->dsc.position[2] = z;
    light->dsc.range = range;
    light->dsc.color = lv_color32_make(color.red, color.green, color.blue, opa);
    light->dsc.intensity = intensity;
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dlight_submit_tree(lv_obj_t * root, lv_layer_t * pass_layer)
{
    if(root == NULL || pass_layer == NULL) return;

    uint32_t i;
    uint32_t cnt = lv_obj_get_child_count(root);
    for(i = 0; i < cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(root, i);
        if(lv_obj_check_type(child, &lv_3dlight_class)) {
            lv_3dlight_submit(child, pass_layer);
        }
        lv_3dlight_submit_tree(child, pass_layer);
    }
}

void lv_3dlight_submit(lv_obj_t * obj, lv_layer_t * pass_layer)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dlight_t * light = (lv_3dlight_t *)obj;
    lv_draw_3d_pass_add_light(pass_layer, &light->dsc);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_3dlight_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dlight_t * light = (lv_3dlight_t *)obj;
    light->dsc.type = LV_3D_LIGHT_TYPE_DIRECTIONAL;
    light->dsc.direction[0] = 0.4f;
    light->dsc.direction[1] = -1.f;
    light->dsc.direction[2] = 0.3f;
    normalize3(light->dsc.direction);
    light->dsc.color = lv_color32_make(0xFF, 0xFF, 0xFF, LV_OPA_COVER);
    light->dsc.intensity = 1.f;
    light->dsc.range = 0.f;
}

static void normalize3(float v[3])
{
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if(len > 0.f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

#endif /*LV_USE_3DLIGHT*/
