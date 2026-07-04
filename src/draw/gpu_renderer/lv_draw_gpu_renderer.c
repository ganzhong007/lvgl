/**
 * @file lv_draw_gpu_renderer.c — LVGL draw unit + frame composite
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "lv_draw_gpu_renderer.h"

#if LV_USE_DRAW_OPENGLES || LV_USE_DRAW_NANOVG
    #error "LV_USE_DRAW_GPU_RENDERER cannot be combined with OPENGLES or NANOVG draw units"
#endif

#include "lv_gpu_renderer_caps.h"
#include "lv_gpu_renderer_framegraph.h"
#include "lv_gpu_renderer_gles2_3d.h"
#include "lv_gpu_renderer_gles2_2d.h"
#include "../lv_draw_private.h"
#include "../../core/lv_refr_private.h"
#include "../../display/lv_display_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_driver.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_texture_private.h"

#include <stdlib.h>
#include "../../include/lvgl/draw/lv_draw_3d.h"
#include <stdio.h>

#define DRAW_UNIT_ID_GPU_RENDERER 11

typedef struct {
    lv_draw_unit_t base_unit;
    lv_draw_task_t * task_act;
    lv_gpu_renderer_caps_t caps;
} lv_draw_gpu_renderer_unit_t;

static lv_draw_gpu_renderer_unit_t * g_unit;
static unsigned int g_last_flush_tex_id;
static lv_gpu_renderer_frame_cb_t g_frame_ready_cb;
static uint32_t g_last_flush_item_count;
static uint32_t g_last_flush_vp_count;
static uint8_t g_last_frame_max_alpha;
static uint32_t g_flush_serial;

typedef struct {
    uint32_t gpu_2d_tasks;
    uint32_t gpu_3d_draws;
    uint32_t sw_overlay_uploads;
    uint32_t sw_2d_raster_tasks;
    uint32_t fg_pass_count;
    uint32_t fg_batch_count;
    uint32_t fg_material_batches;
    uint32_t fg_gl_finish_count;
    char gl_renderer[128];
} lv_gpu_renderer_path_frame_t;

static lv_gpu_renderer_path_frame_t g_path_stats;
static bool g_gl_renderer_logged;
static bool g_overlay_2d_enable = true;

static int32_t gpu_renderer_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task);
static int32_t gpu_renderer_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer);
static void gpu_renderer_flush_internal(unsigned int tex_id, int32_t dw, int32_t dh);
static void gpu_renderer_clear_tex(unsigned int tex_id, int32_t w, int32_t h, float r, float g, float b, float a);

bool lv_gpu_renderer_debug_2d_only(void)
{
    static int cached = -1;
    if(cached < 0) {
        const char * env = getenv("LVGL_GPU_2D_ONLY");
        cached = (env && env[0] == '1') ? 1 : 0;
        if(cached) {
            LV_LOG_USER("GPU debug: 2D batch only (LVGL_GPU_2D_ONLY=1)");
        }
    }
    return cached != 0;
}

void lv_gpu_renderer_flush_2d_only(lv_display_t * disp)
{
    int32_t dw = lv_display_get_horizontal_resolution(disp);
    int32_t dh = lv_display_get_vertical_resolution(disp);
    gpu_renderer_flush_internal(lv_opengles_texture_get_texture_id(disp), dw, dh);
}

void lv_gpu_renderer_clear_tex_for_debug(unsigned int tex_id, int32_t w, int32_t h)
{
    gpu_renderer_clear_tex(tex_id, w, h, 0.0f, 0.75f, 0.15f, 1.0f);
    {
        unsigned int probe_fbo = 0;
        uint8_t center[4] = {0};
        GL_CALL(glGenFramebuffers(1, &probe_fbo));
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, probe_fbo));
        GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0));
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            GL_CALL(glReadPixels(w / 2, h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center));
        }
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDeleteFramebuffers(1, &probe_fbo));
        static bool logged;
        if(!logged) {
            LV_LOG_USER("2D-only tex clear probe center RGBA %u %u %u %u (fbo ok=%d)",
                        center[0], center[1], center[2], center[3],
                        (int)(probe_fbo != 0));
            logged = true;
        }
    }
}

static void gpu_renderer_clear_tex(unsigned int tex_id, int32_t w, int32_t h, float r, float g, float b, float a)
{
    if(tex_id == 0 || w < 1 || h < 1) return;

    unsigned int fbo = 0;
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0));
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        GL_CALL(glViewport(0, 0, w, h));
        GL_CALL(glDisable(GL_SCISSOR_TEST));
        GL_CALL(glDisable(GL_BLEND));
        GL_CALL(glClearColor(r, g, b, a));
        GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
    }
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &fbo));
}

void lv_draw_gpu_renderer_init(void)
{
    lv_draw_gpu_renderer_unit_t * u = lv_draw_create_unit(sizeof(lv_draw_gpu_renderer_unit_t));
    u->base_unit.evaluate_cb = gpu_renderer_evaluate;
    u->base_unit.dispatch_cb = gpu_renderer_dispatch;
    u->base_unit.name = "GPU_RENDERER";
    g_unit = u;
    lv_gpu_renderer_fg_init();
}

void lv_draw_gpu_renderer_deinit(void)
{
#if LV_USE_3D
    lv_gpu_renderer_gles2_3d_deinit();
#endif
    lv_gpu_renderer_gles2_2d_deinit();
    lv_gpu_renderer_fg_deinit();
    g_unit = NULL;
}

void lv_gpu_renderer_set_ui_mode(int mode)
{
    lv_gpu_renderer_fg_set_ui_mode((lv_gpu_ui_mode_t)mode);
}

bool lv_gpu_renderer_overlay_2d_enabled(void)
{
    return g_overlay_2d_enable;
}

void lv_gpu_renderer_set_overlay_2d_enable(bool enable)
{
    g_overlay_2d_enable = enable;
}

void lv_gpu_renderer_set_skip_alpha_probe(bool skip)
{
    lv_gpu_renderer_gles2_3d_set_skip_alpha_probe(skip);
}

static void path_stats_cache_gl_renderer(void)
{
    if(g_path_stats.gl_renderer[0]) return;
    const char * r = (const char *)glGetString(GL_RENDERER);
    if(r) {
        lv_strncpy(g_path_stats.gl_renderer, r, sizeof(g_path_stats.gl_renderer) - 1);
        g_path_stats.gl_renderer[sizeof(g_path_stats.gl_renderer) - 1] = '\0';
    }
}

static void path_stats_from_fg(void)
{
    const lv_gpu_renderer_fg_stats_t * fg = lv_gpu_renderer_fg_get_stats();
    if(!fg) return;
    g_path_stats.fg_pass_count = fg->pass_count;
    g_path_stats.fg_batch_count = fg->batch_count;
    g_path_stats.fg_material_batches = fg->material_batches;
    g_path_stats.fg_gl_finish_count = fg->gl_finish_count;
}

static void gpu_renderer_flush_internal(unsigned int tex_id, int32_t dw, int32_t dh)
{
    if(!g_unit || tex_id == 0) return;
    if(!lv_gpu_renderer_fg_has_pending()) {
        lv_gpu_renderer_fg_restore_last_viewport();
    }
    if(!lv_gpu_renderer_fg_has_pending()) {
        static bool logged;
        if(!logged) {
            LV_LOG_USER("GPU flush_3d: no pending 3D/2D (vp=%u q2d=%u)",
                        (unsigned)lv_gpu_renderer_fg_get_stats()->gpu_3d_vp_recorded,
                        (unsigned)lv_gpu_renderer_gles2_2d_queue_count());
            logged = true;
        }
        return;
    }

    path_stats_cache_gl_renderer();
    g_path_stats.gpu_2d_tasks = 0;
    g_path_stats.gpu_3d_draws = 0;
    g_path_stats.sw_2d_raster_tasks = 0;

    g_last_flush_tex_id = tex_id;
    g_last_flush_vp_count = lv_gpu_renderer_fg_get_stats()->gpu_3d_vp_recorded;
    g_last_flush_item_count = 0;
    g_last_frame_max_alpha = 0;

    uint32_t sw_raster = 0;
    lv_gpu_renderer_fg_execute(tex_id, dw, dh,
                                &g_path_stats.gpu_2d_tasks,
                                &g_last_flush_item_count,
                                &sw_raster,
                                &g_last_frame_max_alpha);
    g_path_stats.gpu_3d_draws = g_last_flush_item_count;
    g_path_stats.sw_2d_raster_tasks = sw_raster;
    path_stats_from_fg();

    {
        static bool logged_full;
        static bool logged_2d;
        if(lv_gpu_renderer_debug_2d_only()) {
            if(!logged_2d) {
                LV_LOG_USER("GPU flush_2d_only: batch=%u sw_raster=%u",
                            (unsigned)g_path_stats.gpu_2d_tasks,
                            (unsigned)g_path_stats.sw_2d_raster_tasks);
                logged_2d = true;
            }
        }
        else if(!logged_full) {
            LV_LOG_USER("GPU flush_3d: vp=%u items=%u 2d=%u max_alpha=%u",
                        (unsigned)g_last_flush_vp_count,
                        (unsigned)g_last_flush_item_count,
                        (unsigned)g_path_stats.gpu_2d_tasks,
                        (unsigned)g_last_frame_max_alpha);
            logged_full = true;
        }
    }

    g_flush_serial++;
}

void lv_gpu_renderer_set_frame_ready_cb(lv_gpu_renderer_frame_cb_t cb)
{
    g_frame_ready_cb = cb;
}

void lv_gpu_renderer_notify_frame_ready(lv_display_t * disp)
{
    if(g_frame_ready_cb) g_frame_ready_cb(disp);
}

bool lv_gpu_renderer_has_pending_composite(void)
{
    return lv_gpu_renderer_fg_has_pending() || lv_gpu_renderer_fg_has_restorable_viewport();
}

bool lv_gpu_renderer_has_restorable_viewport(void)
{
    return lv_gpu_renderer_fg_has_restorable_viewport();
}

void lv_gpu_renderer_flush_3d(lv_display_t * disp)
{
    int32_t dw = lv_display_get_horizontal_resolution(disp);
    int32_t dh = lv_display_get_vertical_resolution(disp);
    gpu_renderer_flush_internal(lv_opengles_texture_get_texture_id(disp), dw, dh);
}

void lv_gpu_renderer_flush_3d_to_tex(lv_display_t * disp, unsigned int tex_id, int32_t w, int32_t h)
{
    LV_UNUSED(disp);
    gpu_renderer_flush_internal(tex_id, w, h);
}

void lv_gpu_renderer_overlay_2d_fb(lv_display_t * disp)
{
    if(!g_overlay_2d_enable) return;
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    lv_gpu_renderer_overlay_2d_to_tex(disp, 0, w, h);
}

#if LV_COLOR_DEPTH == 32
static bool overlay_pixel_visible(const uint8_t * p, lv_color_format_t cf)
{
    if(lv_color_format_has_alpha(cf)) {
        if(p[3] <= 16) return false;
        /* Default widget white background — not intentional 2D UI */
        if(p[0] > 240 && p[1] > 240 && p[2] > 240 && p[3] > 240) return false;
        return true;
    }
    if((p[0] | p[1] | p[2]) <= 16) return false;
    if(p[0] > 240 && p[1] > 240 && p[2] > 240) return false;
    return true;
}

