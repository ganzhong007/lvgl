/**
 * @file lv_draw_gpu_composite.c — LVGL draw unit + frame composite
 */

#include "lv_draw_gpu_composite.h"

#if LV_USE_DRAW_GPU_COMPOSITE

#if LV_USE_DRAW_OPENGLES || LV_USE_DRAW_NANOVG
    #error "LV_USE_DRAW_GPU_COMPOSITE cannot be combined with OPENGLES or NANOVG draw units"
#endif

#include "lv_gpu_composite_caps.h"
#include "lv_gpu_composite_gles2_3d.h"
#include "lv_gpu_composite_gles2_2d.h"
#include "../lv_draw_private.h"
#include "../../include/lvgl/draw/lv_draw_label.h"
#include "../../include/lvgl/draw/lv_draw_image.h"
#include "../../include/lvgl/draw/lv_draw_rect.h"
#include "../../core/lv_refr_private.h"
#include "../../display/lv_display_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_driver.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_texture_private.h"
#include "../../misc/lv_color.h"
#include "../../include/lvgl/draw/lv_draw_3d.h"
#include "../../3d/lv_3d_internal.h"
#include "../../widgets/3d/lv_3dviewport_private.h"
#include <stdio.h>

#define DRAW_UNIT_ID_GPU_COMPOSITE 11
#define LV_GPU_COMPOSITE_MAX_VP 8

typedef struct {
    lv_obj_t * scene;
    lv_obj_t * camera;
    lv_area_t area;
} lv_gpu_composite_vp_t;

typedef struct {
    lv_draw_unit_t base_unit;
    lv_draw_task_t * task_act;
    lv_gpu_composite_caps_t caps;
} lv_draw_gpu_composite_unit_t;

static lv_draw_gpu_composite_unit_t * g_unit;
static lv_gpu_composite_vp_t vp_queue[LV_GPU_COMPOSITE_MAX_VP];
static lv_gpu_composite_vp_t vp_persist[LV_GPU_COMPOSITE_MAX_VP];
static uint32_t vp_queue_count;
static uint32_t vp_persist_count;
static unsigned int g_last_flush_tex_id;
static lv_gpu_composite_frame_cb_t g_frame_ready_cb;
static uint32_t g_last_flush_item_count;
static uint32_t g_last_flush_vp_count;
static uint8_t g_last_frame_max_alpha;
static uint32_t g_flush_serial;

typedef struct {
    uint32_t gpu_2d_tasks;
    uint32_t gpu_3d_draws;
    uint32_t sw_overlay_uploads;
    uint32_t sw_2d_raster_tasks;
    char gl_renderer[128];
} lv_gpu_composite_path_frame_t;

static lv_gpu_composite_path_frame_t g_path_stats;
static bool g_gl_renderer_logged;

static int32_t gpu_composite_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task);
static int32_t gpu_composite_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer);
static void gpu_composite_flush_internal(unsigned int tex_id, int32_t dw, int32_t dh);

void lv_draw_gpu_composite_init(void)
{
    lv_draw_gpu_composite_unit_t * u = lv_draw_create_unit(sizeof(lv_draw_gpu_composite_unit_t));
    u->base_unit.evaluate_cb = gpu_composite_evaluate;
    u->base_unit.dispatch_cb = gpu_composite_dispatch;
    u->base_unit.name = "GPU_COMPOSITE";
    g_unit = u;
    vp_queue_count = 0;
}

