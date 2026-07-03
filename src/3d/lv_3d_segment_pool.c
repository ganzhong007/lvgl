/**
 * @file lv_3d_segment_pool.c
 */

#include "../include/lvgl/3d/lv_3d_segment_pool.h"

#if LV_USE_3D && LV_USE_3D_SEGMENT_POOL

#include "../widgets/3d/lv_3dmesh_private.h"
#include "../widgets/3d/lv_3dviewport_private.h"

#define LV_3D_SEGMENT_POOL_MAX_SLOTS 24

typedef struct {
    uint32_t seg_id;
    float base_z;
    lv_obj_t * root;
} lv_3d_segment_slot_t;

struct _lv_3d_segment_pool_t {
    lv_obj_t * scene;
    lv_3d_segment_pool_cfg_t cfg;
    lv_3d_segment_build_fn_t build_fn;
    void * user_data;
    lv_3d_segment_slot_t slots[LV_3D_SEGMENT_POOL_MAX_SLOTS];
    uint16_t slot_count;
    uint32_t recycle_count;
    uint32_t next_seg_id;
};

static void segment_rebuild(lv_3d_segment_pool_t * pool, lv_3d_segment_slot_t * slot)
{
    if(!slot->root || !pool->build_fn) return;
    lv_obj_clean(slot->root);
    pool->build_fn(slot->root, slot->seg_id, slot->base_z, pool->user_data);
}

static void segment_shift_meshes(lv_3d_segment_pool_t * pool, lv_3d_segment_slot_t * slot, float dz)
{
    LV_UNUSED(pool);
    if(!slot->root) return;

    uint32_t n = lv_obj_get_child_count(slot->root);
    for(uint32_t i = 0; i < n; i++) {
        lv_obj_t * child = lv_obj_get_child(slot->root, i);
        if(!lv_obj_check_type(child, &lv_3dmesh_class)) continue;
        lv_3dmesh_t * mesh = (lv_3dmesh_t *)child;
        lv_3dmesh_set_position(child, mesh->pos[0], mesh->pos[1], mesh->pos[2] + dz);
    }
}

static float segment_min_base_z(const lv_3d_segment_pool_t * pool)
{
    float min_z = 1.0e9f;
    for(uint16_t i = 0; i < pool->slot_count; i++) {
        if(pool->slots[i].base_z < min_z) min_z = pool->slots[i].base_z;
    }
    return min_z;
}

static void invalidate_scene_viewport(lv_obj_t * scene)
{
    lv_obj_t * scr = lv_obj_get_screen(scene);
    if(!scr) return;

    uint32_t n = lv_obj_get_child_count(scr);
    for(uint32_t i = 0; i < n; i++) {
        lv_obj_t * child = lv_obj_get_child(scr, i);
        if(lv_obj_check_type(child, &lv_3dviewport_class)) {
            lv_obj_invalidate(child);
            return;
        }
    }
}

lv_3d_segment_pool_t * lv_3d_segment_pool_create(lv_obj_t * scene, const lv_3d_segment_pool_cfg_t * cfg)
{
    if(!scene || !cfg || cfg->pool_size == 0) return NULL;
    if(cfg->pool_size > LV_3D_SEGMENT_POOL_MAX_SLOTS) return NULL;

    lv_3d_segment_pool_t * pool = lv_malloc_zeroed(sizeof(*pool));
    if(!pool) return NULL;

    pool->scene = scene;
    pool->cfg = *cfg;
    if(pool->cfg.recycle_z == 0.0f) pool->cfg.recycle_z = 1400.0f;
    pool->slot_count = cfg->pool_size;

    for(uint16_t i = 0; i < pool->slot_count; i++) {
        lv_3d_segment_slot_t * slot = &pool->slots[i];
        slot->seg_id = pool->next_seg_id++;
        slot->base_z = -200.0f - (float)i * pool->cfg.segment_length;
        slot->root = lv_obj_create(scene);
        lv_obj_remove_flag(slot->root, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_style_all(slot->root);
        lv_obj_add_flag(slot->root, LV_OBJ_FLAG_HIDDEN);
    }

    return pool;
}

void lv_3d_segment_pool_delete(lv_3d_segment_pool_t * pool)
{
    if(!pool) return;
    for(uint16_t i = 0; i < pool->slot_count; i++) {
        if(pool->slots[i].root) lv_obj_delete(pool->slots[i].root);
    }
    lv_free(pool);
}

void lv_3d_segment_pool_set_build_fn(lv_3d_segment_pool_t * pool, lv_3d_segment_build_fn_t fn, void * user)
{
    if(!pool) return;
    pool->build_fn = fn;
    pool->user_data = user;
    for(uint16_t i = 0; i < pool->slot_count; i++) {
        segment_rebuild(pool, &pool->slots[i]);
    }
}

void lv_3d_segment_pool_tick(lv_3d_segment_pool_t * pool, float dt)
{
    if(!pool || dt <= 0.0f) return;

    /* +Z scroll: buildings ahead move toward camera then recycle behind = forward nav. */
    const float dz = pool->cfg.scroll_speed * dt;

    for(uint16_t i = 0; i < pool->slot_count; i++) {
        lv_3d_segment_slot_t * slot = &pool->slots[i];
        slot->base_z += dz;
        segment_shift_meshes(pool, slot, dz);

        if(slot->base_z > pool->cfg.recycle_z) {
            const float min_z = segment_min_base_z(pool);
            slot->base_z = min_z - pool->cfg.segment_length;
            slot->seg_id = pool->next_seg_id++;
            segment_rebuild(pool, slot);
            pool->recycle_count++;
        }
    }

    lv_obj_invalidate(pool->scene);
    lv_obj_invalidate(lv_obj_get_screen(pool->scene));
    invalidate_scene_viewport(pool->scene);
}

void lv_3d_segment_pool_get_stats(const lv_3d_segment_pool_t * pool, lv_3d_segment_pool_stats_t * stats)
{
    if(!pool || !stats) return;
    stats->recycle_count = pool->recycle_count;
    stats->min_seg_z = 1.0e9f;
    stats->max_seg_z = -1.0e9f;
    for(uint16_t i = 0; i < pool->slot_count; i++) {
        float z = pool->slots[i].base_z;
        if(z < stats->min_seg_z) stats->min_seg_z = z;
        if(z > stats->max_seg_z) stats->max_seg_z = z;
    }
}

#endif /*LV_USE_3D && LV_USE_3D_SEGMENT_POOL*/