static bool overlay_fb_has_visible_2d(const uint8_t * px, uint32_t stride, int32_t w, int32_t h, lv_color_format_t cf)
{
    for(int32_t y = 0; y < h; y += 32) {
        for(int32_t x = 0; x < w; x += 32) {
            const uint8_t * p = px + (uint32_t)y * stride + (uint32_t)x * 4;
            if(overlay_pixel_visible(p, cf)) return true;
        }
    }
    return false;
}

static bool overlay_upload_sw_tex(lv_opengles_texture_t * texture, lv_display_t * disp, int32_t w, int32_t h,
                                  unsigned int * sw_tex_out)
{
    lv_color_format_t cf = lv_display_get_color_format(disp);
    uint32_t stride = lv_draw_buf_width_to_stride(w, cf);
    const uint8_t * px = texture->fb1;

    if(!overlay_fb_has_visible_2d(px, stride, w, h, cf)) return false;

    static unsigned int sw_tex;
    if(sw_tex == 0) {
        GL_CALL(glGenTextures(1, &sw_tex));
    }

    GL_CALL(glBindTexture(GL_TEXTURE_2D, sw_tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    lv_opengles_teximage_bgra8888(0, w, h, texture->fb1, stride);
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    if(sw_tex_out) *sw_tex_out = sw_tex;
    return true;
}

static void overlay_restore_default_fb_state(void)
{
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
    GL_CALL(glUseProgram(0));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
#if defined(glBindVertexArray)
    GL_CALL(glBindVertexArray(0));
#endif
    for(int ai = 0; ai < 4; ai++) {
        GL_CALL(glDisableVertexAttribArray((unsigned int)ai));
    }
#if !LV_USE_EGL
    {
        GLenum draw_buf = GL_BACK;
        GL_CALL(glDrawBuffers(1, &draw_buf));
        GL_CALL(glReadBuffer(GL_BACK));
    }
#endif
}
#endif /*LV_COLOR_DEPTH == 32*/

void lv_gpu_renderer_overlay_2d_screen(lv_display_t * disp, int32_t w, int32_t h)
{
#if LV_COLOR_DEPTH != 32
    LV_UNUSED(disp);
    LV_UNUSED(w);
    LV_UNUSED(h);
    return;
#else
    lv_opengles_texture_t * texture = lv_display_get_driver_data(disp);
    if(!texture || !texture->fb1 || w < 1 || h < 1) return;

    unsigned int sw_tex = 0;
    if(!overlay_upload_sw_tex(texture, disp, w, h, &sw_tex)) return;

    g_path_stats.sw_overlay_uploads++;

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
#if !LV_USE_EGL
    {
        GLenum draw_buf = GL_BACK;
        GL_CALL(glDrawBuffers(1, &draw_buf));
        GL_CALL(glReadBuffer(GL_BACK));
    }
#endif

    lv_area_t full = { 0, 0, w - 1, h - 1 };
    lv_opengles_reinit_state();
    lv_opengles_render_texture_rbswap(sw_tex, &full, LV_OPA_COVER, w, h, &full, false, false);
    overlay_restore_default_fb_state();
#endif
}

void lv_gpu_renderer_overlay_2d_to_tex(lv_display_t * disp, unsigned int tex_id, int32_t w, int32_t h)
{
    if(!g_overlay_2d_enable) return;
    lv_opengles_texture_t * texture = lv_display_get_driver_data(disp);
    if(!texture || !texture->fb1) return;

    if(tex_id == 0) tex_id = texture->texture_id;
    if(tex_id == 0) return;

    if(w < 1 || h < 1) return;

#if LV_COLOR_DEPTH != 32
    return;
#else
    unsigned int sw_tex = 0;
    if(!overlay_upload_sw_tex(texture, disp, w, h, &sw_tex)) return;

    g_path_stats.sw_overlay_uploads++;

    unsigned int fbo = 0;
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0));

#if !LV_USE_EGL
    {
        GLenum draw_buf = GL_COLOR_ATTACHMENT0;
        GL_CALL(glDrawBuffers(1, &draw_buf));
        GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0));
    }
