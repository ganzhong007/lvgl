/**
 * @file lv_style_3d.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../include/lvgl/misc/lv_style_3d.h"
#include "../lvgl_public.h"

#if LV_USE_3D_DRAW_TASKS

#include "../../include/lvgl/widgets/lv_3dviewport.h"
#include "../../include/lvgl/widgets/lv_3dmesh.h"
#include "../../include/lvgl/widgets/lv_3dlight.h"
#include "../widgets/3dviewport/lv_3dviewport_private.h"
#include "../widgets/3dmesh/lv_3dmesh_private.h"
#include "../widgets/3dlight/lv_3dlight_private.h"

/*********************
 *      DEFINES
 *********************/

#define STYLE_PART LV_PART_MAIN

/**********************
 *  STATIC VARIABLES
 **********************/

static bool s_inited;

lv_style_prop_t LV_STYLE_3D_CLEAR_COLOR;
lv_style_prop_t LV_STYLE_3D_CLEAR_OPA;
lv_style_prop_t LV_STYLE_3D_GRID_VISIBLE;
lv_style_prop_t LV_STYLE_3D_GRID_COLOR;
lv_style_prop_t LV_STYLE_3D_MESH_COLOR;
lv_style_prop_t LV_STYLE_3D_MESH_OPA;
lv_style_prop_t LV_STYLE_3D_SHININESS;
lv_style_prop_t LV_STYLE_3D_AMBIENT;
lv_style_prop_t LV_STYLE_3D_PHONG;
lv_style_prop_t LV_STYLE_3D_LIGHT_COLOR;
lv_style_prop_t LV_STYLE_3D_LIGHT_OPA;
lv_style_prop_t LV_STYLE_3D_LIGHT_INTENSITY;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_style_3d_init(void)
{
    if(s_inited) return;

    LV_STYLE_3D_CLEAR_COLOR = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_CLEAR_OPA = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_GRID_VISIBLE = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_GRID_COLOR = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_MESH_COLOR = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_MESH_OPA = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_SHININESS = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_AMBIENT = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_PHONG = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_LIGHT_COLOR = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_LIGHT_OPA = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);
    LV_STYLE_3D_LIGHT_INTENSITY = lv_style_register_prop(LV_STYLE_PROP_FLAG_NONE);

    s_inited = true;
    LV_LOG_INFO("G100 3D theme ready (LV_STYLE_3D_* props)");
}

void lv_3dstyle_apply_viewport(lv_obj_t * obj)
{
    if(!s_inited || obj == NULL) return;
    if(!lv_obj_check_type(obj, &lv_3dviewport_class)) return;

    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    bool changed = false;

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_CLEAR_COLOR)) {
        lv_color_t c = lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_CLEAR_COLOR).color;
        lv_opa_t opa = LV_OPA_COVER;
        if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_CLEAR_OPA)) {
            opa = (lv_opa_t)lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_CLEAR_OPA).num;
        }
        vp->clear_color = lv_color32_make(c.red, c.green, c.blue, opa);
        changed = true;
    }

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_GRID_VISIBLE)) {
        vp->show_grid = lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_GRID_VISIBLE).num != 0;
        changed = true;
    }

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_GRID_COLOR)) {
        lv_color_t c = lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_GRID_COLOR).color;
        vp->grid_color = lv_color32_make(c.red, c.green, c.blue, LV_OPA_COVER);
        changed = true;
    }

    if(changed) lv_obj_invalidate(obj);
}

void lv_3dstyle_apply_mesh(lv_obj_t * obj)
{
    if(!s_inited || obj == NULL) return;
    if(!lv_obj_check_type(obj, &lv_3dmesh_class)) return;

    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    bool changed = false;

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_MESH_COLOR)) {
        lv_color_t c = lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_MESH_COLOR).color;
        lv_opa_t opa = LV_OPA_COVER;
        if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_MESH_OPA)) {
            opa = (lv_opa_t)lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_MESH_OPA).num;
        }
        mesh->color = lv_color32_make(c.red, c.green, c.blue, opa);
        changed = true;
    }

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_SHININESS)) {
        mesh->shininess = LV_3D_STYLE_NUM_TO_SHININESS(
                              lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_SHININESS).num);
        changed = true;
    }

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_AMBIENT)) {
        mesh->ambient = LV_3D_STYLE_NUM_TO_AMBIENT(
                            lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_AMBIENT).num);
        changed = true;
    }

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_PHONG)) {
        mesh->phong = lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_PHONG).num != 0;
        changed = true;
    }

    if(changed) {
        lv_obj_t * parent = lv_obj_get_parent(obj);
        if(parent) lv_obj_invalidate(parent);
    }
}

void lv_3dstyle_apply_light(lv_obj_t * obj)
{
    if(!s_inited || obj == NULL) return;
    if(!lv_obj_check_type(obj, &lv_3dlight_class)) return;

    lv_3dlight_t * light = (lv_3dlight_t *)obj;
    bool changed = false;

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_LIGHT_COLOR)) {
        lv_color_t c = lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_LIGHT_COLOR).color;
        lv_opa_t opa = LV_OPA_COVER;
        if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_LIGHT_OPA)) {
            opa = (lv_opa_t)lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_LIGHT_OPA).num;
        }
        light->dsc.color = lv_color32_make(c.red, c.green, c.blue, opa);
        changed = true;
    }

    if(lv_obj_has_style_prop(obj, STYLE_PART, LV_STYLE_3D_LIGHT_INTENSITY)) {
        light->dsc.intensity = LV_3D_STYLE_NUM_TO_INTENSITY(
                                   lv_obj_get_style_prop(obj, STYLE_PART, LV_STYLE_3D_LIGHT_INTENSITY).num);
        changed = true;
    }

    if(changed) {
        lv_obj_t * parent = lv_obj_get_parent(obj);
        if(parent) lv_obj_invalidate(parent);
    }
}

#endif /*LV_USE_3D_DRAW_TASKS*/
