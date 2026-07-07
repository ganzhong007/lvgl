/**
 * @file lv_gpu_renderer_framegraph.h — Record / build / execute frame passes (DAG)
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
#include "lv_gpu_renderer_gles2_2d.h"

typedef enum {
    LV_GPU_UI_MODE_GENERIC = 0,
    LV_GPU_UI_MODE_AR_LAUNCHER,
    LV_GPU_UI_MODE_NAV_AR,
    LV_GPU_UI_MODE_APP_FULLSCREEN,
    LV_GPU_UI_MODE_WIREFRAME_BENCH,
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
    LV_GPU_PATH_DEFER_LAYER,
} lv_gpu_renderer_path_t;

typedef enum {
    LV_GPU_FG_NODE_VIEWPORT = 0,
    LV_GPU_FG_NODE_2D,
    LV_GPU_FG_NODE_LAYER,
} lv_gpu_fg_node_kind_t;

typedef struct {
    lv_gpu_fg_node_kind_t kind;
    lv_gpu_fg_space_t space;
    lv_gpu_renderer_path_t path;
    uint32_t record_index;
    uint32_t z_key;
    uint32_t shader_key;
    uint32_t pixel_area;
    lv_area_t clip;
    union {
        struct {
            lv_obj_t * scene;
            lv_obj_t * camera;
            lv_area_t area;
        } viewport;
        lv_gpu_renderer_gles2_cmd_t cmd_2d;
        struct {
            lv_draw_image_dsc_t image;
            lv_area_t area;
            lv_area_t clip;
        } layer;
    } u;
} lv_gpu_fg_node_t;

typedef struct {
    uint32_t gpu_2d_recorded;
    uint32_t gpu_3d_vp_recorded;
    uint32_t pass_count;
    uint32_t batch_count;
    uint32_t material_batches;
    uint32_t gl_flush_count;
    uint32_t gl_finish_count;
    uint32_t node_count;
    uint32_t skipped_static_3d;
    uint32_t unified_overlay_merged;
    uint32_t draw_calls;
    uint32_t fbo_switches;
    uint32_t sw_upload_bytes;
    uint32_t overdraw_pixels;
    uint32_t energy_cost;
} lv_gpu_renderer_fg_stats_t;

void lv_gpu_renderer_fg_init(void);
void lv_gpu_renderer_fg_deinit(void);

void lv_gpu_renderer_fg_set_ui_mode(lv_gpu_ui_mode_t mode);
lv_gpu_ui_mode_t lv_gpu_renderer_fg_get_ui_mode(void);

void lv_gpu_renderer_fg_record_reset(void);

bool lv_gpu_renderer_fg_record_viewport(lv_obj_t * scene, lv_obj_t * camera, const lv_area_t * area);

bool lv_gpu_renderer_fg_record_2d_task(lv_draw_task_t * task);

bool lv_gpu_renderer_fg_record_layer_task(lv_draw_task_t * task);

int32_t lv_gpu_renderer_fg_evaluate_score(lv_draw_task_t * task, lv_gpu_renderer_path_t * path_out);

bool lv_gpu_renderer_fg_can_gpu_native_2d(const lv_draw_task_t * task);

bool lv_gpu_renderer_fg_can_gpu_layer(const lv_draw_task_t * task);

bool lv_gpu_renderer_fg_queue_2d_task(lv_draw_task_t * task);

/** Classify 2D task → fg space (§7.7.4). */
lv_gpu_fg_space_t lv_gpu_renderer_fg_classify_2d_space(const lv_draw_task_t * task);

bool lv_gpu_renderer_fg_has_pending(void);

bool lv_gpu_renderer_fg_has_restorable_viewport(void);

bool lv_gpu_renderer_fg_restore_last_viewport(void);

void lv_gpu_renderer_fg_build(void);

void lv_gpu_renderer_fg_execute(unsigned int tex_id, int32_t dw, int32_t dh,
                                 uint32_t * gpu_2d_out, uint32_t * gpu_3d_out,
                                 uint32_t * sw_raster_out, uint8_t * max_alpha_out);

const lv_gpu_renderer_fg_stats_t * lv_gpu_renderer_fg_get_stats(void);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_FRAMEGRAPH_H*/