#endif

    lv_area_t full = { 0, 0, w - 1, h - 1 };
    lv_opengles_reinit_state();
    lv_opengles_render_texture_rbswap(sw_tex, &full, LV_OPA_COVER, w, h, &full, false, false);

    GL_CALL(glDeleteFramebuffers(1, &fbo));
    overlay_restore_default_fb_state();
#endif
}

static unsigned int verify_bind_tex_fbo(lv_display_t * disp, int32_t * w_out, int32_t * h_out)
{
    unsigned int tex_id = g_last_flush_tex_id;
    if(tex_id == 0) tex_id = lv_opengles_texture_get_texture_id(disp);
    if(tex_id == 0) return 0;

    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    if(w < 16 || h < 16) return 0;

    unsigned int fbo = 0;
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0));

#if !LV_USE_EGL
    GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0));
#endif

    if(w_out) *w_out = w;
    if(h_out) *h_out = h;
    return fbo;
}

static void verify_unbind_fbo(unsigned int fbo)
{
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    if(fbo) GL_CALL(glDeleteFramebuffers(1, &fbo));
}

static void verify_read_rgba(int32_t w, int32_t h, int lv_x, int lv_y, uint8_t rgba[4])
{
    int gl_x = lv_x;
    int gl_y = h - 1 - lv_y;
    if(gl_x < 0) gl_x = 0;
    if(gl_y < 0) gl_y = 0;
    if(gl_x >= w) gl_x = w - 1;
    if(gl_y >= h) gl_y = h - 1;
    GL_CALL(glReadPixels(gl_x, gl_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba));
}

