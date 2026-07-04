/**
 * @file lv_3dmesh.c
 */

#include "lv_3dmesh_private.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../../core/lv_obj_class_private.h"
#include "../../3d/lv_3d_internal.h"

#define MY_CLASS (&lv_3dmesh_class)

static void lv_3dmesh_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_3dmesh_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);

static lv_3dmesh_t * mesh_from_obj(lv_obj_t * obj)
{
    if(!lv_obj_has_class(obj, MY_CLASS)) return NULL;
    return (lv_3dmesh_t *)obj;
}

const lv_obj_class_t lv_3dmesh_class = {
    .constructor_cb = lv_3dmesh_constructor,
    .destructor_cb = lv_3dmesh_destructor,
    .width_def = LV_DPI_DEF / 2,
    .height_def = LV_DPI_DEF / 2,
    .instance_size = sizeof(lv_3dmesh_t),
    .base_class = &lv_obj_class,
    .name = "3dmesh",
};

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

static const float rad_to_deg = (float)(180.0 / M_PI);
static const float deg_to_rad = (float)(M_PI / 180.0);

static void sync_draw_material(lv_3dmesh_t * mesh)
{
    lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(mesh->mesh_id);
    if(item) item->material = mesh->material;
}

static void sync_transform(lv_3dmesh_t * mesh)
{
    lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(mesh->mesh_id);
    if(!item) return;
    lv_3d_transform_set_local_trs(&item->transform,
                                  mesh->pos[0], mesh->pos[1], mesh->pos[2],
                                  mesh->rot[0], mesh->rot[1], mesh->rot[2],
                                  mesh->scale[0], mesh->scale[1], mesh->scale[2]);
}

lv_obj_t * lv_3dmesh_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_3dmesh_set_box(lv_obj_t * obj, float w, float h, float d)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->mesh_id = lv_3d_mesh_alloc_box(w, h, d, false);
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_uv_sphere(lv_obj_t * obj, float diameter)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->mesh_id = lv_3d_mesh_alloc_uv_sphere(diameter, false);
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_wireframe(lv_obj_t * obj, bool en)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    if(mesh->mesh_id) {
        lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(mesh->mesh_id);
        if(item) item->wireframe = en;
    }
    lv_3d_material_init(&mesh->material, en ? LV_3D_MAT_WIREFRAME : LV_3D_MAT_OPAQUE, mesh->material.color,
                        mesh->material.opa);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_color(lv_obj_t * obj, lv_color_t c)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->material.color = c;
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_material(lv_obj_t * obj, const lv_3d_material_t * mat)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->material = *mat;
    sync_draw_material(mesh);
    if(mesh->mesh_id) {
        lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(mesh->mesh_id);
        if(item) item->wireframe = (mat->kind == LV_3D_MAT_WIREFRAME);
    }
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_plane_snapshot(lv_obj_t * obj, lv_3d_snapshot_id_t snapshot_id)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->snapshot_id = snapshot_id;
    mesh->material.kind = LV_3D_MAT_PLANE_SNAPSHOT;
    mesh->material.opa = LV_OPA_COVER;
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_position(lv_obj_t * obj, float x, float y, float z)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->pos[0] = x; mesh->pos[1] = y; mesh->pos[2] = z;
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_rotation_y(lv_obj_t * obj, float yaw_deg)
{
    lv_3dmesh_set_rotation(obj, 0.0f, yaw_deg, 0.0f);
}

void lv_3dmesh_set_rotation(lv_obj_t * obj, float pitch_deg, float yaw_deg, float roll_deg)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->rot[0] = pitch_deg * deg_to_rad;
    mesh->rot[1] = yaw_deg * deg_to_rad;
    mesh->rot[2] = roll_deg * deg_to_rad;
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_scale(lv_obj_t * obj, float sx, float sy, float sz)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->scale[0] = sx; mesh->scale[1] = sy; mesh->scale[2] = sz;
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_opa(lv_obj_t * obj, lv_opa_t opa)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    mesh->material.opa = opa;
    sync_draw_material(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_get_position(lv_obj_t * obj, float * x, float * y, float * z)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    if(x) *x = mesh->pos[0];
    if(y) *y = mesh->pos[1];
    if(z) *z = mesh->pos[2];
}

void lv_3dmesh_get_rotation(lv_obj_t * obj, float * pitch_deg, float * yaw_deg, float * roll_deg)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    if(pitch_deg) *pitch_deg = mesh->rot[0] * rad_to_deg;
    if(yaw_deg) *yaw_deg = mesh->rot[1] * rad_to_deg;
    if(roll_deg) *roll_deg = mesh->rot[2] * rad_to_deg;
}

void lv_3dmesh_get_scale(lv_obj_t * obj, float * sx, float * sy, float * sz)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return;
    if(sx) *sx = mesh->scale[0];
    if(sy) *sy = mesh->scale[1];
    if(sz) *sz = mesh->scale[2];
}

lv_opa_t lv_3dmesh_get_opa(lv_obj_t * obj)
{
    lv_3dmesh_t * mesh = mesh_from_obj(obj);
    if(!mesh) return LV_OPA_TRANSP;
    return mesh->material.opa;
}

static void lv_3dmesh_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->mesh_id = lv_3d_mesh_alloc_box(100, 100, 100, true);
    mesh->scale[0] = mesh->scale[1] = mesh->scale[2] = 1.0f;
    mesh->snapshot_id = LV_3D_SNAPSHOT_ID_NONE;
    lv_3d_material_init(&mesh->material, LV_3D_MAT_WIREFRAME, lv_color_hex(0x00FFAA), LV_OPA_COVER);
    sync_transform(mesh);
}

static void lv_3dmesh_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_UNUSED(obj);
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/
