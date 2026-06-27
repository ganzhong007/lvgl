/**
 * @file lv_3d_plane_bake.h
 * Bake lv_obj subtrees to GPU textures for 3D PLANE tiles (scenario 1).
 */

#ifndef LV_3D_PLANE_BAKE_H
#define LV_3D_PLANE_BAKE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_SNAPSHOT

#include "../core/lv_obj.h"
#include "lv_3d.h"

typedef enum {
    LV_3D_PLANE_SRC_LIVE,
    LV_3D_PLANE_SRC_SNAPSHOT,
} lv_3d_plane_src_t;

/** Bake obj subtree to snapshot; returns id or LV_3D_SNAPSHOT_ID_NONE on failure. */
lv_3d_snapshot_id_t lv_3d_plane_bake(lv_obj_t * obj, lv_3d_plane_src_t src);

/** Mark snapshot stale so next bake re-renders the subtree. */
void lv_3d_plane_invalidate(lv_obj_t * obj);

/** Upload pending CPU snapshots to GL (call with GL context current). */
void lv_3d_plane_upload_all(void);

/** GL texture for rendered snapshot; 0 if not ready. */
unsigned int lv_3d_plane_get_gl_texture(lv_3d_snapshot_id_t id);

/** Debug: write CPU snapshot as LVGL-order RGBA (header: uint32 w, uint32 h, then BGRA rows). */
bool lv_3d_plane_dump_snapshot_lvgl(lv_3d_snapshot_id_t id, const char * path);

#endif /*LV_USE_3D && LV_USE_SNAPSHOT*/

#ifdef __cplusplus
}
#endif

#endif /*LV_3D_PLANE_BAKE_H*/
