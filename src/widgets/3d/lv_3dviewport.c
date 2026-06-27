/**
 * @file lv_3dviewport.c
 */

#include "lv_3dviewport_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../../core/lv_obj_class_private.h"
#include "../../include/lvgl/draw/lv_draw_3d.h"
#include "../../draw/gpu_composite/lv_draw_gpu_composite.h"

#define MY_CLASS (&lv_3dviewport_class)

static void lv_3dviewport_event(const lv_obj_class_t * class_p, lv_event_t * e);

const lv_obj_class_t lv_3dviewport_class = {
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
    else {
        lv_obj_event_base(MY_CLASS, e);
    }
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/
