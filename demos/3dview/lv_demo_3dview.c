/**
 * @file lv_demo_3dview.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_demo_3dview.h"

#if LV_USE_DEMO_3DVIEW && LV_USE_3DVIEWPORT && LV_USE_3DMESH && LV_USE_3DLIGHT

#include "../../include/lvgl/misc/lv_3dcaps.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void cube_anim_cb(lv_timer_t * timer);
static void mesh_click_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_obj_t * s_cube;
static lv_obj_t * s_status;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_demo_3dview(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), LV_PART_MAIN);

    lv_obj_t * panel = lv_obj_create(scr);
    lv_obj_set_size(panel, 120, 60);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, 16, 16);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x3949AB), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN);

    lv_obj_t * panel_label = lv_label_create(panel);
    lv_label_set_text(panel_label, "2D FILL");
    lv_obj_center(panel_label);

    lv_obj_t * vp = lv_3dviewport_create(scr);
    lv_obj_set_size(vp, 420, 280);
    lv_obj_center(vp);
    lv_3dviewport_set_clear_color(vp, lv_color_hex(0x121212), LV_OPA_COVER);
    lv_3dviewport_set_grid_visible(vp, true);

    lv_obj_t * sun = lv_3dlight_create(vp);
    lv_3dlight_set_directional(sun, 0.4f, -1.f, 0.3f, lv_color_hex(0xFFF8E1), LV_OPA_COVER, 1.1f);

    lv_obj_t * fill = lv_3dlight_create(vp);
    lv_3dlight_set_point(fill, 1.5f, 0.8f, 1.2f, lv_color_hex(0x64B5F6), LV_OPA_COVER, 0.8f, 6.f);

    lv_obj_t * floor = lv_3dmesh_create(vp);
    lv_3dmesh_set_phong(floor, true);
    lv_3dmesh_set_box(floor, 5.f, 0.05f, 5.f);
    lv_3dmesh_set_color(floor, lv_color_hex(0x606060), LV_OPA_COVER);
    lv_3dmesh_set_shininess(floor, 8.f);
    lv_3dmesh_set_transform(floor, 0.f, -0.025f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f);
    lv_obj_add_event_cb(floor, mesh_click_cb, LV_EVENT_CLICKED, NULL);

    if(lv_3dcaps_has(LV_3D_CAP_OBJ_LOADER)) {
        lv_obj_t * pyramid = lv_3dmesh_create(vp);
        lv_obj_add_flag(pyramid, LV_OBJ_FLAG_USER_1);
        if(lv_3dmesh_load_obj(pyramid, "A:lvgl/demos/3dview/assets/pyramid.obj") == LV_RESULT_OK) {
            lv_3dmesh_set_color(pyramid, lv_color_hex(0xAB47BC), LV_OPA_COVER);
            lv_3dmesh_set_shininess(pyramid, 24.f);
            lv_3dmesh_set_transform(pyramid, -1.2f, 0.2f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f);
            lv_obj_add_event_cb(pyramid, mesh_click_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    s_cube = lv_3dmesh_create(vp);
    lv_3dmesh_set_phong(s_cube, true);
    lv_3dmesh_set_box(s_cube, 0.7f, 0.7f, 0.7f);
    lv_3dmesh_set_color(s_cube, lv_color_hex(0xF27321), LV_OPA_COVER);
    lv_3dmesh_set_shininess(s_cube, 48.f);
    lv_3dmesh_set_cull_face(s_cube, false);
    lv_3dmesh_set_transform(s_cube, 0.f, 0.35f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f);
    lv_obj_add_event_cb(s_cube, mesh_click_cb, LV_EVENT_CLICKED, NULL);

    lv_timer_create(cube_anim_cb, 33, NULL);

    lv_obj_t * label = lv_label_create(scr);
    lv_label_set_text(label, "G8.4/5 phong+pick+OBJ (D3-13/14); click mesh / orbit drag");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 12);

    s_status = lv_label_create(scr);
    lv_label_set_text(s_status, "pick: (none)");
    lv_obj_set_style_text_color(s_status, lv_color_hex(0xB0BEC5), LV_PART_MAIN);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_MID, 0, -12);

    return vp;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void cube_anim_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(s_cube == NULL) return;

    static float rot = 0.f;
    rot += 0.02f;
    lv_3dmesh_set_transform(s_cube, 0.f, 0.35f, 0.f, 0.f, rot, 0.f, 1.f, 1.f, 1.f);
}

static void mesh_click_cb(lv_event_t * e)
{
    lv_obj_t * mesh = lv_event_get_current_target(e);
    if(s_status == NULL) return;

    const char * name = "mesh";
    if(mesh == s_cube) name = "cube";
    else if(lv_obj_has_flag(mesh, LV_OBJ_FLAG_USER_1)) name = "pyramid(OBJ)";
    else name = "floor";

    lv_3dmesh_set_color(mesh, lv_color_hex(0xFFEB3B), LV_OPA_COVER);

    static char buf[64];
    lv_snprintf(buf, sizeof(buf), "pick: %s", name);
    lv_label_set_text(s_status, buf);
}

#endif /*LV_USE_DEMO_3DVIEW*/
