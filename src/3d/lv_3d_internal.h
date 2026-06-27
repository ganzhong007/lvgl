/**
 * @file lv_3d_internal.h
 */

#ifndef LV_3D_INTERNAL_H
#define LV_3D_INTERNAL_H

#include "../lvgl_public.h"
#include "../include/lvgl/3d/lv_3d.h"

#if LV_USE_3D

#define LV_3D_MAX_DRAW_ITEMS 256

typedef struct {
    float local[16];
    float world[16];
    bool dirty;
} lv_3d_transform_t;

typedef struct {
    lv_3d_mesh_id_t id;
    float w, h, d;
    bool wireframe;
    lv_3d_material_t material;
    lv_3d_transform_t transform;
    lv_obj_t * obj;
} lv_3d_draw_item_t;

void lv_3d_mat4_identity(float m[16]);
void lv_3d_mat4_mul(float out[16], const float a[16], const float b[16]);
void lv_3d_mat4_translate(float m[16], float x, float y, float z);
void lv_3d_mat4_scale(float m[16], float sx, float sy, float sz);
void lv_3d_mat4_rotate_y(float m[16], float rad);
void lv_3d_mat4_perspective(float m[16], float fov_deg, float aspect, float near_z, float far_z);
void lv_3d_mat4_look_at(float m[16], lv_vec3_t eye, lv_vec3_t target, lv_vec3_t up);

void lv_3d_transform_init(lv_3d_transform_t * t);
void lv_3d_transform_set_local_trs(lv_3d_transform_t * t, float tx, float ty, float tz,
                                 float rx, float ry, float rz, float sx, float sy, float sz);
void lv_3d_transform_update_world(lv_3d_transform_t * t, const float parent_world[16]);

lv_3d_mesh_id_t lv_3d_mesh_alloc_box(float w, float h, float d, bool wireframe);
const lv_3d_draw_item_t * lv_3d_mesh_get_draw_item(lv_3d_mesh_id_t id);
lv_3d_draw_item_t * lv_3d_mesh_get_draw_item_mut(lv_3d_mesh_id_t id);

void lv_3d_camera_get_view_proj(lv_obj_t * cam, int32_t vp_w, int32_t vp_h, float view[16], float proj[16]);

uint32_t lv_3d_scene_collect(lv_obj_t * scene, lv_3d_draw_item_t * out, uint32_t max_out);

#endif /*LV_USE_3D*/

#endif /*LV_3D_INTERNAL_H*/
