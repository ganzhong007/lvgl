/**
 * @file lv_3d_camera.c
 */

#include "lv_3d_internal.h"
#include "../widgets/3d/lv_3dcamera_private.h"

#if LV_USE_3D

void lv_3d_camera_get_view_proj(lv_obj_t * cam, int32_t vp_w, int32_t vp_h, float view[16], float proj[16])
{
    lv_3dcamera_t * c = (lv_3dcamera_t *)cam;
    float aspect = vp_h > 0 ? (float)vp_w / (float)vp_h : 1.0f;
    lv_vec3_t eye = { c->eye.x, c->eye.y, c->eye.z };
    lv_vec3_t target = { c->target.x, c->target.y, c->target.z };
    lv_vec3_t up = { c->up.x, c->up.y, c->up.z };
    lv_3d_mat4_look_at(view, eye, target, up);
    lv_3d_mat4_perspective(proj, c->fov_deg, aspect, c->near_z, c->far_z);
}

#endif /*LV_USE_3D*/
