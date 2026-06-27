/**
 * @file lv_3d_scene.c
 */

#include "lv_3d_internal.h"
#include "../widgets/3d/lv_3dmesh_private.h"

#if LV_USE_3D

static void collect_obj(lv_obj_t * obj, const float parent_world[16], lv_3d_draw_item_t * out, uint32_t * count,
                          uint32_t max_out)
{
    uint32_t i;
    for(i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t * child = lv_obj_get_child(obj, i);
        if(lv_obj_check_type(child, &lv_3dmesh_class)) {
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

#endif /*LV_USE_3D*/
