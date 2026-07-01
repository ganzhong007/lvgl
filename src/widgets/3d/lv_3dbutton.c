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
static void apply_visual_state(lv_3dbutton_t * btn, lv_3dbutton_vis_t vis);
static void sync_transform(lv_3dbutton_t * btn);
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

static void sync_transform(lv_3dbutton_t * btn)
{
    lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(btn->mesh_id);
    if(!item) return;
    lv_3d_transform_set_local_trs(&item->transform,
                                  btn->pos[0], btn->pos[1], btn->pos[2],
                                  btn->rot[0], btn->rot[1], btn->rot[2],
                                  btn->scale[0], btn->scale[1], btn->scale[2]);
}

static void apply_visual_state(lv_3dbutton_t * btn, lv_3dbutton_vis_t vis)
{
    if(btn->vis == vis) return;
    btn->vis = vis;

    float y_off = 0.0f;
    float sc = 1.0f;
    lv_color_t side = btn->side_released;
    lv_color_t top = btn->top_released;

    switch(vis) {
        case LV_3DBUTTON_VIS_PRESSED:
            y_off = btn->hover_lift_z + btn->press_sink_z;
            sc = btn->press_scale_mul;
            side = btn->side_pressed;
            top = btn->top_pressed;
            break;
        case LV_3DBUTTON_VIS_HOVER:
            y_off = btn->hover_lift_z;
            sc = btn->hover_scale_mul;
            side = btn->side_hover;
            top = btn->top_hover;
            break;
        default:
            break;
    }

    btn->material.color = side;
    btn->material.top_color = top;
    btn->scale[0] = btn->scale[1] = btn->scale[2] = sc;
    btn->pos[0] = btn->base_pos[0];
    btn->pos[1] = btn->base_pos[1] + y_off;
    btn->pos[2] = btn->base_pos[2];

    sync_mesh_material(btn);
    sync_transform(btn);
    lv_obj_invalidate((lv_obj_t *)btn);
}

static void refresh_visual(lv_3dbutton_t * btn)
{
    lv_3dbutton_vis_t vis = LV_3DBUTTON_VIS_NORMAL;
    if(btn->is_pressed) vis = LV_3DBUTTON_VIS_PRESSED;
    else if(btn->hovered) vis = LV_3DBUTTON_VIS_HOVER;
    apply_visual_state(btn, vis);
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
    btn->material.corner_radius = btn->corner_radius;
    sync_mesh_material(btn);
}

void lv_3dbutton_set_corner_radius(lv_obj_t * obj, float radius)
{
    lv_3dbutton_t * btn = btn_from_obj(obj);
    if(radius < 0.0f) radius = 0.0f;
    btn->corner_radius = radius;
    btn->material.corner_radius = radius;
    sync_mesh_material(btn);
    lv_obj_invalidate(obj);
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
    btn->side_hover = lv_color_darken(released, 18);
    btn->top_released = shade_color(released, 56);
    btn->top_pressed = shade_color(pressed, 40);
    btn->top_hover = shade_color(released, 72);
    refresh_visual(btn);
}

void lv_3dbutton_set_hover_lift(lv_obj_t * obj, float lift_z, float hover_scale)
{
    lv_3dbutton_t * btn = btn_from_obj(obj);
    btn->hover_lift_z = lift_z;
    if(hover_scale < 1.0f) hover_scale = 1.0f;
    if(hover_scale > 1.12f) hover_scale = 1.12f;
    btn->hover_scale_mul = hover_scale;
    refresh_visual(btn);
}

void lv_3dbutton_set_press_depth(lv_obj_t * obj, float sink_z, float press_scale)
{
    lv_3dbutton_t * btn = btn_from_obj(obj);
    btn->press_sink_z = sink_z;
    if(press_scale < 0.5f) press_scale = 0.5f;
    if(press_scale > 1.0f) press_scale = 1.0f;
    btn->press_scale_mul = press_scale;
    refresh_visual(btn);
}

void lv_3dbutton_set_press_scale(lv_obj_t * obj, float scale_mul)
{
    lv_3dbutton_set_press_depth(obj, btn_from_obj(obj)->press_sink_z, scale_mul);
}

void lv_3dbutton_place(lv_obj_t * obj, float x, float y, float z)
{
    lv_3dbutton_t * btn = btn_from_obj(obj);
    btn->base_pos[0] = x;
    btn->base_pos[1] = y;
    btn->base_pos[2] = z;
    refresh_visual(btn);
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
    btn->side_hover = lv_color_darken(btn->color_released, 18);
    btn->top_released = shade_color(btn->color_released, 56);
    btn->top_pressed = shade_color(btn->color_pressed, 40);
    btn->top_hover = shade_color(btn->color_released, 72);
    btn->corner_radius = 12.0f;
    btn->hover_lift_z = 10.0f;
    btn->press_sink_z = -14.0f;
    btn->hover_scale_mul = 1.03f;
    btn->press_scale_mul = 0.97f;
    btn->vis = LV_3DBUTTON_VIS_NORMAL;
    btn->hovered = false;
    btn->is_pressed = false;

    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);

    lv_3dmesh_set_box(obj, 200.0f, 16.0f, 56.0f);
    lv_3dmesh_set_rotation(obj, 0.0f, 0.0f, 0.0f);
    lv_3dmesh_set_wireframe(obj, false);

    btn->material.kind = LV_3D_MAT_SHADED_BOX;
    btn->material.corner_radius = btn->corner_radius;
    btn->material.color = btn->side_released;
    btn->material.top_color = btn->top_released;
    btn->material.opa = LV_OPA_COVER;
    btn->base_pos[0] = btn->pos[0];
    btn->base_pos[1] = btn->pos[1];
    btn->base_pos[2] = btn->pos[2];
    sync_mesh_material(btn);
}

static void lv_3dbutton_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_current_target(e);
    lv_3dbutton_t * btn = btn_from_obj(obj);

    if(code == LV_EVENT_HOVER_OVER) {
        btn->hovered = true;
        if(!btn->is_pressed) refresh_visual(btn);
    }
    else if(code == LV_EVENT_HOVER_LEAVE) {
        btn->hovered = false;
        if(!btn->is_pressed) refresh_visual(btn);
    }
    else if(code == LV_EVENT_PRESSED) {
        btn->is_pressed = true;
        refresh_visual(btn);
    }
    else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        btn->is_pressed = false;
        refresh_visual(btn);
    }

    lv_obj_event_base(MY_CLASS, e);
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON*/
