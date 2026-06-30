/**
 * @file lv_3dbutton.c
 */

#include "lv_3dbutton_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON

#include "../../core/lv_obj_class_private.h"
#include "../../3d/lv_3d_internal.h"
#include "../../include/lvgl/widgets/lv_3dviewport.h"
#include "../../include/lvgl/widgets/lv_3dmesh.h"
#include "../../include/lvgl/draw/lv_color.h"

#define MY_CLASS (&lv_3dbutton_class)

static void lv_3dbutton_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_3dbutton_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void apply_visual(lv_3dbutton_t * btn, bool pressed);
static lv_3dbutton_t * btn_from_obj(lv_obj_t * obj);

const lv_obj_class_t lv_3dbutton_class = {
    .constructor_cb = lv_3dbutton_constructor,
    .event_cb = lv_3dbutton_event,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_TRUE,
    .instance_size = sizeof(lv_3dbutton_t),
    .base_class = &lv_3dmesh_class,
    .name = "3dbutton",
};

static lv_3dbutton_t * btn_from_obj(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return (lv_3dbutton_t *)obj;
}

static lv_color_t shade_color(lv_color_t c, lv_opa_t lighten_lvl)
{
    return lv_color_lighten(c, lighten_lvl);
}

static void sync_mesh_material(lv_3dbutton_t * btn)
{
    lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(btn->mesh_id);
    if(item) item->material = btn->material;
}

static void apply_visual(lv_3dbutton_t * btn, bool pressed)
{
    if(btn->is_pressed == pressed) return;
    btn->is_pressed = pressed;

    if(pressed) {
        btn->material.color = btn->side_pressed;
        btn->material.top_color = btn->top_pressed;
        float m = btn->press_scale_mul;
        btn->scale[0] = m;
        btn->scale[1] = m;
        btn->scale[2] = m;
    }
    else {
        btn->material.color = btn->side_released;
        btn->material.top_color = btn->top_released;
        btn->scale[0] = btn->scale[1] = btn->scale[2] = 1.0f;
    }

    sync_mesh_material(btn);
    lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(btn->mesh_id);
    if(item) {
        lv_3d_transform_set_local_trs(&item->transform,
                                      btn->pos[0], btn->pos[1], btn->pos[2],
                                      btn->rot[0], btn->rot[1], btn->rot[2],
                                      btn->scale[0], btn->scale[1], btn->scale[2]);
    }
    lv_obj_invalidate((lv_obj_t *)btn);
}

lv_obj_t * lv_3dbutton_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_3dbutton_set_box_size(lv_obj_t * obj, float w, float h, float depth)
{
    lv_3dbutton_t * btn = btn_from_obj(obj);
    lv_3dmesh_set_box(obj, w, h, depth);
    btn->material.kind = LV_3D_MAT_SHADED_BOX;
    sync_mesh_material(btn);
}

void lv_3dbutton_set_tilt(lv_obj_t * obj, float pitch_deg, float yaw_deg)
{
    lv_3dmesh_set_rotation(obj, pitch_deg, yaw_deg, 0.0f);
}

void lv_3dbutton_set_colors(lv_obj_t * obj, lv_color_t released, lv_color_t pressed)
{
    lv_3dbutton_t * btn = btn_from_obj(obj);
    btn->color_released = released;
    btn->color_pressed = pressed;
    btn->side_released = lv_color_darken(released, 36);
    btn->side_pressed = lv_color_darken(pressed, 28);
    btn->top_released = shade_color(released, 56);
    btn->top_pressed = shade_color(pressed, 40);
    btn->material.color = btn->is_pressed ? btn->side_pressed : btn->side_released;
    btn->material.top_color = btn->is_pressed ? btn->top_pressed : btn->top_released;
    sync_mesh_material(btn);
    lv_obj_invalidate(obj);
}

void lv_3dbutton_set_press_scale(lv_obj_t * obj, float scale_mul)
{
    lv_3dbutton_t * btn = btn_from_obj(obj);
    if(scale_mul < 0.5f) scale_mul = 0.5f;
    if(scale_mul > 1.0f) scale_mul = 1.0f;
    btn->press_scale_mul = scale_mul;
    if(btn->is_pressed) apply_visual(btn, true);
}

bool lv_3dbutton_is_pressed(const lv_obj_t * obj)
{
    if(!lv_obj_has_class(obj, MY_CLASS)) return false;
    return ((const lv_3dbutton_t *)obj)->is_pressed;
}

bool lv_3dbutton_hit_test(lv_obj_t * vp, lv_obj_t * btn, int32_t x, int32_t y)
{
    if(!vp || !btn) return false;
    return lv_3dviewport_pick_at(vp, x, y) == btn;
}

static void lv_3dbutton_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dbutton_t * btn = btn_from_obj(obj);

    btn->color_released = lv_color_hex(0x2196F3);
    btn->color_pressed = lv_color_hex(0x1565C0);
    btn->side_released = lv_color_darken(btn->color_released, 36);
    btn->side_pressed = lv_color_darken(btn->color_pressed, 28);
    btn->top_released = shade_color(btn->color_released, 56);
    btn->top_pressed = shade_color(btn->color_pressed, 40);
    btn->press_scale_mul = 0.96f;
    btn->is_pressed = false;

    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);

    lv_3dmesh_set_box(obj, 200.0f, 56.0f, 48.0f);
    lv_3dmesh_set_rotation(obj, -18.0f, 24.0f, 0.0f);
    lv_3dmesh_set_wireframe(obj, false);

    btn->material.kind = LV_3D_MAT_SHADED_BOX;
    btn->material.color = btn->side_released;
    btn->material.top_color = btn->top_released;
    btn->material.opa = LV_OPA_COVER;
    sync_mesh_material(btn);
}

static void lv_3dbutton_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_current_target(e);
    lv_3dbutton_t * btn = btn_from_obj(obj);

    if(code == LV_EVENT_PRESSED) {
        apply_visual(btn, true);
    }
    else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        apply_visual(btn, false);
    }

    lv_obj_event_base(MY_CLASS, e);
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON*/