bool lv_gpu_renderer_verify_alpha(lv_display_t * disp, uint8_t * min_alpha_out, uint32_t * opaque_count_out)
{
    lv_gpu_renderer_verify_stats_t stats;
    if(!lv_gpu_renderer_verify_stats(disp, &stats)) return false;
    if(min_alpha_out) *min_alpha_out = stats.corner_min_alpha;
    if(opaque_count_out) *opaque_count_out = stats.corner_opaque_count;
    return true;
}

bool lv_gpu_renderer_verify_stats(lv_display_t * disp, lv_gpu_renderer_verify_stats_t * stats)
{
    if(!stats) return false;
    lv_memzero(stats, sizeof(*stats));

    int32_t w, h;
    unsigned int fbo = verify_bind_tex_fbo(disp, &w, &h);
    if(!fbo) return false;

    stats->corner_min_alpha = 255;
    static const int corner_lv[8][2] = {{2, 2}, {0, 2}, {-3, 2}, {2, 0}, {-3, 0}, {2, -3}, {0, -3}, {-3, -3}};
    for(int i = 0; i < 8; i++) {
        int lv_x = corner_lv[i][0] >= 0 ? corner_lv[i][0] : w + corner_lv[i][0];
        int lv_y = corner_lv[i][1] >= 0 ? corner_lv[i][1] : h + corner_lv[i][1];
        uint8_t px[4];
        verify_read_rgba(w, h, lv_x, lv_y, px);
        if(px[3] < stats->corner_min_alpha) stats->corner_min_alpha = px[3];
        if(px[3] > 128) stats->corner_opaque_count++;
    }

    verify_read_rgba(w, h, w / 2, h / 2, stats->center_rgba);
    stats->region_max_alpha = 0;

    const int grid_x = 48;
    const int grid_y = 27;
    int32_t x0 = w / 20;
    int32_t y0 = h / 20;
    int32_t x1 = w - 1 - x0;
    int32_t y1 = h - 1 - y0;

    for(int gy = 0; gy < grid_y; gy++) {
        for(int gx = 0; gx < grid_x; gx++) {
            int32_t lv_x = x0 + (int32_t)((int64_t)(x1 - x0) * gx / (grid_x - 1));
            int32_t lv_y = y0 + (int32_t)((int64_t)(y1 - y0) * gy / (grid_y - 1));
            uint8_t px[4];
            verify_read_rgba(w, h, lv_x, lv_y, px);
            stats->region_samples++;
            if(px[3] > stats->region_max_alpha) stats->region_max_alpha = px[3];
            if(px[3] >= 32) stats->region_visible_count++;
            if(px[3] >= 128) stats->region_opaque_count++;
            if(px[3] >= 32 && px[1] > px[0] + 8 && px[1] > px[2] + 8) stats->region_greenish_count++;
            /* 3D pass stores logical blue in GL R after rb_swap uniform */
            if(px[3] >= 128 && px[0] > 170 && px[1] > 90 && px[2] < 120) {
                stats->region_bluish_count++;
            }
            if(px[3] >= 128) {
                const int drg = (int)px[0] - (int)px[1];
                const int dgb = (int)px[1] - (int)px[2];
                const uint32_t avg = ((uint32_t)px[0] + px[1] + px[2]) / 3u;
                if(drg < 24 && drg > -24 && dgb < 24 && dgb > -24 && avg >= 55 && avg < 150) {
                    stats->region_grayish_count++;
                }
            }
            if(px[3] >= 128 && (uint32_t)px[0] + px[1] + px[2] >= 64) stats->region_colorful_count++;
        }
    }

    GL_CALL(glFinish());
    verify_unbind_fbo(fbo);
    stats->last_flush_items = g_last_flush_item_count;
    stats->last_flush_viewports = g_last_flush_vp_count;
    stats->flush_max_alpha = g_last_frame_max_alpha;
    stats->flush_serial = g_flush_serial;
    stats->gpu_2d_tasks = g_path_stats.gpu_2d_tasks;
    stats->gpu_3d_draws = g_path_stats.gpu_3d_draws;
    stats->sw_overlay_uploads = g_path_stats.sw_overlay_uploads;
    stats->sw_2d_raster_tasks = g_path_stats.sw_2d_raster_tasks;
    stats->fg_pass_count = g_path_stats.fg_pass_count;
    stats->fg_batch_count = g_path_stats.fg_batch_count;
    stats->fg_material_batches = g_path_stats.fg_material_batches;
    stats->fg_gl_finish_count = g_path_stats.fg_gl_finish_count;
    lv_strncpy(stats->gl_renderer, g_path_stats.gl_renderer, sizeof(stats->gl_renderer) - 1);
    stats->gl_renderer[sizeof(stats->gl_renderer) - 1] = '\0';
    if(stats->region_max_alpha < stats->flush_max_alpha) {
        stats->region_max_alpha = stats->flush_max_alpha;
    }
    return true;
}

