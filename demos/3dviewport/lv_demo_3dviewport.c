/**
 * @file lv_demo_3dviewport.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_demo_3dviewport.h"

#if LV_USE_DEMO_3DVIEWPORT

/*********************
 *      DEFINES
 *********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_demo_3dviewport(void)
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
    lv_3dviewport_set_clear_color(vp, lv_color_hex(0x00897B), LV_OPA_COVER);

    lv_obj_t * label = lv_label_create(scr);
    lv_label_set_text(label, "G8.0 3D Viewport + 2D Label (D3-06 / D3-17)");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 12);

    return vp;
}

#endif /*LV_USE_DEMO_3DVIEWPORT*/
