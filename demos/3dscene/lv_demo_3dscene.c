/**
 * @file lv_demo_3dscene.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_demo_3dscene.h"

#if LV_USE_DEMO_3DSCENE && LV_USE_3DVIEWPORT && LV_USE_3DMESH

/*********************
 *      DEFINES
 *********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void cube_anim_cb(lv_timer_t * timer);

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_obj_t * s_cube;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_demo_3dscene(void)
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

    lv_obj_t * floor = lv_3dmesh_create(vp);
    lv_3dmesh_set_box(floor, 5.f, 0.05f, 5.f);
    lv_3dmesh_set_color(floor, lv_color_hex(0x505050), LV_OPA_COVER);
    lv_3dmesh_set_transform(floor, 0.f, -0.025f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f);

    s_cube = lv_3dmesh_create(vp);
    lv_3dmesh_set_box(s_cube, 0.7f, 0.7f, 0.7f);
    lv_3dmesh_set_color(s_cube, lv_color_hex(0xF27321), LV_OPA_COVER);
    /* Disable cull so the bottom interior occludes the grid at oblique Y rotations. */
    lv_3dmesh_set_cull_face(s_cube, false);
    lv_3dmesh_set_transform(s_cube, 0.f, 0.35f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f);

    lv_timer_create(cube_anim_cb, 33, NULL);

    lv_obj_t * label = lv_label_create(scr);
    lv_label_set_text(label, "G8.2 mesh: floor + cube (D3-11/18); orbit drag");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 12);

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

#endif /*LV_USE_DEMO_3DSCENE*/
