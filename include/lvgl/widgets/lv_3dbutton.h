/**
 * @file lv_3dbutton.h — 3D mesh button (pick + hover lift + press depth)
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

/** Box size: X width, Y thickness (top cap normal +Y), Z depth. */
void lv_3dbutton_set_box_size(lv_obj_t * btn, float w, float h, float depth);

/** Top-cap corner radius (Quick3D-style rounded extrusion). */
void lv_3dbutton_set_corner_radius(lv_obj_t * btn, float radius);

/** Pitch/yaw in degrees — tilts the button so side faces are visible. */
void lv_3dbutton_set_tilt(lv_obj_t * btn, float pitch_deg, float yaw_deg);

void lv_3dbutton_set_colors(lv_obj_t * btn, lv_color_t released, lv_color_t pressed);

/** +Y lift on hover + optional uniform scale (default 10 / 1.03). */
void lv_3dbutton_set_hover_lift(lv_obj_t * btn, float lift_y, float hover_scale);

/** Extra -Y sink while pressed (negative) + scale (default -14 / 0.97). */
void lv_3dbutton_set_press_depth(lv_obj_t * btn, float sink_y, float press_scale);

/** Uniform scale multiplier while pressed (legacy API). */
void lv_3dbutton_set_press_scale(lv_obj_t * btn, float scale_mul);

/** Rest position in scene space; hover/press offsets apply on top. */
void lv_3dbutton_place(lv_obj_t * btn, float x, float y, float z);

bool lv_3dbutton_is_pressed(const lv_obj_t * btn);

/** Returns true if (x,y) in display coords hits this button via viewport pick. */
bool lv_3dbutton_hit_test(lv_obj_t * vp, lv_obj_t * btn, int32_t x, int32_t y);

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON*/

#ifdef __cplusplus
}
#endif

#endif /*LV_3DBUTTON_H */
