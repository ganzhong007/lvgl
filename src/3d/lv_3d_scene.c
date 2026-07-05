/**
 * @file lv_3d_scene.c
 */

#include "lv_3d_internal.h"
#include "../widgets/3d/lv_3dmesh_private.h"

#if LV_USE_3D

static bool g_scene_dirty[32];
static lv_obj_t * g_scene_dirty_obj[32];
static uint32_t g_scene_dirty_count;

static bool g_cam_dirty[16];
static lv_obj_t * g_cam_dirty_obj[16];
static uint32_t g_cam_dirty_count;

static void track_scene_dirty(lv_obj_t * scene)
{
    if(!scene) return;
    for(uint32_t i = 0; i < g_scene_dirty_count; i++) {
        if(g_scene_dirty_obj[i] == scene) {
            g_scene_dirty[i] = true;
            return;
        }
    }
    if(g_scene_dirty_count >= (sizeof(g_scene_dirty) / sizeof(g_scene_dirty[0]))) return;
    g_scene_dirty_obj[g_scene_dirty_count] = scene;
    g_scene_dirty[g_scene_dirty_count] = true;
    g_scene_dirty_count++;
}

static bool scene_tracked_dirty(lv_obj_t * scene)
{
    if(!scene) return true;
    for(uint32_t i = 0; i < g_scene_dirty_count; i++) {
        if(g_scene_dirty_obj[i] == scene) return g_scene_dirty[i];
    }
    return true;
}

void lv_3d_scene_mark_dirty(lv_obj_t * scene)
{
    track_scene_dirty(scene);
}

bool lv_3d_scene_is_dirty(lv_obj_t * scene)
{
    return scene_tracked_dirty(scene);
}

void lv_3d_scene_clear_dirty(lv_obj_t * scene)
{
    if(!scene) return;
    for(uint32_t i = 0; i < g_scene_dirty_count; i++) {
        if(g_scene_dirty_obj[i] == scene) {
            g_scene_dirty[i] = false;
            return;
        }
    }
}

void lv_3d_camera_mark_dirty(lv_obj_t * cam)
{
    if(!cam) return;
    for(uint32_t i = 0; i < g_cam_dirty_count; i++) {
        if(g_cam_dirty_obj[i] == cam) {
            g_cam_dirty[i] = true;
            return;
        }
    }
    if(g_cam_dirty_count >= (sizeof(g_cam_dirty) / sizeof(g_cam_dirty[0]))) return;
    g_cam_dirty_obj[g_cam_dirty_count] = cam;
    g_cam_dirty[g_cam_dirty_count] = true;
    g_cam_dirty_count++;
}

bool lv_3d_camera_is_dirty(lv_obj_t * cam)
{
    if(!cam) return true;
    for(uint32_t i = 0; i < g_cam_dirty_count; i++) {
        if(g_cam_dirty_obj[i] == cam) return g_cam_dirty[i];
    }
    return true;
}

void lv_3d_camera_clear_dirty(lv_obj_t * cam)
{
    if(!cam) return;
    for(uint32_t i = 0; i < g_cam_dirty_count; i++) {
        if(g_cam_dirty_obj[i] == cam) {
            g_cam_dirty[i] = false;
            return;
        }
    }
}

static bool collect_obj_has_local_dirty(lv_obj_t * obj)
{
    uint32_t i;
    for(i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t * child = lv_obj_get_child(obj, i);
        if(lv_obj_has_class(child, &lv_3dmesh_class)) {
            lv_3dmesh_t * mesh = (lv_3dmesh_t *)child;
            lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(mesh->mesh_id);
            if(item && item->transform.dirty) return true;
        }
        else if(collect_obj_has_local_dirty(child)) {
            return true;
        }
    }
    return false;
}

static void collect_obj(lv_obj_t * obj, const float parent_world[16], lv_3d_draw_item_t * out, uint32_t * count,
                          uint32_t max_out)
{
    uint32_t i;
    for(i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t * child = lv_obj_get_child(obj, i);
        if(lv_obj_has_class(child, &lv_3dmesh_class)) {
            if(*count >= max_out) return;
            lv_3dmesh_t * mesh = (lv_3dmesh_t *)child;
            lv_3d_draw_item_t * item = lv_3d_mesh_get_draw_item_mut(mesh->mesh_id);
            if(item) {
                lv_3d_transform_set_local_trs(&item->transform,
                                              mesh->pos[0], mesh->pos[1], mesh->pos[2],
                                              mesh->rot[0], mesh->rot[1], mesh->rot[2],
                                              mesh->scale[0], mesh->scale[1], mesh->scale[2]);
                lv_3d_transform_update_world(&item->transform, parent_world);
                item->material = mesh->material;
                item->snapshot_id = mesh->snapshot_id;
                item->wireframe = (mesh->material.kind == LV_3D_MAT_WIREFRAME);
                item->obj = child;
                lv_memcpy(&out[*count], item, sizeof(*item));
                (*count)++;
            }
        }
        else {
            collect_obj(child, parent_world, out, count, max_out);
        }
    }
}

uint32_t lv_3d_scene_collect(lv_obj_t * scene, lv_3d_draw_item_t * out, uint32_t max_out)
{
    float identity[16];
    lv_3d_mat4_identity(identity);
    uint32_t count = 0;
    collect_obj(scene, identity, out, &count, max_out);
    return count;
}

bool lv_3d_scene_has_volatile_meshes(lv_obj_t * scene)
{
    return collect_obj_has_local_dirty(scene) || lv_3d_scene_is_dirty(scene);
}

#endif /*LV_USE_3D*/
