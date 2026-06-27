/**
 * @file lv_3d_mesh.c
 */

#include "lv_3d_internal.h"

#if LV_USE_3D

typedef struct {
    bool used;
    float w, h, d;
    bool wireframe;
    lv_3d_draw_item_t item;
} lv_3d_mesh_slot_t;

#define LV_3D_MESH_POOL_SIZE 512

static lv_3d_mesh_slot_t mesh_pool[LV_3D_MESH_POOL_SIZE];

lv_3d_mesh_id_t lv_3d_mesh_alloc_box(float w, float h, float d, bool wireframe)
{
    for(uint32_t i = 1; i < LV_3D_MESH_POOL_SIZE; i++) {
        if(!mesh_pool[i].used) {
            mesh_pool[i].used = true;
            mesh_pool[i].w = w;
            mesh_pool[i].h = h;
            mesh_pool[i].d = d;
            mesh_pool[i].wireframe = wireframe;
            mesh_pool[i].item.id = i;
            mesh_pool[i].item.w = w;
            mesh_pool[i].item.h = h;
            mesh_pool[i].item.d = d;
            mesh_pool[i].item.wireframe = wireframe;
            lv_3d_transform_init(&mesh_pool[i].item.transform);
            return i;
        }
    }
    return LV_3D_MESH_ID_NONE;
}

const lv_3d_draw_item_t * lv_3d_mesh_get_draw_item(lv_3d_mesh_id_t id)
{
    if(id == 0 || id >= LV_3D_MESH_POOL_SIZE || !mesh_pool[id].used) return NULL;
    return &mesh_pool[id].item;
}

lv_3d_draw_item_t * lv_3d_mesh_get_draw_item_mut(lv_3d_mesh_id_t id)
{
    if(id == 0 || id >= LV_3D_MESH_POOL_SIZE || !mesh_pool[id].used) return NULL;
    return &mesh_pool[id].item;
}

#endif /*LV_USE_3D*/
