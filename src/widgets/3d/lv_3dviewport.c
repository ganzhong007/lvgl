/**
 * @file lv_3dviewport.c
 */

#include "lv_3dviewport_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../../core/lv_obj_class_private.h"
#include "../../core/lv_obj_event_private.h"
#include "../../indev/lv_indev.h"
#include "../../include/lvgl/draw/lv_draw_3d.h"
#include "../../draw/gpu_renderer/lv_draw_gpu_renderer.h"
#include "../../3d/lv_3d_internal.h"

#define MY_CLASS (&lv_3dviewport_class)

static void lv_3dviewport_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void lv_3dviewport_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void route_input_event(lv_obj_t * vp, lv_event_t * e);

const lv_obj_class_t lv_3dviewport_class = {
    .constructor_cb = lv_3dviewport_constructor,
    .event_cb = lv_3dviewport_event,
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .instance_size = sizeof(lv_3dviewport_t),
    .base_class = &lv_obj_class,
    .name = "3dviewport",
};

lv_obj_t * lv_3dviewport_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_3dviewport_set_camera(lv_obj_t * obj, lv_obj_t * camera)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->camera = camera;
    lv_obj_invalidate(obj);
}

void lv_3dviewport_set_scene(lv_obj_t * obj, lv_obj_t * scene)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->scene = scene;
    lv_obj_invalidate(obj);
}

void lv_3dviewport_set_pickable(lv_obj_t * obj, bool en)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->pickable = en;
}

void lv_3dviewport_set_input_routing(lv_obj_t * obj, bool en)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->input_route = en;
    if(en) lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t * lv_3dviewport_pick_obj(lv_obj_t * obj, lv_point3d_t ray_origin, lv_vec3_t ray_dir)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    if(!vp->pickable || !vp->scene) return NULL;
    return lv_3d_pick_scene(vp->scene, ray_origin, ray_dir);
}

lv_obj_t * lv_3dviewport_pick_at(lv_obj_t * obj, int32_t x, int32_t y)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    if(!vp->pickable || !vp->scene || !vp->camera) return NULL;

    int32_t w = lv_obj_get_width(obj);
    int32_t h = lv_obj_get_height(obj);
    if(w < 1 || h < 1) return NULL;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    x -= coords.x1;
    y -= coords.y1;
    if(x < 0 || y < 0 || x >= w || y >= h) return NULL;

    /* Match GLES viewport placement (vy = dh - y2 - 1) used during 3D flush. */
    y = h - 1 - y;

    float view[16], proj[16];
    lv_3d_camera_get_view_proj(vp->camera, w, h, view, proj);

    lv_vec3_t origin, dir;
    lv_3d_ray_from_screen(x, y, w, h, view, proj, &origin, &dir);
    return lv_3d_pick_scene(vp->scene, origin, dir);
}

static void lv_3dviewport_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->pickable = true;
    vp->input_route = false;
    vp->pressed_obj = NULL;
    vp->hovered_obj = NULL;
}

static void route_hover_event(lv_obj_t * obj, lv_event_t * e)
{
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    if(!vp->input_route || !vp->pickable) return;

    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_HOVER_OVER && code != LV_EVENT_HOVER_LEAVE) return;

    lv_indev_t * indev = lv_event_get_indev(e);
    if(!indev) indev = lv_indev_active();
    if(!indev) return;

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    lv_obj_t * hit = (code == LV_EVENT_HOVER_OVER) ? lv_3dviewport_pick_at(obj, pt.x, pt.y) : NULL;

    if(vp->hovered_obj && vp->hovered_obj != hit) {
        if(lv_obj_has_flag(vp->hovered_obj, LV_OBJ_FLAG_CLICKABLE)) {
            lv_obj_send_event(vp->hovered_obj, LV_EVENT_HOVER_LEAVE, indev);
        }
        vp->hovered_obj = NULL;
    }

    if(hit && hit != vp->hovered_obj && lv_obj_has_flag(hit, LV_OBJ_FLAG_CLICKABLE)) {
        vp->hovered_obj = hit;
        lv_obj_send_event(hit, LV_EVENT_HOVER_OVER, indev);
    }
}

static void route_input_event(lv_obj_t * obj, lv_event_t * e)
{
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    if(!vp->input_route || !vp->pickable) return;

    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED &&
       code != LV_EVENT_PRESS_LOST && code != LV_EVENT_CLICKED) {
        return;
    }

    lv_indev_t * indev = lv_event_get_indev(e);
    if(!indev) indev = lv_indev_active();
    if(!indev) return;

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    lv_obj_t * hit = lv_3dviewport_pick_at(obj, pt.x, pt.y);

    if(code == LV_EVENT_PRESSED) {
        vp->pressed_obj = hit;
        if(hit && lv_obj_has_flag(hit, LV_OBJ_FLAG_CLICKABLE)) {
            lv_obj_send_event(hit, LV_EVENT_PRESSED, indev);
        }
    }
    else if(code == LV_EVENT_PRESSING) {
        if(vp->pressed_obj && lv_obj_has_flag(vp->pressed_obj, LV_OBJ_FLAG_CLICKABLE)) {
            lv_obj_send_event(vp->pressed_obj, LV_EVENT_PRESSING, indev);
        }
    }
    else if(code == LV_EVENT_RELEASED) {
        lv_obj_t * was = vp->pressed_obj;
        vp->pressed_obj = NULL;
        if(was && lv_obj_has_flag(was, LV_OBJ_FLAG_CLICKABLE)) {
            lv_obj_send_event(was, LV_EVENT_RELEASED, indev);
            if(hit == was) {
                lv_obj_send_event(was, LV_EVENT_CLICKED, indev);
            }
        }
    }
    else if(code == LV_EVENT_PRESS_LOST) {
        lv_obj_t * was = vp->pressed_obj;
        vp->pressed_obj = NULL;
        if(was && lv_obj_has_flag(was, LV_OBJ_FLAG_CLICKABLE)) {
            lv_obj_send_event(was, LV_EVENT_PRESS_LOST, indev);
        }
    }
    else if(code == LV_EVENT_CLICKED) {
        /* Routed on RELEASED when still over target; swallow duplicate vp CLICKED. */
        lv_event_stop_processing(e);
    }
}

static void lv_3dviewport_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_current_target(e);

    if(code == LV_EVENT_DRAW_MAIN) {
        lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
        if(!vp->scene || !vp->camera) return;

        lv_layer_t * layer = lv_event_get_layer(e);
        lv_area_t coords;
        lv_obj_get_coords(obj, &coords);

        lv_draw_3d_dsc_t dsc;
        lv_draw_3d_dsc_init(&dsc);
        dsc.kind = LV_3D_DRAW_KIND_VIEWPORT_PASS;
        dsc.scene = vp->scene;
        dsc.camera = vp->camera;
        dsc.viewport_area = coords;
        lv_draw_3d(layer, &dsc, &coords);
    }
    else if(code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED ||
            code == LV_EVENT_PRESS_LOST || code == LV_EVENT_CLICKED) {
        route_input_event(obj, e);
        lv_obj_event_base(MY_CLASS, e);
    }
    else if(code == LV_EVENT_HOVER_OVER || code == LV_EVENT_HOVER_LEAVE) {
        route_hover_event(obj, e);
        lv_obj_event_base(MY_CLASS, e);
    }
    else {
        lv_obj_event_base(MY_CLASS, e);
    }
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/
