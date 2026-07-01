/**
 * @file lv_3dbutton_private.h
 */

#ifndef LV_3DBUTTON_PRIVATE_H
#define LV_3DBUTTON_PRIVATE_H

#include "../../lvgl_public.h"
#include "../../core/lv_obj_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS && LV_USE_3DBUTTON

#include "lv_3dmesh_private.h"

typedef enum {
    LV_3DBUTTON_VIS_NORMAL = 0,
    LV_3DBUTTON_VIS_HOVER,
    LV_3DBUTTON_VIS_PRESSED,
} lv_3dbutton_vis_t;

typedef struct {
    lv_obj_t obj;
    lv_3d_mesh_id_t mesh_id;
    lv_3d_material_t material;
    lv_3d_snapshot_id_t snapshot_id;
    float pos[3];
    float rot[3];
    float scale[3];
    float base_pos[3];
    lv_color_t color_released;
    lv_color_t color_pressed;
    lv_color_t side_released;
    lv_color_t side_pressed;
    lv_color_t side_hover;
    lv_color_t top_released;
    lv_color_t top_pressed;
    lv_color_t top_hover;
    float corner_radius;
    float hover_lift_z;
    float press_sink_z;
    float hover_scale_mul;
    float press_scale_mul;
    lv_3dbutton_vis_t vis;
    bool hovered;
    bool is_pressed;
} lv_3dbutton_t;

extern const lv_obj_class_t lv_3dbutton_class;

#endif

#endif /*LV_3DBUTTON_PRIVATE_H */