void lv_gpu_renderer_get_path_stats(lv_gpu_renderer_path_stats_t * stats)
{
    if(!stats) return;
    stats->gpu_2d_tasks = g_path_stats.gpu_2d_tasks;
    stats->gpu_3d_draws = g_path_stats.gpu_3d_draws;
    stats->sw_overlay_uploads = g_path_stats.sw_overlay_uploads;
    stats->sw_2d_raster_tasks = g_path_stats.sw_2d_raster_tasks;
    stats->last_flush_items = g_last_flush_item_count;
    stats->last_flush_viewports = g_last_flush_vp_count;
    stats->flush_serial = g_flush_serial;
    stats->fg_pass_count = g_path_stats.fg_pass_count;
    stats->fg_batch_count = g_path_stats.fg_batch_count;
    stats->fg_material_batches = g_path_stats.fg_material_batches;
    stats->fg_gl_finish_count = g_path_stats.fg_gl_finish_count;
    lv_strncpy(stats->gl_renderer, g_path_stats.gl_renderer, sizeof(stats->gl_renderer) - 1);
    stats->gl_renderer[sizeof(stats->gl_renderer) - 1] = '\0';
}

static bool dump_rgba_lvgl_rows(int32_t w, int32_t h, const char * path)
{
    if(!path || w < 1 || h < 1) return false;

    FILE * f = fopen(path, "wb");
    if(!f) return false;

    uint32_t wh[2] = { (uint32_t)w, (uint32_t)h };
    if(fwrite(wh, 1, sizeof(wh), f) != sizeof(wh)) {
        fclose(f);
        return false;
    }

    uint8_t * row = lv_malloc((size_t)w * 4);
    if(!row) {
        fclose(f);
        return false;
    }

    uint8_t * gl_buf = lv_malloc((size_t)w * h * 4);
    if(!gl_buf) {
        lv_free(row);
        fclose(f);
        return false;
    }

    GL_CALL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, gl_buf));
    for(int32_t lv_y = 0; lv_y < h; lv_y++) {
        const int32_t gl_y = h - 1 - lv_y;
        lv_memcpy(row, gl_buf + (uint32_t)gl_y * (uint32_t)w * 4, (size_t)w * 4);
        if(fwrite(row, 1, (size_t)w * 4, f) != (size_t)w * 4) {
            lv_free(gl_buf);
            lv_free(row);
            fclose(f);
            return false;
        }
    }

    lv_free(gl_buf);
    lv_free(row);
    fclose(f);
    GL_CALL(glFinish());
    return true;
}

