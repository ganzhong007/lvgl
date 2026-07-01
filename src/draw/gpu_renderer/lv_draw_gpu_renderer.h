/**
 * @file lv_draw_gpu_renderer.h
 */

#ifndef LV_DRAW_GPU_RENDERER_H
#define LV_DRAW_GPU_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_RENDERER

#include "../../display/lv_display.h"

#define LV_GPU_RENDERER_UI_GENERIC         0
#define LV_GPU_RENDERER_UI_AR_LAUNCHER     1
#define LV_GPU_RENDERER_UI_NAV_AR          2
#define LV_GPU_RENDERER_UI_APP_FULLSCREEN  3

void lv_gpu_renderer_set_ui_mode(int mode);

void lv_draw_gpu_renderer_init(void);
void lv_draw_gpu_renderer_deinit(void);

/** Called from display flush: render queued 3D viewports. tex_id=0 uses display driver texture. */
void lv_gpu_renderer_flush_3d(lv_display_t * disp);
void lv_gpu_renderer_flush_3d_to_tex(lv_display_t * disp, unsigned int tex_id, int32_t w, int32_t h);

/** Alpha-blend SW framebuffer (2D/cursor) on top of the GPU texture after 3D pass. */
void lv_gpu_renderer_overlay_2d_fb(lv_display_t * disp);
void lv_gpu_renderer_overlay_2d_to_tex(lv_display_t * disp, unsigned int tex_id, int32_t w, int32_t h);
/** Alpha-blend 2D/cursor on the default framebuffer (after 3D texture blit). */
void lv_gpu_renderer_overlay_2d_screen(lv_display_t * disp, int32_t w, int32_t h);

/** Read back alpha stats for LVGL_VERIFY (returns min alpha in corner samples). */
bool lv_gpu_renderer_verify_alpha(lv_display_t * disp, uint8_t * min_alpha_out, uint32_t * opaque_count_out);

typedef struct {
    uint8_t corner_min_alpha;
    uint32_t corner_opaque_count;
    uint32_t region_samples;
    uint32_t region_visible_count;   /**< alpha >= 32 */
    uint32_t region_opaque_count;    /**< alpha >= 128 */
    uint32_t region_greenish_count;  /**< visible && g > r+8 && g > b+8 */
    uint32_t region_bluish_count;    /**< visible && b > r+8 && b > g+4 */
    uint32_t region_grayish_count;   /**< visible && channels within 20 && avg < 180 */
    uint32_t region_colorful_count;  /**< opaque && (r+g+b) >= 64 */
    uint8_t center_rgba[4];          /**< RGBA at screen center (GL readback order) */
    uint8_t region_max_alpha;        /**< max alpha in sampled region */
    uint8_t flush_max_alpha;         /**< max alpha probed right after 3D flush */
    uint32_t last_flush_items;       /**< meshes rendered in last 3D flush */
    uint32_t last_flush_viewports;   /**< viewports in last 3D flush */
    uint32_t flush_serial;           /**< incremented on each successful 3D flush */
    /** M0 path observability (last completed frame) */
    uint32_t gpu_2d_tasks;
    uint32_t gpu_3d_draws;
    uint32_t sw_overlay_uploads;
    uint32_t sw_2d_raster_tasks;
    uint32_t fg_pass_count;
    uint32_t fg_batch_count;
    uint32_t fg_material_batches;
    uint32_t fg_gl_finish_count;
    char gl_renderer[128];
} lv_gpu_renderer_verify_stats_t;

/** Corner alpha + center-region content sampling for LVGL_VERIFY. */
bool lv_gpu_renderer_verify_stats(lv_display_t * disp, lv_gpu_renderer_verify_stats_t * stats);

/** Per-frame GPU path counters only (no glReadPixels — safe for live HUD). */
typedef struct {
    uint32_t gpu_2d_tasks;
    uint32_t gpu_3d_draws;
    uint32_t sw_overlay_uploads;
    uint32_t sw_2d_raster_tasks;
    uint32_t last_flush_items;
    uint32_t last_flush_viewports;
    uint32_t flush_serial;
    uint32_t fg_pass_count;
    uint32_t fg_batch_count;
    uint32_t fg_material_batches;
    uint32_t fg_gl_finish_count;
    char gl_renderer[128];
} lv_gpu_renderer_path_stats_t;

void lv_gpu_renderer_get_path_stats(lv_gpu_renderer_path_stats_t * stats);

/** Debug: dump composite texture in LVGL row order (header: uint32 w, h, then RGBA rows). */
bool lv_gpu_renderer_dump_frame_lvgl(lv_display_t * disp, const char * path);
/** Debug: dump default framebuffer (window back buffer) in LVGL row order. */
bool lv_gpu_renderer_dump_screen_lvgl(int32_t w, int32_t h, const char * path);

/** Restore default window framebuffer draw/read buffers after off-screen GPU passes. */
void lv_gpu_renderer_restore_default_framebuffer(void);

/** Blit composite texture to the window back buffer. Returns false if unsupported or incomplete. */
bool lv_gpu_renderer_present_tex_to_window(unsigned int tex_id, int32_t w, int32_t h);

/** Slow path: read GPU texture to CPU and re-upload for window present (VM fallback). */
bool lv_gpu_renderer_present_tex_readback(unsigned int tex_id, int32_t w, int32_t h);

typedef void (*lv_gpu_renderer_frame_cb_t)(lv_display_t * disp);
void lv_gpu_renderer_set_frame_ready_cb(lv_gpu_renderer_frame_cb_t cb);
void lv_gpu_renderer_notify_frame_ready(lv_display_t * disp);

/** Call after GL context is current (from lv_opengles_init). */
void lv_gpu_renderer_caps_probe_on_context(void);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_DRAW_GPU_RENDERER_H*/