void lv_draw_gpu_composite_deinit(void)
{
#if LV_USE_3D
    lv_gpu_composite_gles2_3d_deinit();
#endif
    lv_gpu_composite_gles2_2d_deinit();
    g_unit = NULL;
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

static bool gpu2d_is_display_fb_layer(const lv_layer_t * layer)
{
    lv_display_t * disp = lv_refr_get_disp_refreshing();
    if(!disp || !layer || !layer->draw_buf) return false;

    lv_opengles_texture_t * tex = lv_display_get_driver_data(disp);
    if(!tex || !tex->fb1) return false;

    return layer->draw_buf->data == tex->fb1;
}

static bool gpu2d_can_claim(lv_draw_task_t * task)
{
    if(lv_gpu_composite_gles2_2d_is_raster_nest()) return false;
    if(lv_refr_get_disp_refreshing() == NULL) return false;
    if(!gpu2d_is_display_fb_layer(task->target_layer)) return false;

    switch(task->type) {
        case LV_DRAW_TASK_TYPE_FILL: {
            lv_draw_fill_dsc_t * fd = lv_draw_task_get_fill_dsc(task);
            return fd && fd->grad.dir == LV_GRAD_DIR_NONE;
        }
        case LV_DRAW_TASK_TYPE_BORDER: {
            lv_draw_border_dsc_t * bd = lv_draw_task_get_border_dsc(task);
            return bd && bd->width > 0;
        }
        case LV_DRAW_TASK_TYPE_LABEL: {
            lv_draw_label_dsc_t * ld = lv_draw_task_get_label_dsc(task);
            return ld && ld->rotation == 0;
        }
        case LV_DRAW_TASK_TYPE_LETTER: {
            lv_draw_letter_dsc_t * ld = (lv_draw_letter_dsc_t *)task->draw_dsc;
            return ld && ld->rotation == 0;
        }
        case LV_DRAW_TASK_TYPE_IMAGE: {
            lv_draw_image_dsc_t * id = lv_draw_task_get_image_dsc(task);
            return id && id->rotation == 0 && id->scale_x == LV_SCALE_NONE && id->scale_y == LV_SCALE_NONE
                   && id->skew_x == 0 && id->skew_y == 0 && id->blend_mode == LV_BLEND_MODE_NORMAL;
        }
        default:
            return false;
    }
}

static bool gpu2d_queue_task(lv_draw_task_t * t)
{
    switch(t->type) {
        case LV_DRAW_TASK_TYPE_FILL: {
            lv_draw_fill_dsc_t * fd = lv_draw_task_get_fill_dsc(t);
            if(!fd) return false;
            return lv_gpu_composite_gles2_2d_queue_fill(&t->area, &t->clip_area, fd->color, fd->opa, fd->radius);
        }
        case LV_DRAW_TASK_TYPE_BORDER: {
            lv_draw_border_dsc_t * bd = lv_draw_task_get_border_dsc(t);
            if(!bd) return false;
            return lv_gpu_composite_gles2_2d_queue_border(&t->area, &t->clip_area, bd->color, bd->opa, bd->width,
                                                          bd->radius, bd->side);
        }
        case LV_DRAW_TASK_TYPE_LABEL: {
            lv_draw_label_dsc_t * ld = lv_draw_task_get_label_dsc(t);
            if(!ld) return false;
            return lv_gpu_composite_gles2_2d_queue_label(&t->area, &t->clip_area, ld);
        }
        case LV_DRAW_TASK_TYPE_LETTER: {
            lv_draw_letter_dsc_t * ld = (lv_draw_letter_dsc_t *)t->draw_dsc;
            if(!ld) return false;
            return lv_gpu_composite_gles2_2d_queue_letter(&t->area, &t->clip_area, ld);
        }
        case LV_DRAW_TASK_TYPE_IMAGE: {
            lv_draw_image_dsc_t * id = lv_draw_task_get_image_dsc(t);
            if(!id) return false;
            return lv_gpu_composite_gles2_2d_queue_image(&t->area, &t->clip_area, id);
        }
        default:
            return false;
    }
}

static void gpu_composite_flush_internal(unsigned int tex_id, int32_t dw, int32_t dh)
{
    if(!g_unit || tex_id == 0) return;

#if LV_USE_3D && LV_USE_SNAPSHOT
#include "../../include/lvgl/3d/lv_3d_plane_bake.h"
#endif

    if(vp_queue_count > 0) {
        vp_persist_count = vp_queue_count;
        for(uint32_t i = 0; i < vp_queue_count; i++) {
            vp_persist[i] = vp_queue[i];
        }
    }

    uint32_t count = vp_persist_count;
    uint32_t q2d = lv_gpu_composite_gles2_2d_queue_count();
    if(tex_id == 0 || (count == 0 && q2d == 0)) return;

    path_stats_cache_gl_renderer();
    g_path_stats.gpu_3d_draws = 0;
    g_path_stats.gpu_2d_tasks = 0;
    g_path_stats.sw_2d_raster_tasks = 0;
    g_path_stats.sw_overlay_uploads = 0;

    g_last_flush_tex_id = tex_id;
    g_last_flush_vp_count = count;
    g_last_flush_item_count = 0;
    g_last_frame_max_alpha = 0;

#if LV_USE_3D && LV_USE_SNAPSHOT
    lv_3d_plane_upload_all();
#endif

#if !LV_USE_EGL
    GL_CALL(glBindVertexArray(0));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    for(int ai = 0; ai < 4; ai++) {
        GL_CALL(glDisableVertexAttribArray((unsigned int)ai));
    }
#endif

    unsigned int depth_rb = 0;
    LV_UNUSED(depth_rb);
    if(count > 0) {
        for(uint32_t v = 0; v < count; v++) {
            lv_gpu_composite_vp_t * vp = &vp_persist[v];
            lv_3d_draw_item_t items[LV_3D_MAX_DRAW_ITEMS];
            uint32_t n = lv_3d_scene_collect(vp->scene, items, LV_3D_MAX_DRAW_ITEMS);

            float view[16], proj[16];
            lv_3d_camera_get_view_proj(vp->camera, dw, dh, view, proj);

            int32_t vx = vp->area.x1;
            int32_t vy = dh - vp->area.y2 - 1;
            int32_t vw = lv_area_get_width(&vp->area);
            int32_t vh = lv_area_get_height(&vp->area);

            uint8_t vp_max_a = 0;
            lv_gpu_composite_gles2_render_viewport(tex_id, 0, vx, vy, vw, vh,
                                                   view, proj, items, n,
                                                   LV_GPU_COMPOSITE_AR_PASSTHROUGH, &vp_max_a);
            if(vp_max_a > g_last_frame_max_alpha) g_last_frame_max_alpha = vp_max_a;
            g_last_flush_item_count += n;
        }
    }
    vp_queue_count = 0;

    if(q2d > 0) {
        uint32_t sw_raster = 0;
        g_path_stats.gpu_2d_tasks = lv_gpu_composite_gles2_2d_render_batch(tex_id, dw, dh, &sw_raster);
        g_path_stats.sw_2d_raster_tasks = sw_raster;
        lv_gpu_composite_gles2_2d_queue_reset();
    }

    g_path_stats.gpu_3d_draws = g_last_flush_item_count;
    GL_CALL(glFinish());
    g_flush_serial++;
}

void lv_gpu_composite_set_frame_ready_cb(lv_gpu_composite_frame_cb_t cb)
{
    g_frame_ready_cb = cb;
}

void lv_gpu_composite_notify_frame_ready(lv_display_t * disp)
{
    if(g_frame_ready_cb) g_frame_ready_cb(disp);
}

void lv_gpu_composite_flush_3d(lv_display_t * disp)
{
    int32_t dw = lv_display_get_horizontal_resolution(disp);
    int32_t dh = lv_display_get_vertical_resolution(disp);
    gpu_composite_flush_internal(lv_opengles_texture_get_texture_id(disp), dw, dh);
}

void lv_gpu_composite_flush_3d_to_tex(lv_display_t * disp, unsigned int tex_id, int32_t w, int32_t h)
{
    LV_UNUSED(disp);
    gpu_composite_flush_internal(tex_id, w, h);
}

void lv_gpu_composite_overlay_2d_fb(lv_display_t * disp)
{
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    lv_gpu_composite_overlay_2d_to_tex(disp, 0, w, h);
}

#if LV_COLOR_DEPTH == 32
static bool overlay_fb_has_visible_2d(const uint8_t * px, uint32_t stride, int32_t w, int32_t h, lv_color_format_t cf)
{
    for(int32_t y = 0; y < h; y += 32) {
        for(int32_t x = 0; x < w; x += 32) {
            const uint8_t * p = px + (uint32_t)y * stride + (uint32_t)x * 4;
            if(lv_color_format_has_alpha(cf)) {
                if(p[3] > 16) return true;
            }
            else if((p[0] | p[1] | p[2]) > 16) {
                return true;
            }
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
    GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / lv_color_format_get_size(cf)));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture->fb1));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));

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

