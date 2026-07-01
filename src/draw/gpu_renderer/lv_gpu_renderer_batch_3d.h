/**
 * @file lv_gpu_renderer_batch_3d.h — 3D draw-item material batch helpers
 */

#ifndef LV_GPU_RENDERER_BATCH_3D_H
#define LV_GPU_RENDERER_BATCH_3D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_RENDERER && LV_USE_3D

#include "../../3d/lv_3d_internal.h"

/** Stable sort opaque indices by material kind (better GPU state locality). */
void lv_gpu_renderer_batch_3d_sort_opaque(const lv_3d_draw_item_t * items, uint32_t item_count,
                                           uint32_t * opaque_order, uint32_t opaque_count);

/** Count material-kind runs after opaque sort (metrics / batch planning). */
uint32_t lv_gpu_renderer_batch_3d_count_material_runs(const lv_3d_draw_item_t * items,
                                                       const uint32_t * opaque_order,
                                                       uint32_t opaque_count);

#endif /*LV_USE_DRAW_GPU_RENDERER && LV_USE_3D*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_BATCH_3D_H*/
