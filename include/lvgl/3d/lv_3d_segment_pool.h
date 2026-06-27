/**
 * @file lv_3d_segment_pool.h
 * Parallax building segments with spawn/recycle (scenario 2).
 */

#ifndef LV_3D_SEGMENT_POOL_H
#define LV_3D_SEGMENT_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_3D_SEGMENT_POOL

#include "../core/lv_obj.h"

typedef struct {
    float segment_length;
    float scroll_speed;
    uint16_t pool_size;
    float recycle_z;
} lv_3d_segment_pool_cfg_t;

typedef struct _lv_3d_segment_pool_t lv_3d_segment_pool_t;

/** Populate segment_root with lv_3dmesh children at world z around seg_base_z. */
typedef void (*lv_3d_segment_build_fn_t)(lv_obj_t * segment_root, uint32_t seg_id, float seg_base_z, void * user);

typedef struct {
    uint32_t recycle_count;
    float min_seg_z;
    float max_seg_z;
} lv_3d_segment_pool_stats_t;

lv_3d_segment_pool_t * lv_3d_segment_pool_create(lv_obj_t * scene, const lv_3d_segment_pool_cfg_t * cfg);
void lv_3d_segment_pool_delete(lv_3d_segment_pool_t * pool);
void lv_3d_segment_pool_set_build_fn(lv_3d_segment_pool_t * pool, lv_3d_segment_build_fn_t fn, void * user);
void lv_3d_segment_pool_tick(lv_3d_segment_pool_t * pool, float dt);
void lv_3d_segment_pool_get_stats(const lv_3d_segment_pool_t * pool, lv_3d_segment_pool_stats_t * stats);

#endif /*LV_USE_3D && LV_USE_3D_SEGMENT_POOL*/

#ifdef __cplusplus
}
#endif

#endif /*LV_3D_SEGMENT_POOL_H*/
