/**
 * @file lv_3dbutton.h — 3D mesh button (pick + pressed visual feedback)
 */

#ifndef LV_3DBUTTON_H
#define LV_3DBUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON

#include "../core/lv_obj.h"
#include "../3d/lv_3d.h"

lv_obj_t * lv_3dbutton_create(lv_obj_t * parent);

/** Box size in 3D scene units (default 200 x 56 x 48, extruded along Z). */
void lv_3dbutton_set_box_size(lv_obj_t * btn, float w, float h, float depth);

/** Pitch/yaw in degrees — tilts the button so side faces are visible (default -18°, 24°). */
void lv_3dbutton_set_tilt(lv_obj_t * btn, float pitch_deg, float yaw_deg);

void lv_3dbutton_set_colors(lv_obj_t * btn, lv_color_t released, lv_color_t pressed);

/** Uniform scale multiplier while pressed (default 0.96). */
void lv_3dbutton_set_press_scale(lv_obj_t * btn, float scale_mul);

bool lv_3dbutton_is_pressed(const lv_obj_t * btn);

/** Returns true if (x,y) in display coords hits this button via viewport pick. */
bool lv_3dbutton_hit_test(lv_obj_t * vp, lv_obj_t * btn, int32_t x, int32_t y);

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON*/

#ifdef __cplusplus
}
#endif

#endif /*LV_3DBUTTON_H*/