bool lv_gpu_renderer_dump_frame_lvgl(lv_display_t * disp, const char * path)
{
    if(!path || !disp) return false;

    int32_t w, h;
    unsigned int fbo = verify_bind_tex_fbo(disp, &w, &h);
    if(!fbo || w < 1 || h < 1) return false;

    const bool ok = dump_rgba_lvgl_rows(w, h, path);
    verify_unbind_fbo(fbo);
    return ok;
}

bool lv_gpu_renderer_dump_screen_lvgl(int32_t w, int32_t h, const char * path)
{
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
#if !LV_USE_EGL
    GL_CALL(glReadBuffer(GL_BACK));
#endif
    return dump_rgba_lvgl_rows(w, h, path);
}

void lv_gpu_renderer_restore_default_framebuffer(void)
{
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
#if !LV_USE_EGL
    {
        GLenum draw_buf = GL_BACK;
        GL_CALL(glDrawBuffers(1, &draw_buf));
        GL_CALL(glReadBuffer(GL_BACK));
    }
#endif
}

bool lv_gpu_renderer_present_tex_to_window(unsigned int tex_id, int32_t w, int32_t h)
{
    if(tex_id == 0 || w < 1 || h < 1) return false;

    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glViewport(0, 0, w, h));
    GL_CALL(glDisable(GL_SCISSOR_TEST));