void lv_gpu_composite_overlay_2d_screen(lv_display_t * disp, int32_t w, int32_t h)
{
#if LV_COLOR_DEPTH != 32
    LV_UNUSED(disp);
    LV_UNUSED(w);
    LV_UNUSED(h);
    return;
#else
    if(g_path_stats.gpu_2d_tasks > 0) return;

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

void lv_gpu_composite_overlay_2d_to_tex(lv_display_t * disp, unsigned int tex_id, int32_t w, int32_t h)
{
    lv_opengles_texture_t * texture = lv_display_get_driver_data(disp);
    if(!texture || !texture->fb1) return;

    if(tex_id == 0) tex_id = texture->texture_id;
    if(tex_id == 0) return;

    if(w < 1 || h < 1) return;

#if LV_COLOR_DEPTH != 32
    return;
#else
    /* M1: skip full-screen SW upload when 2D overlay was composited on GPU */
    if(g_path_stats.gpu_2d_tasks > 0) return;

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

bool lv_gpu_composite_verify_alpha(lv_display_t * disp, uint8_t * min_alpha_out, uint32_t * opaque_count_out)
{
    lv_gpu_composite_verify_stats_t stats;
    if(!lv_gpu_composite_verify_stats(disp, &stats)) return false;
    if(min_alpha_out) *min_alpha_out = stats.corner_min_alpha;
    if(opaque_count_out) *opaque_count_out = stats.corner_opaque_count;
    return true;
}

bool lv_gpu_composite_verify_stats(lv_display_t * disp, lv_gpu_composite_verify_stats_t * stats)
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
    lv_strncpy(stats->gl_renderer, g_path_stats.gl_renderer, sizeof(stats->gl_renderer) - 1);
    stats->gl_renderer[sizeof(stats->gl_renderer) - 1] = '\0';
    if(stats->region_max_alpha < stats->flush_max_alpha) {
        stats->region_max_alpha = stats->flush_max_alpha;
    }
    return true;
}

void lv_gpu_composite_get_path_stats(lv_gpu_composite_path_stats_t * stats)
{
    if(!stats) return;
    stats->gpu_2d_tasks = g_path_stats.gpu_2d_tasks;
    stats->gpu_3d_draws = g_path_stats.gpu_3d_draws;
    stats->sw_overlay_uploads = g_path_stats.sw_overlay_uploads;
    stats->sw_2d_raster_tasks = g_path_stats.sw_2d_raster_tasks;
    stats->last_flush_items = g_last_flush_item_count;
    stats->last_flush_viewports = g_last_flush_vp_count;
    stats->flush_serial = g_flush_serial;
    lv_strncpy(stats->gl_renderer, g_path_stats.gl_renderer, sizeof(stats->gl_renderer) - 1);
    stats->gl_renderer[sizeof(stats->gl_renderer) - 1] = '\0';
}

bool lv_gpu_composite_dump_frame_lvgl(lv_display_t * disp, const char * path)
{
    if(!path || !disp) return false;

    int32_t w, h;
    unsigned int fbo = verify_bind_tex_fbo(disp, &w, &h);
    if(!fbo || w < 1 || h < 1) return false;

    FILE * f = fopen(path, "wb");
    if(!f) {
        verify_unbind_fbo(fbo);
        return false;
    }

    uint32_t wh[2] = { (uint32_t)w, (uint32_t)h };
    if(fwrite(wh, 1, sizeof(wh), f) != sizeof(wh)) {
        fclose(f);
        verify_unbind_fbo(fbo);
        return false;
    }

    uint8_t * row = lv_malloc((size_t)w * 4);
    if(!row) {
        fclose(f);
        verify_unbind_fbo(fbo);
        return false;
    }

    uint8_t * gl_buf = lv_malloc((size_t)w * h * 4);
    if(!gl_buf) {
        lv_free(row);
        fclose(f);
        verify_unbind_fbo(fbo);
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
            verify_unbind_fbo(fbo);
            return false;
        }
    }

    lv_free(gl_buf);

    lv_free(row);
    fclose(f);
    GL_CALL(glFinish());
    verify_unbind_fbo(fbo);
    return true;
}

