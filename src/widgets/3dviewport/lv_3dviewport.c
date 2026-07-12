/**
 * @file lv_3dviewport.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_3dviewport_private.h"
#include "../../lvgl_public.h"

#if LV_USE_3DVIEWPORT

#include "../../core/lv_obj_class_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_viewport.h"
#include "../../include/lvgl/draw/lv_draw_3d_clear.h"

/*********************
 *      DEFINES
 *********************/

#define MY_CLASS (&lv_3dviewport_class)

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void lv_3dviewport_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_3dviewport_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_3dviewport_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void draw_3dviewport(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t lv_3dviewport_class = {
    .constructor_cb = lv_3dviewport_constructor,
    .destructor_cb = lv_3dviewport_destructor,
    .event_cb = lv_3dviewport_event,
    .width_def = LV_DPI_DEF * 2,
    .height_def = LV_DPI_DEF * 3 / 2,
    .instance_size = sizeof(lv_3dviewport_t),
    .base_class = &lv_obj_class,
    .name = "3dviewport",
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_3dviewport_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_3dviewport_set_clear_color(lv_obj_t * obj, lv_color_t color, lv_opa_t opa)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);

    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->clear_color = lv_color32_make(color.red, color.green, color.blue, opa);
    lv_obj_invalidate(obj);
}

void lv_3dviewport_set_clear_depth(lv_obj_t * obj, bool clear_depth)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);

    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->clear_depth = clear_depth;
    lv_obj_invalidate(obj);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_3dviewport_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->pass_layer = NULL;
    vp->clear_color = lv_color32_make(0x00, 0x89, 0x7B, LV_OPA_COVER);
    vp->clear_depth = true;
}

static void lv_3dviewport_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    if(vp->pass_layer != NULL) {
        lv_display_t * disp = lv_obj_get_display(obj);
        lv_draw_3d_pass_layer_destroy(vp->pass_layer, disp);
        vp->pass_layer = NULL;
    }
}

static void lv_3dviewport_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    lv_result_t res = lv_obj_event_base(class_p, e);
    if(res != LV_RESULT_OK) return;

    if(lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        draw_3dviewport(e);
    }
}

static void draw_3dviewport(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_current_target(e);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    lv_layer_t * layer = lv_event_get_layer(e);

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    if(vp->pass_layer == NULL) {
        vp->pass_layer = lv_draw_3d_pass_layer_create(layer, &coords);
        if(vp->pass_layer == NULL) return;
    }
    else {
        vp->pass_layer->buf_area = coords;
        vp->pass_layer->_clip_area = coords;
        vp->pass_layer->phy_clip_area = coords;
    }

    vp->pass_layer->all_tasks_added = false;

    lv_draw_3d_clear_dsc_t clr_dsc;
    lv_draw_3d_clear_dsc_init(&clr_dsc);
    clr_dsc.color = vp->clear_color;
    clr_dsc.opa = LV_OPA_COVER;
    clr_dsc.clear_depth = vp->clear_depth;
    lv_draw_3d_clear(vp->pass_layer, &clr_dsc);

    lv_draw_3d_viewport_dsc_t vp_dsc;
    lv_draw_3d_viewport_dsc_init(&vp_dsc);
    vp_dsc.pass_layer = vp->pass_layer;
    vp_dsc.opa = lv_obj_get_style_opa(obj, LV_PART_MAIN);
    lv_draw_3d_viewport(layer, &vp_dsc, &coords);

    lv_draw_3d_viewport_end(vp->pass_layer);
}

#endif /*LV_USE_3DVIEWPORT*/