#if LV_USE_EGL && LV_GPU_RENDERER_GLES_API < 3
    /* GLES2 EGL (e.g. Mali): no glBlitFramebuffer — composite with a fullscreen quad. */
    lv_area_t full = { 0, 0, w - 1, h - 1 };
    lv_opengles_reinit_state();
    lv_opengles_render_texture_rbswap(tex_id, &full, LV_OPA_COVER, w, h, &full, false, false);
    lv_gpu_renderer_restore_default_framebuffer();
    return true;
#else

    static unsigned int read_fbo;
    if(read_fbo == 0) {
        GL_CALL(glGenFramebuffers(1, &read_fbo));
    }

    /* Match verify_bind_tex_fbo: attach via GL_FRAMEBUFFER (READ_FRAMEBUFFER attach fails on some drivers). */
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, read_fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0));

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        return false;
    }

#if !LV_USE_EGL
    GL_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo));
    GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
    {
        GLenum draw_buf = GL_BACK;
        GL_CALL(glDrawBuffers(1, &draw_buf));
    }
#else
    GL_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo));
    GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
#endif

    if(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ||
       glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        return false;
    }

    GL_CALL(glDisable(GL_SCISSOR_TEST));
    GL_CALL(glDisable(GL_BLEND));
    GL_CALL(glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST));

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    lv_gpu_renderer_restore_default_framebuffer();
    return true;
#endif
}

bool lv_gpu_renderer_present_tex_readback(unsigned int tex_id, int32_t w, int32_t h)
{
    if(tex_id == 0 || w < 1 || h < 1) return false;

    unsigned int fbo = 0;
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0));

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDeleteFramebuffers(1, &fbo));
        return false;
    }

    static unsigned int sw_tex;
    static int32_t sw_w;
    static int32_t sw_h;
    if(sw_tex == 0) {
        GL_CALL(glGenTextures(1, &sw_tex));
    }

    const size_t row = (size_t)w * 4;
    const size_t buf_sz = row * (size_t)h;
    uint8_t * gl_buf = lv_malloc(buf_sz);
    uint8_t * lv_buf = lv_malloc(buf_sz);
    if(!gl_buf || !lv_buf) {
        lv_free(gl_buf);
        lv_free(lv_buf);
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDeleteFramebuffers(1, &fbo));
        return false;
    }

    GL_CALL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, gl_buf));
    for(int32_t lv_y = 0; lv_y < h; lv_y++) {
        const int32_t gl_y = h - 1 - lv_y;
        lv_memcpy(lv_buf + (size_t)lv_y * row, gl_buf + (size_t)gl_y * row, row);
    }
    lv_free(gl_buf);

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &fbo));

    GL_CALL(glBindTexture(GL_TEXTURE_2D, sw_tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    if(sw_w != w || sw_h != h) {
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, lv_buf));
        sw_w = w;
        sw_h = h;
    }
    else {
        GL_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, lv_buf));
    }
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    lv_free(lv_buf);

    lv_gpu_renderer_restore_default_framebuffer();
    lv_opengles_reinit_state();
    GL_CALL(glDisable(GL_SCISSOR_TEST));

    lv_area_t full = { 0, 0, w - 1, h - 1 };
    lv_opengles_render_texture_rbswap(sw_tex, &full, LV_OPA_COVER, w, h, &full, false, false);
    return true;
}

