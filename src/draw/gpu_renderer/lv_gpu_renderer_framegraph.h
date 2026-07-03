/**
 * @file lv_gpu_renderer_framegraph.h — Record / build / execute frame passes
 */

#ifndef LV_GPU_RENDERER_FRAMEGRAPH_H
#define LV_GPU_RENDERER_FRAMEGRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_RENDERER

#include "../../misc/lv_area.h"
#include "../lv_draw_private.h"
#include "../../include/lvgl/draw/lv_draw_3d.h"

typedef enum {
    LV_GPU_UI_MODE_GENERIC = 0,
    LV_GPU_UI_MODE_AR_LAUNCHER,
    LV_GPU_UI_MODE_NAV_AR,
    LV_GPU_UI_MODE_APP_FULLSCREEN,
} lv_gpu_ui_mode_t;

typedef enum {
    LV_GPU_FG_SCREEN = 0,
    LV_GPU_FG_OVERLAY,
    LV_GPU_FG_VIEWPORT_3D,
    LV_GPU_FG_PLANE_3D,
    LV_GPU_FG_LAYER,
    LV_GPU_FG_FULLSCREEN_APP,
} lv_gpu_fg_space_t;

typedef enum {
    LV_GPU_PATH_NONE = 0,
    LV_GPU_PATH_3D_VIEWPORT,
    LV_GPU_PATH_2D_NATIVE,
    LV_GPU_PATH_2D_RASTER,
    LV_GPU_PATH_SW_DIRECT,
} lv_gpu_renderer_path_t;

typedef struct {
    uint32_t gpu_2d_recorded;
    uint32_t gpu_3d_vp_recorded;
    uint32_t pass_count;
    uint32_t batch_count;
    uint32_t material_batches;
    uint32_t gl_flush_count;
    uint32_t gl_finish_count;
} lv_gpu_renderer_fg_stats_t;

void lv_gpu_renderer_fg_init(void);
void lv_gpu_renderer_fg_deinit(void);

void lv_gpu_renderer_fg_set_ui_mode(lv_gpu_ui_mode_t mode);
lv_gpu_ui_mode_t lv_gpu_renderer_fg_get_ui_mode(void);

/** Per-frame record (during lv_refr dispatch). */
void lv_gpu_renderer_fg_record_reset(void);

bool lv_gpu_renderer_fg_record_viewport(lv_obj_t * scene, lv_obj_t * camera, const lv_area_t * area);

bool lv_gpu_renderer_fg_record_2d_task(lv_draw_task_t * task);

/**
 * Score for evaluate: lower = gpu_renderer prefers this task (LVGL convention).
 * Returns 0 if GPU should not claim; otherwise 1..99.
 */
int32_t lv_gpu_renderer_fg_evaluate_score(lv_draw_task_t * task, lv_gpu_renderer_path_t * path_out);

/** True if task can be queued for GLES2 native 2D batch (display FB). */
bool lv_gpu_renderer_fg_can_gpu_native_2d(const lv_draw_task_t * task);

bool lv_gpu_renderer_fg_queue_2d_task(lv_draw_task_t * task);

/** True when viewport or 2D batch queues have work for the next flush. */
bool lv_gpu_renderer_fg_has_pending(void);

/** Re-queue the last viewport pass (animated 3D when DRAW_MAIN was skipped). */
bool lv_gpu_renderer_fg_restore_last_viewport(void);

/**
 * Build + execute recorded frame into tex_id.
 * Updates fg_stats and caller path counters via out params.
 */
void lv_gpu_renderer_fg_execute(unsigned int tex_id, int32_t dw, int32_t dh,
                                 uint32_t * gpu_2d_out, uint32_t * gpu_3d_out,
                                 uint32_t * sw_raster_out, uint8_t * max_alpha_out);

const lv_gpu_renderer_fg_stats_t * lv_gpu_renderer_fg_get_stats(void);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_FRAMEGRAPH_H*/
