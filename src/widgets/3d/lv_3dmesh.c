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

const lv_obj_class_t lv_3dmesh_class = {
    .constructor_cb = lv_3dmesh_constructor,
    .destructor_cb = lv_3dmesh_destructor,
    .width_def = LV_DPI_DEF / 2,
    .height_def = LV_DPI_DEF / 2,
    .instance_size = sizeof(lv_3dmesh_t),
    .base_class = &lv_obj_class,
    .name = "3dmesh",
};

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
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->mesh_id = lv_3d_mesh_alloc_box(w, h, d, false);
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_wireframe(lv_obj_t * obj, bool en)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
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
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->material.color = c;
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_material(lv_obj_t * obj, const lv_3d_material_t * mat)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->material = *mat;
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_position(lv_obj_t * obj, float x, float y, float z)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->pos[0] = x; mesh->pos[1] = y; mesh->pos[2] = z;
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_rotation_y(lv_obj_t * obj, float yaw_deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->rot[1] = yaw_deg * (float)(3.14159265 / 180.0);
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

void lv_3dmesh_set_scale(lv_obj_t * obj, float sx, float sy, float sz)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->scale[0] = sx; mesh->scale[1] = sy; mesh->scale[2] = sz;
    sync_transform(mesh);
    lv_obj_invalidate(obj);
}

static void lv_3dmesh_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->mesh_id = lv_3d_mesh_alloc_box(100, 100, 100, true);
    mesh->scale[0] = mesh->scale[1] = mesh->scale[2] = 1.0f;
    lv_3d_material_init(&mesh->material, LV_3D_MAT_WIREFRAME, lv_color_hex(0x00FFAA), LV_OPA_COVER);
    sync_transform(mesh);
}

static void lv_3dmesh_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_UNUSED(obj);
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/