static int32_t gpu_composite_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task)
{
    LV_UNUSED(draw_unit);
    if(lv_refr_get_disp_refreshing() == NULL) return 0;

    if(task->type == LV_DRAW_TASK_TYPE_3D) {
        task->preference_score = 0;
        task->preferred_draw_unit_id = DRAW_UNIT_ID_GPU_COMPOSITE;
    }
    else if(gpu2d_can_claim(task)) {
        task->preference_score = 0;
        task->preferred_draw_unit_id = DRAW_UNIT_ID_GPU_COMPOSITE;
    }
    return 0;
}

static int32_t gpu_composite_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    lv_draw_gpu_composite_unit_t * u = (lv_draw_gpu_composite_unit_t *)draw_unit;
    if(u->task_act) return 0;

    lv_draw_task_t * t = lv_draw_get_available_task(layer, NULL, DRAW_UNIT_ID_GPU_COMPOSITE);
    if(t == NULL) return LV_DRAW_UNIT_IDLE;
    if(t->preferred_draw_unit_id != DRAW_UNIT_ID_GPU_COMPOSITE) return LV_DRAW_UNIT_IDLE;

    if(t->type == LV_DRAW_TASK_TYPE_3D) {
        lv_draw_3d_dsc_t * dsc = lv_draw_task_get_3d_dsc(t);
        if(!dsc) return LV_DRAW_UNIT_IDLE;

#if LV_USE_3D
        if(dsc->kind == LV_3D_DRAW_KIND_VIEWPORT_PASS && dsc->scene && dsc->camera) {
            if(vp_queue_count < LV_GPU_COMPOSITE_MAX_VP) {
                bool dup = false;
                for(uint32_t qi = 0; qi < vp_queue_count; qi++) {
                    lv_gpu_composite_vp_t * q = &vp_queue[qi];
                    if(q->scene == dsc->scene && q->camera == dsc->camera
                       && q->area.x1 == dsc->viewport_area.x1 && q->area.y1 == dsc->viewport_area.y1
                       && q->area.x2 == dsc->viewport_area.x2 && q->area.y2 == dsc->viewport_area.y2) {
                        dup = true;
                        break;
                    }
                }
                if(!dup) {
                    vp_queue[vp_queue_count].scene = dsc->scene;
                    vp_queue[vp_queue_count].camera = dsc->camera;
                    vp_queue[vp_queue_count].area = dsc->viewport_area;
                    vp_queue_count++;
                }
            }
        }
#endif
    }
    else if(gpu2d_can_claim(t)) {
        if(!gpu2d_queue_task(t)) return LV_DRAW_UNIT_IDLE;
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

void lv_gpu_composite_caps_probe_on_context(void)
{
    if(g_unit) {
        lv_gpu_composite_caps_probe(&g_unit->caps);
        lv_gpu_composite_caps_log(&g_unit->caps);
        path_stats_cache_gl_renderer();
        if(!g_gl_renderer_logged && g_path_stats.gl_renderer[0]) {
            LV_LOG_USER("LVGL GL_RENDERER: %s", g_path_stats.gl_renderer);
            g_gl_renderer_logged = true;
        }
        lv_gpu_composite_gles2_2d_init();
#if LV_USE_3D
        lv_gpu_composite_gles2_3d_init();
#endif
    }
}

#endif /*LV_USE_DRAW_GPU_COMPOSITE*/