static int32_t gpu_renderer_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task)
{
    LV_UNUSED(draw_unit);
    lv_gpu_renderer_path_t path = LV_GPU_PATH_NONE;
    int32_t score = lv_gpu_renderer_fg_evaluate_score(task, &path);
    if(score <= 0) return 0;
    if(g_unit && g_unit->caps.max_texture_size > 0 && !g_unit->caps.has_fbo) return 0;
    if(task->preference_score > score) {
        task->preference_score = score;
        task->preferred_draw_unit_id = DRAW_UNIT_ID_GPU_RENDERER;
    }
    LV_UNUSED(path);
    return 0;
}

static int32_t gpu_renderer_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    lv_draw_gpu_renderer_unit_t * u = (lv_draw_gpu_renderer_unit_t *)draw_unit;
    if(u->task_act) return 0;

    lv_draw_task_t * t = lv_draw_get_available_task(layer, NULL, DRAW_UNIT_ID_GPU_RENDERER);
    if(t == NULL) return LV_DRAW_UNIT_IDLE;
    if(t->preferred_draw_unit_id != DRAW_UNIT_ID_GPU_RENDERER) return LV_DRAW_UNIT_IDLE;

    if(t->type == LV_DRAW_TASK_TYPE_3D) {
        lv_draw_3d_dsc_t * dsc = lv_draw_task_get_3d_dsc(t);
        if(!dsc) return LV_DRAW_UNIT_IDLE;

#if LV_USE_3D
        if(dsc->kind == LV_3D_DRAW_KIND_VIEWPORT_PASS && dsc->scene && dsc->camera) {
            if(!lv_gpu_renderer_fg_record_viewport(dsc->scene, dsc->camera, &dsc->viewport_area)) {
                return LV_DRAW_UNIT_IDLE;
            }
        }
        else {
            return LV_DRAW_UNIT_IDLE;
        }
#endif
    }
    else if(lv_gpu_renderer_fg_can_gpu_native_2d(t)) {
        if(!lv_gpu_renderer_fg_queue_2d_task(t)) return LV_DRAW_UNIT_IDLE;
        lv_gpu_renderer_fg_record_2d_task(t);
    }
    else {
        return LV_DRAW_UNIT_IDLE;
    }

    t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
    u->task_act = t;
    t->state = LV_DRAW_TASK_STATE_FINISHED;
    u->task_act = NULL;
    lv_draw_dispatch_request();
    return 1;
}

void lv_gpu_renderer_caps_probe_on_context(void)
{
    if(g_unit) {
        lv_gpu_renderer_caps_probe(&g_unit->caps);
        lv_gpu_renderer_caps_log(&g_unit->caps);
        path_stats_cache_gl_renderer();
        if(!g_gl_renderer_logged && g_path_stats.gl_renderer[0]) {
            LV_LOG_USER("LVGL GL_RENDERER: %s", g_path_stats.gl_renderer);
            g_gl_renderer_logged = true;
        }
        lv_gpu_renderer_gles2_2d_init();
#if LV_USE_3D
        lv_gpu_renderer_gles2_3d_init();
#endif
    }
}

#endif /*LV_USE_DRAW_GPU_RENDERER*/
