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
#include "../../include/lvgl/draw/lv_draw_3d_line.h"
#include "../../include/lvgl/draw/lv_draw_3d_callback.h"

/*********************
 *      DEFINES
 *********************/

#define MY_CLASS (&lv_3dviewport_class)
#define GRID_HALF   2.5f
#define GRID_STEP   0.5f

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void lv_3dviewport_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_3dviewport_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_3dviewport_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void draw_3dviewport(lv_event_t * e);
static void submit_grid_lines(lv_layer_t * pass_layer);
static void on_pointer_event(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_3dpoint_t grid_points[88];
static uint32_t grid_point_cnt;

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
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
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

void lv_3dviewport_set_grid_visible(lv_obj_t * obj, bool visible)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);

    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->show_grid = visible;
    lv_obj_invalidate(obj);
}

void lv_3dviewport_set_render_cb(lv_obj_t * obj, lv_draw_3d_cb_t cb, void * user_data)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);

    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    vp->render_cb = cb;
    vp->render_user_data = user_data;
    lv_obj_invalidate(obj);
}

lv_3d_camera_t * lv_3dviewport_get_camera(lv_obj_t * obj)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return NULL);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    return &vp->camera;
}

void lv_3dviewport_set_orbit(lv_obj_t * obj, float yaw, float pitch, float distance)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    lv_3d_camera_set_orbit(&vp->camera, yaw, pitch, distance);
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
    vp->clear_color = lv_color32_make(0x12, 0x12, 0x12, LV_OPA_COVER);
    vp->clear_depth = true;
    vp->show_grid = true;
    vp->render_cb = NULL;
    vp->render_user_data = NULL;
    vp->dragging = false;
    lv_3d_camera_init(&vp->camera);

    if(grid_point_cnt == 0) {
        uint32_t idx = 0;
        for(float x = -GRID_HALF; x <= GRID_HALF + 0.001f; x += GRID_STEP) {
            grid_points[idx++] = (lv_3dpoint_t) { x, 0.f, -GRID_HALF };
            grid_points[idx++] = (lv_3dpoint_t) { x, 0.f, GRID_HALF };
        }
        for(float z = -GRID_HALF; z <= GRID_HALF + 0.001f; z += GRID_STEP) {
            grid_points[idx++] = (lv_3dpoint_t) { -GRID_HALF, 0.f, z };
            grid_points[idx++] = (lv_3dpoint_t) { GRID_HALF, 0.f, z };
        }
        grid_point_cnt = idx;
    }
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

    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_DRAW_MAIN) {
        draw_3dviewport(e);
    }
    else if(code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) {
        on_pointer_event(e);
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
    lv_draw_3d_pass_set_camera(vp->pass_layer, &vp->camera);

    lv_draw_3d_clear_dsc_t clr_dsc;
    lv_draw_3d_clear_dsc_init(&clr_dsc);
    clr_dsc.color = vp->clear_color;
    clr_dsc.opa = LV_OPA_COVER;
    clr_dsc.clear_depth = vp->clear_depth;
    lv_draw_3d_clear(vp->pass_layer, &clr_dsc);

    if(vp->show_grid) {
        submit_grid_lines(vp->pass_layer);
    }

    if(vp->render_cb) {
        lv_draw_3d_callback_dsc_t cb_dsc;
        lv_draw_3d_callback_dsc_init(&cb_dsc);
        cb_dsc.cb = vp->render_cb;
        cb_dsc.user_data = vp->render_user_data;
        lv_draw_3d_callback(vp->pass_layer, &cb_dsc);
    }

    lv_draw_3d_viewport_dsc_t vp_dsc;
    lv_draw_3d_viewport_dsc_init(&vp_dsc);
    vp_dsc.pass_layer = vp->pass_layer;
    vp_dsc.opa = lv_obj_get_style_opa(obj, LV_PART_MAIN);
    lv_draw_3d_viewport(layer, &vp_dsc, &coords);

    lv_draw_3d_viewport_end(vp->pass_layer);
}

static void submit_grid_lines(lv_layer_t * pass_layer)
{
    lv_draw_3d_line_dsc_t line_dsc;
    lv_draw_3d_line_dsc_init(&line_dsc);
    line_dsc.points = grid_points;
    line_dsc.point_cnt = grid_point_cnt;
    line_dsc.color = lv_color32_make(0x80, 0x80, 0x80, LV_OPA_COVER);
    line_dsc.width = 1.f;
    line_dsc.depth_test = true;
    lv_draw_3d_line(pass_layer, &line_dsc);
}

static void on_pointer_event(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_current_target(e);
    lv_3dviewport_t * vp = (lv_3dviewport_t *)obj;
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t * indev = lv_event_get_indev(e);
    if(indev == NULL) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if(code == LV_EVENT_PRESSED) {
        vp->dragging = true;
        vp->last_drag = p;
    }
    else if(code == LV_EVENT_PRESSING && vp->dragging) {
        int32_t dx = p.x - vp->last_drag.x;
        int32_t dy = p.y - vp->last_drag.y;
        vp->last_drag = p;
        vp->camera.yaw += dx * 0.01f;
        vp->camera.pitch += dy * 0.01f;
        if(vp->camera.pitch > 1.4f) vp->camera.pitch = 1.4f;
        if(vp->camera.pitch < -1.4f) vp->camera.pitch = -1.4f;
        lv_obj_invalidate(obj);
    }
    else if(code == LV_EVENT_RELEASED) {
        vp->dragging = false;
    }
}

#endif /*LV_USE_3DVIEWPORT*/
