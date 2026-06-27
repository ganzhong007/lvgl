/**
 * @file lv_draw_gpu_composite.h
 */

#ifndef LV_DRAW_GPU_COMPOSITE_H
#define LV_DRAW_GPU_COMPOSITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU_COMPOSITE

#include "../../display/lv_display.h"

void lv_draw_gpu_composite_init(void);
void lv_draw_gpu_composite_deinit(void);

/** Called from display flush: render queued 3D viewports. tex_id=0 uses display driver texture. */
void lv_gpu_composite_flush_3d(lv_display_t * disp);
void lv_gpu_composite_flush_3d_to_tex(lv_display_t * disp, unsigned int tex_id, int32_t w, int32_t h);

/** Alpha-blend SW framebuffer (2D/cursor) on top of the GPU texture after 3D pass. */
void lv_gpu_composite_overlay_2d_fb(lv_display_t * disp);
void lv_gpu_composite_overlay_2d_to_tex(lv_display_t * disp, unsigned int tex_id, int32_t w, int32_t h);
/** Alpha-blend 2D/cursor on the default framebuffer (after 3D texture blit). */
void lv_gpu_composite_overlay_2d_screen(lv_display_t * disp, int32_t w, int32_t h);

/** Read back alpha stats for LVGL_VERIFY (returns min alpha in corner samples). */
bool lv_gpu_composite_verify_alpha(lv_display_t * disp, uint8_t * min_alpha_out, uint32_t * opaque_count_out);

typedef struct {
    uint8_t corner_min_alpha;
    uint32_t corner_opaque_count;
    uint32_t region_samples;
    uint32_t region_visible_count;   /**< alpha >= 32 */
    uint32_t region_opaque_count;    /**< alpha >= 128 */
    uint32_t region_greenish_count;  /**< visible && g > r+8 && g > b+8 */
    uint32_t region_colorful_count;  /**< opaque && (r+g+b) >= 64 */
    uint8_t center_rgba[4];          /**< RGBA at screen center (GL readback order) */
    uint8_t region_max_alpha;        /**< max alpha in sampled region */
    uint8_t flush_max_alpha;         /**< max alpha probed right after 3D flush */
    uint32_t last_flush_items;       /**< meshes rendered in last 3D flush */
    uint32_t last_flush_viewports;   /**< viewports in last 3D flush */
    uint32_t flush_serial;           /**< incremented on each successful 3D flush */
} lv_gpu_composite_verify_stats_t;

/** Corner alpha + center-region content sampling for LVGL_VERIFY. */
bool lv_gpu_composite_verify_stats(lv_display_t * disp, lv_gpu_composite_verify_stats_t * stats);

typedef void (*lv_gpu_composite_frame_cb_t)(lv_display_t * disp);
void lv_gpu_composite_set_frame_ready_cb(lv_gpu_composite_frame_cb_t cb);
void lv_gpu_composite_notify_frame_ready(lv_display_t * disp);

/** Call after GL context is current (from lv_opengles_init). */
void lv_gpu_composite_caps_probe_on_context(void);

#endif /*LV_USE_DRAW_GPU_COMPOSITE*/

#ifdef __cplusplus
}
#endif

#endif /*LV_DRAW_GPU_COMPOSITE_H*/
