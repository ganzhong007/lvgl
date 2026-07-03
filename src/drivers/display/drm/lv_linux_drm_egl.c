/**
 * @file lv_linux_drm_egl.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../../../lvgl_public.h"

#if LV_USE_LINUX_DRM && LV_LINUX_DRM_USE_EGL

#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <xf86drmMode.h>
#include <stdlib.h>
#include LV_STDINT_INCLUDE
#include <gbm.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <time.h>
#include <unistd.h>
#include "lv_linux_drm_egl_private.h"
#include "../../opengles/lv_opengles_debug.h"
#include "../../opengles/lv_opengles_private.h"
#if LV_USE_DRAW_GPU_RENDERER
#include "../../../draw/gpu_renderer/lv_draw_gpu_renderer.h"
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    int fd;
    struct gbm_bo * bo;
    uint32_t fb_id;
} drm_fb_state_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static uint32_t tick_cb(void);
static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
static void event_cb(lv_event_t * e);

static lv_result_t drm_device_init(lv_drm_ctx_t * ctx, const char * path);
static void drm_device_deinit(lv_drm_ctx_t * ctx);

static lv_egl_interface_t drm_get_egl_interface(lv_drm_ctx_t * ctx);
static drmModeConnector * drm_get_connector(lv_drm_ctx_t * ctx);
static drmModeModeInfo * drm_get_mode(lv_drm_ctx_t * ctx);
static drmModeEncoder * drm_get_encoder(lv_drm_ctx_t * ctx);
static drmModeCrtc * drm_get_crtc(lv_drm_ctx_t * ctx);
static void drm_on_page_flip(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void * data);
static int drm_do_page_flip(lv_drm_ctx_t * ctx, int timeout_ms);
static void drm_egl_scanout_deinit(lv_drm_ctx_t * ctx);
static void drm_egl_capture_scanout_from_gl(lv_drm_ctx_t * ctx, lv_display_t * disp, int32_t w, int32_t h);
static lv_result_t drm_egl_fill_scanout_solid_cpu(lv_drm_ctx_t * ctx, int32_t w, int32_t h,
                                                   uint8_t r, uint8_t g, uint8_t b);
static lv_result_t drm_ensure_scanout_bo(lv_drm_ctx_t * ctx, uint32_t w, uint32_t h);
static bool drm_plane_is_32bit(uint32_t plane_fourcc);
static void drm_copy_gbm_bo_to_scanout(lv_drm_ctx_t * ctx, struct gbm_bo * src);
static drm_fb_state_t * drm_fb_state_for_present(lv_drm_ctx_t * ctx, struct gbm_bo * bo,
                                                 drm_fb_state_t * out);
static void drm_fb_state_destroy_cb(struct gbm_bo * bo, void * data);
static void drm_egl_atomic_deinit(lv_drm_ctx_t * ctx);
static lv_result_t drm_egl_atomic_init(lv_drm_ctx_t * ctx);
static int drm_egl_atomic_present(lv_drm_ctx_t * ctx, drm_fb_state_t * pending_fb);
static void drm_flip_cb(void * driver_data, bool vsync);
static void drm_egl_present_scanout_gl(lv_drm_ctx_t * ctx);

static void * drm_create_window(void * driver_data, const lv_egl_native_window_properties_t * properties);
static void drm_destroy_window(void * driver_data, void * native_window);
static size_t drm_egl_select_config_cb(void * driver_data, const lv_egl_config_t * configs, size_t config_count);
static inline void set_viewport(lv_display_t * display);

static bool drm_egl_use_texture_scanout(const lv_drm_ctx_t * ctx)
{
    LV_UNUSED(ctx);
    /* glReadPixels from FBO textures is unreliable on Mali-400 GLES2 (reads as 0xFF). */
    return false;
}

static bool drm_debug_solid_scanout(void)
{
    static int cached = -1;
    if(cached < 0) {
        const char * env = getenv("LVGL_DRM_SOLID_SCANOUT");
        cached = (env && env[0] == '1') ? 1 : 0;
        if(cached) {
            LV_LOG_USER("DRM debug: CPU solid scanout (LVGL_DRM_SOLID_SCANOUT=1)");
        }
    }
    return cached != 0;
}

static lv_result_t drm_egl_fill_scanout_solid_cpu(lv_drm_ctx_t * ctx, int32_t w, int32_t h,
                                                   uint8_t r, uint8_t g, uint8_t b)
{
    if(w < 1 || h < 1) {
        return LV_RESULT_INVALID;
    }
    if(drm_ensure_scanout_bo(ctx, (uint32_t)w, (uint32_t)h) != LV_RESULT_OK) {
        return LV_RESULT_INVALID;
    }

    uint32_t stride = 0;
    void * map_data = NULL;
    void * ptr = gbm_bo_map(ctx->scanout_bo, 0, 0, (uint32_t)w, (uint32_t)h,
                            GBM_BO_TRANSFER_WRITE, &stride, &map_data);
    if(!ptr) {
        LV_LOG_ERROR("CPU solid scanout: gbm_bo_map failed");
        return LV_RESULT_INVALID;
    }

    const bool plane32 = drm_plane_is_32bit(ctx->plane_fourcc);
    for(int32_t y = 0; y < h; y++) {
        uint8_t * row = (uint8_t *)ptr + (uint32_t)y * stride;
        for(int32_t x = 0; x < w; x++) {
            if(plane32) {
                row[0] = b;
                row[1] = g;
                row[2] = r;
                row[3] = 0xFF;
                row += 4;
            }
            else if(ctx->plane_fourcc == DRM_FORMAT_BGR888) {
                row[0] = b;
                row[1] = g;
                row[2] = r;
                row += 3;
            }
            else {
                row[0] = r;
                row[1] = g;
                row[2] = b;
                row += 3;
            }
        }
    }

    gbm_bo_unmap(ctx->scanout_bo, map_data);
    ctx->scanout_from_gl = true;
    LV_LOG_USER("CPU solid scanout R=%u G=%u B=%u plane %#x fb %u", r, g, b, ctx->plane_fourcc,
                ctx->scanout_fb_id);
    return LV_RESULT_OK;
}

static void drm_egl_composite_to_window(lv_display_t * disp)
{
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    set_viewport(disp);
    lv_gpu_renderer_restore_default_framebuffer();
    unsigned int tex_id = lv_opengles_texture_get_texture_id(disp);
    if(!lv_gpu_renderer_present_tex_to_window(tex_id, w, h)) {
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        set_viewport(disp);
        lv_opengles_render_params_t params = {
            .h_flip = false,
            .v_flip = false,
            .rb_swap = true,
        };
        lv_opengles_render_display(disp, &params);
    }
    GL_CALL(glFinish());
}

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_display_t * lv_linux_drm_create(void)
{
    lv_tick_set_cb(tick_cb);
    lv_drm_ctx_t * ctx = lv_zalloc(sizeof(*ctx));
    LV_ASSERT_MALLOC(ctx);
    if(!ctx) {
        LV_LOG_ERROR("Failed to create drm context");
        return NULL;
    }

    ctx->display = lv_display_create(1, 1);

    if(!ctx->display) {
        LV_LOG_ERROR("Failed to create display");
        lv_free(ctx);
        return NULL;
    }

    lv_display_set_driver_data(ctx->display, ctx);
    lv_display_add_event_cb(ctx->display, event_cb, LV_EVENT_DELETE, NULL);
    return ctx->display;
}

lv_result_t lv_linux_drm_set_file(lv_display_t * display, const char * file, int64_t connector_id)
{
    LV_UNUSED(connector_id);
    lv_drm_ctx_t * ctx = lv_display_get_driver_data(display);

    lv_result_t err = drm_device_init(ctx, file);
    if(err != LV_RESULT_OK) {
        LV_LOG_ERROR("Failed to initialize DRM device");
        return LV_RESULT_INVALID;
    }

    lv_display_set_resolution(display, ctx->drm_mode->hdisplay, ctx->drm_mode->vdisplay);

    ctx->egl_interface = drm_get_egl_interface(ctx);
    ctx->egl_ctx = lv_opengles_egl_context_create(&ctx->egl_interface);
    if(!ctx->egl_ctx) {
        LV_LOG_ERROR("Failed to create egl context");
        return LV_RESULT_INVALID;
    }

    /* Let the opengles texture driver handle the texture lifetime */
    ctx->texture.is_texture_owner = true;
    /*Initialize the draw buffers and texture*/
    lv_result_t res = lv_opengles_texture_reshape(&ctx->texture, display, ctx->drm_mode->hdisplay, ctx->drm_mode->vdisplay);
    if(res != LV_RESULT_OK) {
        LV_LOG_ERROR("Failed to create draw buffers");
        lv_opengles_egl_context_destroy(ctx->egl_ctx);
        ctx->egl_ctx = NULL;
        return LV_RESULT_INVALID;
    }

    lv_display_set_flush_cb(display, flush_cb);
    lv_display_set_render_mode(display, LV_USE_DRAW_NANOVG ? LV_DISPLAY_RENDER_MODE_FULL : LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_display_add_event_cb(ctx->display, event_cb, LV_EVENT_RESOLUTION_CHANGED, NULL);
    lv_display_add_event_cb(ctx->display, event_cb, LV_EVENT_DELETE, NULL);

    return LV_RESULT_OK;
}

void lv_linux_drm_set_mode_cb(lv_display_t * disp, lv_linux_drm_select_mode_cb_t callback)
{
    if(!disp) {
        LV_LOG_ERROR("Cannot set a mode select callback on a NULL display");
        return;
    }
    lv_drm_ctx_t * ctx = lv_display_get_driver_data(disp);
    ctx->mode_select_cb = callback;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_display_t * display = (lv_display_t *) lv_event_get_target(e);
    lv_drm_ctx_t * ctx = lv_display_get_driver_data(display);
    switch(code) {
        case LV_EVENT_DELETE:
            if(ctx) {
                lv_opengles_egl_context_destroy(ctx->egl_ctx);
                ctx->egl_ctx = NULL;
                lv_opengles_texture_deinit(&ctx->texture);
                drm_device_deinit(ctx);
                lv_display_set_driver_data(display, NULL);
            }
            break;
        case LV_EVENT_RESOLUTION_CHANGED: {
                lv_result_t res = lv_opengles_texture_reshape(&ctx->texture, display, lv_display_get_horizontal_resolution(display),
                                                              lv_display_get_vertical_resolution(display));

                if(res != LV_RESULT_OK) {
                    LV_LOG_ERROR("Failed to resize display");
                }
            }
            break;
        default:
            return;
    }
}

static uint32_t tick_cb(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);;
}

static inline void set_viewport(lv_display_t * display)
{
    const lv_display_rotation_t rotation = lv_display_get_rotation(display);
    int32_t disp_width, disp_height;
    if(rotation == LV_DISPLAY_ROTATION_0 || rotation == LV_DISPLAY_ROTATION_180) {
        disp_width = lv_display_get_horizontal_resolution(display);
        disp_height = lv_display_get_vertical_resolution(display);
    }
    else {
        disp_width = lv_display_get_vertical_resolution(display) ;
        disp_height = lv_display_get_horizontal_resolution(display) ;
    }
    lv_opengles_viewport(0, 0, disp_width, disp_height);
}

#if LV_USE_DRAW_OPENGLES || LV_USE_DRAW_NANOVG

static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    LV_UNUSED(area);
    LV_UNUSED(px_map);
    if(lv_display_flush_is_last(disp)) {
        set_viewport(disp);
        lv_drm_ctx_t * ctx = lv_display_get_driver_data(disp);
#if LV_USE_DRAW_OPENGLES
        lv_opengles_render_display_texture(disp, false, true);
#endif /*LV_USE_DRAW_OPENGLES*/
        lv_opengles_egl_update(ctx->egl_ctx);
    }
    lv_display_flush_ready(disp);
}

#elif LV_USE_DRAW_GPU_RENDERER

/* GL → scanout BO capture path: present plane FB directly (eglSwapBuffers flip_cb
 * bails on frame 2+ while atomic_req is still pending). */
static void drm_egl_present_scanout_gl(lv_drm_ctx_t * ctx)
{
    if(!ctx->use_atomic || !ctx->scanout_fb_id) {
        lv_opengles_egl_update(ctx->egl_ctx);
        return;
    }

    for(int i = 0; i < 40 && ctx->atomic_req; i++) {
        if(drm_do_page_flip(ctx, 10) <= 0) break;
    }

    drm_fb_state_t fb = {
        .fd = ctx->fd,
        .bo = NULL,
        .fb_id = ctx->scanout_fb_id,
    };

    int status = drm_egl_atomic_present(ctx, &fb);
    if(status < 0) {
        LV_LOG_ERROR("Scanout atomic present failed: %d", status);
        return;
    }

    drm_do_page_flip(ctx, 0);

    static uint32_t frame;
    frame++;
    if(frame == 2 || (frame % 120U) == 0U) {
        LV_LOG_USER("Scanout present frame %u fb %u", (unsigned)frame, (unsigned)ctx->scanout_fb_id);
    }
}

void lv_linux_drm_gpu_present(lv_display_t * disp)
{
    if(!disp) return;

    lv_drm_ctx_t * ctx = lv_display_get_driver_data(disp);
    if(!ctx || !ctx->egl_ctx) return;

    const int32_t w = lv_display_get_horizontal_resolution(disp);
    const int32_t h = lv_display_get_vertical_resolution(disp);
    set_viewport(disp);
    if(drm_debug_solid_scanout()) {
        drm_egl_fill_scanout_solid_cpu(ctx, w, h, 255, 0, 255);
        drm_egl_present_scanout_gl(ctx);
    }
    else if(lv_gpu_renderer_debug_2d_only()) {
        lv_gpu_renderer_flush_2d_only(disp);
        drm_egl_capture_scanout_from_gl(ctx, disp, w, h);
        drm_egl_present_scanout_gl(ctx);
    }
    else {
        lv_gpu_renderer_flush_3d(disp);
        lv_gpu_renderer_notify_frame_ready(disp);
        lv_gpu_renderer_overlay_2d_fb(disp);
        drm_egl_capture_scanout_from_gl(ctx, disp, w, h);
        drm_egl_present_scanout_gl(ctx);
    }
}

static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    LV_UNUSED(area);
    LV_UNUSED(px_map);
    /* GPU composite + KMS present run from run_loop via lv_linux_drm_gpu_present(). */
    lv_display_flush_ready(disp);
}

#else

static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    LV_UNUSED(px_map);
    LV_UNUSED(area);
    if(lv_display_flush_is_last(disp)) {
        lv_drm_ctx_t * ctx = lv_display_get_driver_data(disp);
        int32_t disp_width = lv_display_get_horizontal_resolution(disp);
        int32_t disp_height = lv_display_get_vertical_resolution(disp);

        set_viewport(disp);

        lv_color_format_t cf = lv_display_get_color_format(disp);
        uint32_t stride = lv_draw_buf_width_to_stride(lv_display_get_horizontal_resolution(disp), cf);
        GL_CALL(glBindTexture(GL_TEXTURE_2D, ctx->texture.texture_id));

        GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
        GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / lv_color_format_get_size(cf)));
        /*Color depth: 16 (RGB565), 32 (ARGB8888)*/
#if LV_COLOR_DEPTH == 16
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, disp_width, disp_height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                             ctx->texture.fb1));
#elif LV_COLOR_DEPTH == 32
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, disp_width, disp_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             ctx->texture.fb1));
#else
#error("Unsupported color format")
#endif

        lv_opengles_render_params_t params = {
            .h_flip = false,
            .v_flip = false,
            .rb_swap = LV_COLOR_DEPTH == 32,
        };
        lv_opengles_render_display(disp, &params);
        lv_opengles_egl_update(ctx->egl_ctx);
    }
    lv_display_flush_ready(disp);
}
#endif

void drm_device_deinit(lv_drm_ctx_t * ctx)
{
    drm_egl_scanout_deinit(ctx);
    drm_egl_atomic_deinit(ctx);

    if(ctx->drm_crtc) {
        drmModeSetCrtc(ctx->fd,
                       ctx->drm_crtc->crtc_id,
                       ctx->drm_crtc->buffer_id,
                       ctx->drm_crtc->x,
                       ctx->drm_crtc->y,
                       &ctx->drm_connector->connector_id,
                       1,
                       &ctx->drm_crtc->mode);
        drmModeFreeCrtc(ctx->drm_crtc);
        ctx->drm_crtc = 0;
    }
    drm_destroy_window(ctx, ctx->gbm_surface);

    if(ctx->gbm_dev) {
        gbm_device_destroy(ctx->gbm_dev);
        ctx->gbm_dev = NULL;
    }
    if(ctx->drm_connector) {
        drmModeFreeConnector(ctx->drm_connector);
        ctx->drm_connector = NULL;
    }
    if(ctx->drm_encoder) {
        drmModeFreeEncoder(ctx->drm_encoder);
        ctx->drm_encoder = NULL;
    }
    if(ctx->drm_resources) {
        drmModeFreeResources(ctx->drm_resources);
        ctx->drm_resources = NULL;
    }
    if(ctx->fd > 0) {
        drmClose(ctx->fd);
    }
    ctx->fd = 0;
    ctx->drm_mode = NULL;
    lv_free(ctx);
}

static void drm_fb_state_destroy_cb(struct gbm_bo * bo, void * data)
{
    LV_UNUSED(bo);
    drm_fb_state_t * fb = (drm_fb_state_t *) data;
    if(fb && fb->fb_id) {
        drmModeRmFB(fb->fd, fb->fb_id);
    }
    lv_free(fb);
}

static int drm_do_page_flip(lv_drm_ctx_t * ctx, int timeout_ms)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(ctx->fd, &fds);

    drmEventContext event_ctx;
    lv_memset(&event_ctx, 0, sizeof(event_ctx));
    event_ctx.version = 2;
    event_ctx.page_flip_handler = drm_on_page_flip;

    struct timeval timeout;
    int status;
    if(timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        status = select(ctx->fd + 1, &fds, NULL, NULL, &timeout);
    }
    else {
        status = select(ctx->fd + 1, &fds, NULL, NULL, NULL);
    }

    if(status == 1) {
        drmHandleEvent(ctx->fd, &event_ctx);
    }
    return status;
}

static void drm_on_page_flip(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void * data)
{
    LV_UNUSED(fd);
    LV_UNUSED(frame);
    LV_UNUSED(sec);
    LV_UNUSED(usec);
    lv_drm_ctx_t * ctx = (lv_drm_ctx_t *) data;

    if(ctx->atomic_req) {
        drmModeAtomicFree(ctx->atomic_req);
        ctx->atomic_req = NULL;
    }

    if(ctx->gbm_bo_presented) {
        gbm_surface_release_buffer(ctx->gbm_surface, ctx->gbm_bo_presented);
    }
    ctx->gbm_bo_presented = ctx->gbm_bo_flipped;
    ctx->gbm_bo_flipped = NULL;
}

static void drm_egl_scanout_deinit(lv_drm_ctx_t * ctx)
{
    if(!ctx) {
        return;
    }
    if(ctx->scanout_fb_id) {
        drmModeRmFB(ctx->fd, ctx->scanout_fb_id);
        ctx->scanout_fb_id = 0;
    }
    if(ctx->scanout_bo) {
        gbm_bo_destroy(ctx->scanout_bo);
        ctx->scanout_bo = NULL;
    }
    ctx->scanout_w = 0;
    ctx->scanout_h = 0;
    ctx->scanout_from_gl = false;
}

static void rgba_row_to_argb8888_le(uint8_t * dst, const uint8_t * src, uint32_t w, uint32_t plane_fourcc)
{
    for(uint32_t x = 0; x < w; x++) {
        const uint8_t r = src[0];
        const uint8_t g = src[1];
        const uint8_t b = src[2];
        const uint8_t a = src[3];

        if(plane_fourcc == DRM_FORMAT_ABGR8888 || plane_fourcc == DRM_FORMAT_XBGR8888) {
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
            dst[3] = (plane_fourcc == DRM_FORMAT_XBGR8888) ? 0xFF : a;
        }
        else {
            /* DRM AR24/XB24 LE memory: B,G,R,A */
            dst[0] = b;
            dst[1] = g;
            dst[2] = r;
            dst[3] = (plane_fourcc == DRM_FORMAT_XRGB8888) ? 0xFF : a;
        }
        src += 4;
        dst += 4;
    }
}

static bool drm_plane_is_32bit(uint32_t plane_fourcc)
{
    return plane_fourcc == DRM_FORMAT_ARGB8888 || plane_fourcc == DRM_FORMAT_XRGB8888
           || plane_fourcc == DRM_FORMAT_ABGR8888 || plane_fourcc == DRM_FORMAT_XBGR8888;
}

static void rgba_row_to_rgb888(uint8_t * dst, const uint8_t * src, uint32_t w, uint32_t plane_fourcc)
{
    for(uint32_t x = 0; x < w; x++) {
        uint8_t r = src[0];
        uint8_t g = src[1];
        uint8_t b = src[2];
        const uint8_t a = src[3];

        if(a == 0) {
            r = g = b = 0;
        }
        else if(a < 255) {
            r = (uint8_t)(((uint16_t)r * 255 + (a / 2)) / a);
            g = (uint8_t)(((uint16_t)g * 255 + (a / 2)) / a);
            b = (uint8_t)(((uint16_t)b * 255 + (a / 2)) / a);
        }

        if(plane_fourcc == DRM_FORMAT_BGR888) {
            dst[0] = b;
            dst[1] = g;
            dst[2] = r;
        }
        else {
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
        }
        src += 4;
        dst += 3;
    }
}

static void drm_egl_capture_scanout_from_gl(lv_drm_ctx_t * ctx, lv_display_t * disp, int32_t w, int32_t h)
{
    LV_UNUSED(disp);
    if(w < 1 || h < 1 || w > 4096) {
        return;
    }

    const unsigned int tex_id = lv_opengles_texture_get_texture_id(disp);
    if(tex_id == 0) {
        LV_LOG_WARN("Scanout capture skipped: display texture not ready");
        return;
    }

    if(drm_ensure_scanout_bo(ctx, (uint32_t)w, (uint32_t)h) != LV_RESULT_OK) {
        return;
    }

    uint32_t dst_stride = 0;
    void * dst_map_data = NULL;
    void * dst_ptr = gbm_bo_map(ctx->scanout_bo, 0, 0, (uint32_t)w, (uint32_t)h,
                                GBM_BO_TRANSFER_WRITE, &dst_stride, &dst_map_data);
    if(!dst_ptr) {
        LV_LOG_ERROR("scanout map for GL capture failed");
        return;
    }

    unsigned int read_fbo = 0;
    GL_CALL(glGenFramebuffers(1, &read_fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, read_fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0));
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LV_LOG_ERROR("Scanout read FBO incomplete for tex %u", tex_id);
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDeleteFramebuffers(1, &read_fbo));
        gbm_bo_unmap(ctx->scanout_bo, dst_map_data);
        return;
    }

    GL_CALL(glFinish());

    const bool plane32 = drm_plane_is_32bit(ctx->plane_fourcc);
    const size_t rgba_bytes = (size_t)w * (size_t)h * 4U;
    uint8_t * full_rgba = (uint8_t *)lv_malloc(rgba_bytes);
    if(!full_rgba) {
        LV_LOG_ERROR("scanout capture: OOM %zu bytes", rgba_bytes);
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDeleteFramebuffers(1, &read_fbo));
        gbm_bo_unmap(ctx->scanout_bo, dst_map_data);
        return;
    }

    GL_CALL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, full_rgba));

    static bool logged;
    if(!logged) {
        const size_t mid = (size_t)(h / 2) * (size_t)w * 4U + (size_t)(w / 2) * 4U;
        LV_LOG_USER("Scanout tex %u → plane %#x (center RGBA %u %u %u %u)",
                    tex_id, ctx->plane_fourcc,
                    full_rgba[mid + 0], full_rgba[mid + 1], full_rgba[mid + 2], full_rgba[mid + 3]);
        logged = true;
    }

    for(int32_t y = 0; y < h; y++) {
        /* Mali FBO readPixels row 0 = top; no Y flip (flip caused upside-down on DP). */
        const uint8_t * src = full_rgba + (size_t)y * (size_t)w * 4U;
        uint8_t * row = (uint8_t *)dst_ptr + (uint32_t)y * dst_stride;
        if(plane32) {
            rgba_row_to_argb8888_le(row, src, (uint32_t)w, ctx->plane_fourcc);
        }
        else {
            rgba_row_to_rgb888(row, src, (uint32_t)w, ctx->plane_fourcc);
        }
    }

    lv_free(full_rgba);

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &read_fbo));
    gbm_bo_unmap(ctx->scanout_bo, dst_map_data);
    ctx->scanout_from_gl = true;
}

static lv_result_t drm_ensure_scanout_bo(lv_drm_ctx_t * ctx, uint32_t w, uint32_t h)
{
    if(ctx->scanout_bo && ctx->scanout_w == w && ctx->scanout_h == h && ctx->scanout_fb_id) {
        return LV_RESULT_OK;
    }

    drm_egl_scanout_deinit(ctx);

    ctx->scanout_bo = gbm_bo_create(ctx->gbm_dev, w, h, ctx->plane_fourcc, GBM_BO_USE_SCANOUT);
    if(!ctx->scanout_bo) {
        LV_LOG_ERROR("Failed to create RGB888 scanout BO (%ux%u fmt %#x)", w, h, ctx->plane_fourcc);
        return LV_RESULT_INVALID;
    }

    uint32_t handles[4] = {0};
    uint32_t strides[4] = {0};
    uint32_t offsets[4] = {0};
    handles[0] = gbm_bo_get_handle(ctx->scanout_bo).u32;
    strides[0] = gbm_bo_get_stride(ctx->scanout_bo);

    if(drmModeAddFB2(ctx->fd, w, h, ctx->plane_fourcc, handles, strides, offsets,
                     &ctx->scanout_fb_id, 0) < 0) {
        LV_LOG_ERROR("Failed to AddFB2 scanout BO fmt %#x", ctx->plane_fourcc);
        drm_egl_scanout_deinit(ctx);
        return LV_RESULT_INVALID;
    }

    ctx->scanout_w = w;
    ctx->scanout_h = h;
    LV_LOG_USER("Scanout BO %#x %ux%u fb %u", ctx->plane_fourcc, w, h, ctx->scanout_fb_id);
    return LV_RESULT_OK;
}

static void drm_copy_gbm_bo_to_scanout(lv_drm_ctx_t * ctx, struct gbm_bo * src)
{
    const uint32_t w = gbm_bo_get_width(src);
    const uint32_t h = gbm_bo_get_height(src);

    uint32_t src_stride = 0;
    void * src_map_data = NULL;
    void * src_ptr = gbm_bo_map(src, 0, 0, w, h, GBM_BO_TRANSFER_READ, &src_stride, &src_map_data);
    if(!src_ptr) {
        LV_LOG_ERROR("gbm_bo_map src failed");
        return;
    }

    uint32_t dst_stride = 0;
    void * dst_map_data = NULL;
    void * dst_ptr = gbm_bo_map(ctx->scanout_bo, 0, 0, w, h, GBM_BO_TRANSFER_WRITE, &dst_stride, &dst_map_data);
    if(!dst_ptr) {
        LV_LOG_ERROR("gbm_bo_map scanout failed");
        gbm_bo_unmap(src, src_map_data);
        return;
    }

    for(uint32_t y = 0; y < h; y++) {
        const uint8_t * s = (const uint8_t *)src_ptr + y * src_stride;
        uint8_t * d = (uint8_t *)dst_ptr + y * dst_stride;
        for(uint32_t x = 0; x < w; x++) {
            /* DRM ARGB8888 LE memory: B,G,R,A */
            if(ctx->plane_fourcc == DRM_FORMAT_BGR888) {
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
            }
            else {
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
            }
            s += 4;
            d += 3;
        }
    }

    gbm_bo_unmap(src, src_map_data);
    gbm_bo_unmap(ctx->scanout_bo, dst_map_data);
}

static drm_fb_state_t * drm_fb_state_create(lv_drm_ctx_t * ctx, struct gbm_bo * bo)
{
    LV_ASSERT_NULL(bo);
    drm_fb_state_t * fb = (drm_fb_state_t *)gbm_bo_get_user_data(bo);

    if(fb) {
        return fb;
    }

    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    uint32_t handles[4] = {0};
    uint32_t strides[4] = {0};
    uint32_t offsets[4] = {0};
    uint64_t modifiers[4] = {0};
    uint32_t format = gbm_bo_get_format(bo);
    uint64_t modifier = gbm_bo_get_modifier(bo);
    uint32_t fb_id = 0;
    uint64_t addfb2_mods = 0;
    int32_t status;

    drmGetCap(ctx->fd, DRM_CAP_ADDFB2_MODIFIERS, &addfb2_mods);

    for(int i = 0; i < gbm_bo_get_plane_count(bo); i++) {
        handles[i] = gbm_bo_get_handle_for_plane(bo, i).u32;
        strides[i] = gbm_bo_get_stride_for_plane(bo, i);
        offsets[i] = gbm_bo_get_offset(bo, i);
        modifiers[i] = modifier;
    }

    if(addfb2_mods && modifier != DRM_FORMAT_MOD_INVALID) {
        LV_UNUSED(addfb2_mods);
    }

    /* Prefer plain AddFB2 on embedded Mali — modifiers often break SetCrtc. */
    status = drmModeAddFB2(ctx->fd, width, height, format,
                           handles, strides, offsets, &fb_id, 0);

    if(status < 0 && modifier != DRM_FORMAT_MOD_INVALID) {
        status = drmModeAddFB2WithModifiers(ctx->fd, width, height, format,
                                            handles, strides, offsets, modifiers,
                                            &fb_id, DRM_MODE_FB_MODIFIERS);
    }

    if(status < 0) {
        status = drmModeAddFB(ctx->fd, width, height, 24, 32, strides[0], handles[0], &fb_id);
    }

    if(status < 0) {
        LV_LOG_ERROR("Failed to create drm_fb_state: %d (fmt %#x %ux%u stride %u)",
                     status, format, width, height, strides[0]);
        return NULL;
    }

    fb = (drm_fb_state_t *)lv_malloc(sizeof(*fb));
    if(!fb) {
        LV_LOG_ERROR("Failed to allocate drmfb_state");
        return NULL;
    }

    fb->fd = ctx->fd;
    fb->bo = bo;
    fb->fb_id = fb_id;

    gbm_bo_set_user_data(bo, fb, drm_fb_state_destroy_cb);
    return fb;
}

static drm_fb_state_t * drm_fb_state_for_present(lv_drm_ctx_t * ctx, struct gbm_bo * bo, drm_fb_state_t * out)
{
    if(ctx->use_atomic && ctx->plane_fourcc && ctx->scanout_from_gl && ctx->scanout_fb_id) {
        out->fd = ctx->fd;
        out->bo = NULL;
        out->fb_id = ctx->scanout_fb_id;
        ctx->scanout_from_gl = false;
        return out;
    }

    const uint32_t bo_fmt = gbm_bo_get_format(bo);

    if(ctx->use_atomic && ctx->plane_fourcc && ctx->gbm_fourcc == ctx->plane_fourcc) {
        static bool logged;
        if(!logged) {
            LV_LOG_USER("DRM direct GBM scanout fmt %#x (no convert)", ctx->plane_fourcc);
            logged = true;
        }
    }

    if(ctx->use_atomic && ctx->plane_fourcc && bo_fmt != ctx->plane_fourcc) {
        static bool logged;
        if(!logged) {
            LV_LOG_USER("ARGB GLES render (%#x) → plane scanout (%#x)", bo_fmt, ctx->plane_fourcc);
            logged = true;
        }
        if(drm_ensure_scanout_bo(ctx, gbm_bo_get_width(bo), gbm_bo_get_height(bo)) != LV_RESULT_OK) {
            return NULL;
        }
        drm_copy_gbm_bo_to_scanout(ctx, bo);
        out->fd = ctx->fd;
        out->bo = NULL;
        out->fb_id = ctx->scanout_fb_id;
        return out;
    }

    return drm_fb_state_create(ctx, bo);
}

static uint32_t drm_egl_get_prop_id(drmModePropertyPtr * props, uint32_t count, const char * name)
{
    for(uint32_t i = 0; i < count; i++) {
        if(props[i] && !lv_strcmp(props[i]->name, name)) {
            return props[i]->prop_id;
        }
    }
    return 0;
}

static lv_result_t drm_egl_load_obj_props(int fd, uint32_t obj_id, uint32_t obj_type,
                                          drmModePropertyPtr * out_props, uint32_t * out_count)
{
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, obj_id, obj_type);
    if(!props) {
        return LV_RESULT_INVALID;
    }

    *out_count = props->count_props;
    if(*out_count > DRM_EGL_MAX_PROPS) {
        *out_count = DRM_EGL_MAX_PROPS;
    }

    for(uint32_t i = 0; i < *out_count; i++) {
        out_props[i] = drmModeGetProperty(fd, props->props[i]);
    }
    drmModeFreeObjectProperties(props);
    return LV_RESULT_OK;
}

static void drm_egl_free_props(drmModePropertyPtr * props, uint32_t count)
{
    for(uint32_t i = 0; i < count; i++) {
        if(props[i]) {
            drmModeFreeProperty(props[i]);
            props[i] = NULL;
        }
    }
}

static int drm_egl_find_plane(lv_drm_ctx_t * ctx, uint32_t fourcc, uint32_t * plane_id)
{
    drmModePlaneResPtr planes = drmModeGetPlaneResources(ctx->fd);
    if(!planes) {
        return -1;
    }

    int ret = -1;
    for(uint32_t i = 0; i < planes->count_planes; i++) {
        drmModePlanePtr plane = drmModeGetPlane(ctx->fd, planes->planes[i]);
        if(!plane) {
            continue;
        }

        if(!(plane->possible_crtcs & (1u << ctx->crtc_idx))) {
            drmModeFreePlane(plane);
            continue;
        }

        uint32_t j;
        for(j = 0; j < plane->count_formats; j++) {
            if(plane->formats[j] == fourcc) {
                break;
            }
        }

        if(j < plane->count_formats) {
            *plane_id = plane->plane_id;
            ret = 0;
            drmModeFreePlane(plane);
            break;
        }

        drmModeFreePlane(plane);
    }

    drmModeFreePlaneResources(planes);
    return ret;
}

static int drm_egl_atomic_add_prop(lv_drm_ctx_t * ctx, drmModeAtomicReqPtr req,
                                   uint32_t obj_id, drmModePropertyPtr * props, uint32_t prop_count,
                                   const char * name, uint64_t value)
{
    uint32_t prop_id = drm_egl_get_prop_id(props, prop_count, name);
    if(!prop_id) {
        LV_LOG_ERROR("DRM property not found: %s", name);
        return -1;
    }
    int ret = drmModeAtomicAddProperty(req, obj_id, prop_id, value);
    if(ret < 0) {
        LV_LOG_ERROR("drmModeAtomicAddProperty %s failed: %d", name, ret);
    }
    return ret;
}

static void drm_egl_atomic_deinit(lv_drm_ctx_t * ctx)
{
    if(!ctx) {
        return;
    }

    if(ctx->atomic_req) {
        drmModeAtomicFree(ctx->atomic_req);
        ctx->atomic_req = NULL;
    }

    if(ctx->mode_blob_id) {
        drmModeDestroyPropertyBlob(ctx->fd, ctx->mode_blob_id);
        ctx->mode_blob_id = 0;
    }

    drm_egl_free_props(ctx->plane_props, ctx->count_plane_props);
    drm_egl_free_props(ctx->crtc_props, ctx->count_crtc_props);
    drm_egl_free_props(ctx->conn_props, ctx->count_conn_props);
    ctx->count_plane_props = 0;
    ctx->count_crtc_props = 0;
    ctx->count_conn_props = 0;
    ctx->use_atomic = false;
    ctx->atomic_modeset_done = false;
    ctx->plane_id = 0;
    ctx->plane_fourcc = 0;
}

static void drm_egl_log_planes(lv_drm_ctx_t * ctx)
{
    drmModePlaneResPtr planes = drmModeGetPlaneResources(ctx->fd);
    if(!planes) {
        return;
    }

    for(uint32_t i = 0; i < planes->count_planes; i++) {
        drmModePlanePtr plane = drmModeGetPlane(ctx->fd, planes->planes[i]);
        if(!plane) {
            continue;
        }
        LV_LOG_USER("DRM plane %u possible_crtcs %#x formats:",
                    plane->plane_id, plane->possible_crtcs);
        for(uint32_t j = 0; j < plane->count_formats; j++) {
            uint32_t f = plane->formats[j];
            LV_LOG_USER("  %#x %c%c%c%c", f,
                        (char)(f & 0xff), (char)((f >> 8) & 0xff),
                        (char)((f >> 16) & 0xff), (char)((f >> 24) & 0xff));
        }
        drmModeFreePlane(plane);
    }
    drmModeFreePlaneResources(planes);
    LV_UNUSED(ctx);
}

static lv_result_t drm_egl_atomic_init(lv_drm_ctx_t * ctx)
{
    /* DP primary plane (modetest): AR24/XB24 + RG24/BG24 — prefer ARGB to match Mali GBM */
    static const uint32_t plane_formats[] = {
        DRM_FORMAT_ARGB8888,
        DRM_FORMAT_XRGB8888,
        DRM_FORMAT_RGB888,
        DRM_FORMAT_BGR888,
        DRM_FORMAT_RGB565,
    };

    ctx->plane_fourcc = 0;
    for(size_t fi = 0; fi < sizeof(plane_formats) / sizeof(plane_formats[0]); fi++) {
        if(drm_egl_find_plane(ctx, plane_formats[fi], &ctx->plane_id) == 0) {
            ctx->plane_fourcc = plane_formats[fi];
            LV_LOG_USER("DRM plane %u scanout format %#x", ctx->plane_id, ctx->plane_fourcc);
            break;
        }
        ctx->plane_id = 0;
    }

    if(!ctx->plane_id) {
        LV_LOG_WARN("No suitable DRM plane found for atomic KMS");
        drm_egl_log_planes(ctx);
        return LV_RESULT_INVALID;
    }

    if(drm_egl_load_obj_props(ctx->fd, ctx->plane_id, DRM_MODE_OBJECT_PLANE,
                              ctx->plane_props, &ctx->count_plane_props) != LV_RESULT_OK) {
        goto fail;
    }

    if(drm_egl_load_obj_props(ctx->fd, ctx->drm_crtc->crtc_id, DRM_MODE_OBJECT_CRTC,
                              ctx->crtc_props, &ctx->count_crtc_props) != LV_RESULT_OK) {
        goto fail;
    }

    if(drm_egl_load_obj_props(ctx->fd, ctx->drm_connector->connector_id, DRM_MODE_OBJECT_CONNECTOR,
                              ctx->conn_props, &ctx->count_conn_props) != LV_RESULT_OK) {
        goto fail;
    }

    if(drmModeCreatePropertyBlob(ctx->fd, ctx->drm_mode, sizeof(*ctx->drm_mode), &ctx->mode_blob_id)) {
        LV_LOG_ERROR("Failed to create DRM mode property blob");
        goto fail;
    }

    return LV_RESULT_OK;

fail:
    drm_egl_atomic_deinit(ctx);
    return LV_RESULT_INVALID;
}

static int drm_egl_atomic_present(lv_drm_ctx_t * ctx, drm_fb_state_t * pending_fb)
{
    drmModeAtomicReqPtr req = drmModeAtomicAlloc();
    if(!req) {
        return -ENOMEM;
    }

    const uint32_t crtc_id = ctx->drm_crtc->crtc_id;
    const uint32_t conn_id = ctx->drm_connector->connector_id;
    const uint32_t w = ctx->drm_mode->hdisplay;
    const uint32_t h = ctx->drm_mode->vdisplay;
    uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;

    if(!ctx->atomic_modeset_done) {
        if(drm_egl_atomic_add_prop(ctx, req, conn_id, ctx->conn_props, ctx->count_conn_props,
                                   "CRTC_ID", crtc_id) < 0) {
            goto fail;
        }
        if(drm_egl_atomic_add_prop(ctx, req, crtc_id, ctx->crtc_props, ctx->count_crtc_props,
                                   "MODE_ID", ctx->mode_blob_id) < 0) {
            goto fail;
        }
        if(drm_egl_atomic_add_prop(ctx, req, crtc_id, ctx->crtc_props, ctx->count_crtc_props,
                                   "ACTIVE", 1) < 0) {
            goto fail;
        }
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    }

    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "FB_ID", pending_fb->fb_id) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "CRTC_ID", crtc_id) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "SRC_X", 0) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "SRC_Y", 0) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "SRC_W", (uint64_t)w << 16) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "SRC_H", (uint64_t)h << 16) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "CRTC_X", 0) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "CRTC_Y", 0) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "CRTC_W", w) < 0) {
        goto fail;
    }
    if(drm_egl_atomic_add_prop(ctx, req, ctx->plane_id, ctx->plane_props, ctx->count_plane_props,
                               "CRTC_H", h) < 0) {
        goto fail;
    }

    const bool first_modeset = !ctx->atomic_modeset_done;

    int status = drmModeAtomicCommit(ctx->fd, req, flags, ctx);
    if(status < 0) {
        LV_LOG_ERROR("Atomic commit failed: %s (%d) fb %u plane %u",
                     strerror(errno), status, pending_fb->fb_id, ctx->plane_id);
        drmModeAtomicFree(req);
        return status;
    }

    ctx->atomic_modeset_done = true;
    ctx->crtc_isset = true;
    ctx->atomic_req = req;

    if(first_modeset) {
        LV_LOG_USER("Atomic modeset plane %u fb %u (%ux%u)", ctx->plane_id, pending_fb->fb_id, w, h);
    }

    return 0;

fail:
    drmModeAtomicFree(req);
    return -EINVAL;
}

static void drm_flip_cb(void * driver_data, bool vsync)
{
    lv_drm_ctx_t * ctx = (lv_drm_ctx_t *) driver_data;

    if(ctx->gbm_bo_pending) {
        gbm_surface_release_buffer(ctx->gbm_surface, ctx->gbm_bo_pending);
    }
    ctx->gbm_bo_pending = gbm_surface_lock_front_buffer(ctx->gbm_surface);

    if(!ctx->gbm_bo_pending) {
        LV_LOG_ERROR("Failed to lock front buffer");
        return;
    }

    drm_fb_state_t present_fb;
    drm_fb_state_t * pending_fb = drm_fb_state_for_present(ctx, ctx->gbm_bo_pending, &present_fb);
    if(!pending_fb || pending_fb->fb_id == 0) {
        LV_LOG_ERROR("Failed to create drm framebuffer");
        return;
    }

    if(vsync) {
        while((ctx->gbm_bo_flipped || ctx->atomic_req) && drm_do_page_flip(ctx, -1) >= 0) {
            continue;
        }
    }
    else {
        drm_do_page_flip(ctx, 0);
    }

    if(ctx->gbm_bo_flipped || ctx->atomic_req) {
        return;
    }

    uint32_t crtc_id = ctx->drm_crtc ? ctx->drm_crtc->crtc_id : ctx->drm_encoder->crtc_id;
    uint32_t flip_flags = DRM_MODE_PAGE_FLIP_EVENT;
    int status;

    if(ctx->use_atomic) {
        status = drm_egl_atomic_present(ctx, pending_fb);
        if(status < 0) {
            LV_LOG_ERROR("Failed to present frame (atomic): %d", status);
            return;
        }
        ctx->gbm_bo_flipped = ctx->gbm_bo_pending;
        ctx->gbm_bo_pending = NULL;
    }
    else if(!ctx->crtc_isset) {
        /* Always SetCrtc on first present — page flip only works after our fb owns the CRTC.
         * If fbcon left CRTC active (e.g. fb 36), skipping SetCrtc makes PageFlip return -EINVAL. */
        status = drmModeSetCrtc(ctx->fd, crtc_id, pending_fb->fb_id, 0, 0,
                                &ctx->drm_connector->connector_id, 1, ctx->drm_mode);
        if(status < 0) {
            LV_LOG_WARN("SetCrtc failed (%d), trying page flip", status);
            status = drmModePageFlip(ctx->fd, crtc_id, pending_fb->fb_id, flip_flags, ctx);
            if(status < 0) {
                LV_LOG_ERROR("Failed to present first frame (SetCrtc and page flip): %d", status);
                return;
            }
            ctx->crtc_isset = true;
            ctx->gbm_bo_flipped = ctx->gbm_bo_pending;
            ctx->gbm_bo_pending = NULL;
        }
        else {
            LV_LOG_USER("SetCrtc %u fb %u (%ux%u)", crtc_id, pending_fb->fb_id,
                        ctx->drm_mode->hdisplay, ctx->drm_mode->vdisplay);
            ctx->crtc_isset = true;
            if(ctx->gbm_bo_presented) {
                gbm_surface_release_buffer(ctx->gbm_surface, ctx->gbm_bo_presented);
            }
            ctx->gbm_bo_presented = ctx->gbm_bo_pending;
            ctx->gbm_bo_pending = NULL;
        }
    }
    else {
        status = drmModePageFlip(ctx->fd, crtc_id, pending_fb->fb_id, flip_flags, ctx);
        if(status < 0) {
            LV_LOG_WARN("Page flip failed (%d), falling back to SetCrtc", status);
            status = drmModeSetCrtc(ctx->fd, crtc_id, pending_fb->fb_id, 0, 0,
                                    &ctx->drm_connector->connector_id, 1, ctx->drm_mode);
            if(status < 0) {
                LV_LOG_ERROR("Failed to present frame: %d", status);
                return;
            }
            if(ctx->gbm_bo_presented) {
                gbm_surface_release_buffer(ctx->gbm_surface, ctx->gbm_bo_presented);
            }
            ctx->gbm_bo_presented = ctx->gbm_bo_pending;
            ctx->gbm_bo_pending = NULL;
        }
        else {
            ctx->gbm_bo_flipped = ctx->gbm_bo_pending;
            ctx->gbm_bo_pending = NULL;
        }
    }

    while(!gbm_surface_has_free_buffers(ctx->gbm_surface) &&
          drm_do_page_flip(ctx, -1) >= 0) {
        continue;
    }
}

static lv_result_t drm_device_init(lv_drm_ctx_t * ctx, const char * path)
{
    if(!path) {
        LV_LOG_ERROR("Device path must not be NULL");
        return LV_RESULT_INVALID;
    }
    int ret = open(path, O_RDWR);
    if(ret < 0) {
        LV_LOG_ERROR("Failed to open device path '%s'", path);
        goto open_err;
    }
    ctx->fd = ret;

    ret = drmSetClientCap(ctx->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if(ret < 0) {
        LV_LOG_ERROR("Failed to set universal planes capability");
        goto set_client_cap_err;
    }

    ctx->drm_resources = drmModeGetResources(ctx->fd);
    if(!ctx->drm_resources) {
        LV_LOG_ERROR("Failed to get card resources");
        goto get_resources_err;
    }

    ctx->drm_connector = drm_get_connector(ctx);
    if(!ctx->drm_connector) {
        LV_LOG_ERROR("Failed to find a suitable connector");
        goto get_connector_err;
    }

    ctx->drm_mode = drm_get_mode(ctx);
    if(!ctx->drm_mode) {
        LV_LOG_ERROR("Failed to find a suitable drm mode");
        goto get_mode_err;
    }

    ctx->drm_encoder = drm_get_encoder(ctx);
    if(!ctx->drm_encoder) {
        LV_LOG_ERROR("Failed to find a suitable encoder");
        goto get_encoder_err;
    }

    ctx->gbm_dev = gbm_create_device(ctx->fd);
    if(!ctx->gbm_dev) {
        LV_LOG_ERROR("Failed to create gbm device");
        goto gbm_create_device_err;
    }

    if(drmSetMaster(ctx->fd) < 0) {
        LV_LOG_ERROR("Failed to become DRM master");
        goto set_master_err;
    }

    ctx->drm_crtc = drm_get_crtc(ctx);
    if(!ctx->drm_crtc) {
        LV_LOG_ERROR("Failed to get crtc");
        goto get_crtc_err;
    }

    ctx->crtc_idx = UINT32_MAX;
    for(int i = 0; i < ctx->drm_resources->count_crtcs; i++) {
        if(ctx->drm_resources->crtcs[i] == ctx->drm_crtc->crtc_id) {
            ctx->crtc_idx = (uint32_t)i;
            break;
        }
    }

    if(drmSetClientCap(ctx->fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0 &&
       ctx->crtc_idx != UINT32_MAX &&
       drm_egl_atomic_init(ctx) == LV_RESULT_OK) {
        ctx->use_atomic = true;
        LV_LOG_USER("DRM atomic KMS enabled (plane %u crtc %u)", ctx->plane_id, ctx->drm_crtc->crtc_id);
    }
    else {
        drm_egl_atomic_deinit(ctx);
        LV_LOG_WARN("DRM atomic KMS unavailable, using legacy SetCrtc/PageFlip");
    }

    return LV_RESULT_OK;

get_crtc_err:
    gbm_device_destroy(ctx->gbm_dev);
    ctx->gbm_dev = NULL;
set_master_err:
    /* Nothing special to do */
gbm_create_device_err:
    drmModeFreeEncoder(ctx->drm_encoder);
    ctx->drm_encoder = NULL;
get_encoder_err:
    drmModeFreeConnector(ctx->drm_connector);
    ctx->drm_connector = NULL;
get_mode_err:
    /* Nothing special to do */
get_connector_err:
    drmModeFreeResources(ctx->drm_resources);
    ctx->drm_resources = NULL;
get_resources_err:
    /* Nothing special to do */
set_client_cap_err:
    close(ctx->fd);
    ctx->fd = 0;
open_err:
    return LV_RESULT_INVALID;
}

static size_t drm_egl_select_config_cb(void * driver_data, const lv_egl_config_t * configs, size_t config_count)
{
    LV_UNUSED(driver_data);

    for(size_t i = 0; i < config_count; ++i) {
        lv_color_format_t cf = lv_opengles_egl_color_format_from_egl_config(&configs[i]);
        LV_LOG_USER("DRM EGL config %zu priority %d (%s %d-%d-%d-%d)", i,
                    lv_linux_drm_egl_config_priority(&configs[i]),
                    cf == LV_COLOR_FORMAT_ARGB8888 ? "ARGB8888" :
                    cf == LV_COLOR_FORMAT_RGB888 ? "RGB888" :
                    cf == LV_COLOR_FORMAT_RGB565 ? "RGB565" : "other",
                    configs[i].r_bits, configs[i].g_bits, configs[i].b_bits, configs[i].a_bits);
        LV_LOG_TRACE("Got config %zu %#x %dx%d %d %d %d %d buffer size %d depth %d  samples %d stencil %d surface type %d",
                     i, configs[i].id,
                     configs[i].max_width, configs[i].max_height, configs[i].r_bits, configs[i].g_bits, configs[i].b_bits, configs[i].a_bits,
                     configs[i].buffer_size, configs[i].depth, configs[i].samples, configs[i].stencil, configs[i].surface_type);
    }

    /* lv_opengles_egl.c tries configs sorted by lv_linux_drm_egl_config_priority(). */
    return config_count;
}

int lv_linux_drm_egl_config_priority(const lv_egl_config_t * config)
{
    if(!config) return 100;

    const bool is_window = (config->surface_type & EGL_WINDOW_BIT) != 0;
    const bool is_gles2 = (config->renderable_type & EGL_OPENGL_ES2_BIT) != 0;
    if(!is_window || !is_gles2) return 90;

    lv_color_format_t cf = lv_opengles_egl_color_format_from_egl_config(config);
    switch(cf) {
        case LV_COLOR_FORMAT_ARGB8888:
            return 0; /* Mali GBM ARGB — matches DP plane AR24 direct scanout */
        case LV_COLOR_FORMAT_RGB888:
            return 1;
        case LV_COLOR_FORMAT_RGB565:
            return 2;
        default:
            return 50;
    }
}


/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_egl_interface_t drm_get_egl_interface(lv_drm_ctx_t * ctx)
{
    return (lv_egl_interface_t) {
        .driver_data = ctx,
        .native_display = ctx->gbm_dev,
        .egl_platform = EGL_PLATFORM_GBM_KHR,
        .select_config = drm_egl_select_config_cb,
        .flip_cb = drm_flip_cb,
        .create_window_cb = drm_create_window,
        .destroy_window_cb = drm_destroy_window,
    };
}

static drmModeConnector * drm_get_connector(lv_drm_ctx_t * ctx)
{
    drmModeConnector * connector = NULL;
    drmModeConnector * fallback = NULL;

    LV_ASSERT_NULL(ctx->drm_resources);
    for(int i = 0; i < ctx->drm_resources->count_connectors; i++) {
        connector = drmModeGetConnector(ctx->fd, ctx->drm_resources->connectors[i]);
        if(!connector) {
            continue;
        }
        if(connector->connection != DRM_MODE_CONNECTED || connector->count_modes == 0) {
            drmModeFreeConnector(connector);
            connector = NULL;
            continue;
        }
        if(connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
            LV_LOG_USER("DRM connector DP-%u selected", connector->connector_type_id);
            if(fallback) {
                drmModeFreeConnector(fallback);
            }
            return connector;
        }
        if(!fallback) {
            fallback = connector;
            connector = NULL;
        }
        else {
            drmModeFreeConnector(connector);
            connector = NULL;
        }
    }
    if(fallback) {
        LV_LOG_USER("DRM connector type %u id %u (no DP found)", fallback->connector_type,
                    fallback->connector_type_id);
    }
    return fallback;
}

static drmModeModeInfo * drm_get_mode(lv_drm_ctx_t * ctx)
{
    LV_ASSERT_NULL(ctx->drm_connector);
    if(ctx->mode_select_cb) {
        lv_linux_drm_mode_t * modes = lv_malloc(sizeof(lv_linux_drm_mode_t) * ctx->drm_connector->count_modes);
        if(!modes) {
            LV_LOG_WARN("Failed to allocate memory for drm modes");
            return NULL;
        }
        for(int i = 0; i < ctx->drm_connector->count_modes; i++) {
            modes[i].mode_info = &ctx->drm_connector->modes[i];
        }
        size_t mode_index = ctx->mode_select_cb(ctx->display, modes, (size_t)ctx->drm_connector->count_modes);
        lv_free(modes);

        if(mode_index >= (size_t)ctx->drm_connector->count_modes) {
            LV_LOG_ERROR("Failed to select drm mode. User select callback return an invalid mode index");
            return NULL;
        }
        return &ctx->drm_connector->modes[mode_index];
    }

    drmModeModeInfo * best_mode = NULL;
    uint32_t best_area = 0;

    for(int i = 0 ; i < ctx->drm_connector->count_modes; ++i) {
        drmModeModeInfo * mode = &ctx->drm_connector->modes[i];
        if(mode->type & DRM_MODE_TYPE_PREFERRED) {
            return mode;
        }
        uint32_t area = mode->hdisplay * mode->vdisplay;
        if(area > best_area) {
            best_area = area;
            best_mode = mode;
        }
    }
    LV_LOG_WARN("Failed to find a drm mode with the TYPE_PREFERRED flag. Using the one with the biggest area");
    return best_mode;
}

static drmModeCrtc * drm_get_crtc(lv_drm_ctx_t * ctx)
{
    drmModeCrtc * crtc = drmModeGetCrtc(ctx->fd, ctx->drm_encoder->crtc_id);
    if(crtc) {
        return crtc;
    }

    /* if there is no current CRTC, attach a suitable one */
    for(int i = 0; i < ctx->drm_resources->count_crtcs; i++) {
        if(ctx->drm_encoder->possible_crtcs & (1 << i)) {
            ctx->drm_encoder->crtc_id = ctx->drm_resources->crtcs[i];
            crtc = drmModeGetCrtc(ctx->fd, ctx->drm_encoder->crtc_id);
            break;
        }
    }
    return crtc;
}

static drmModeEncoder * drm_get_encoder(lv_drm_ctx_t * ctx)
{
    LV_ASSERT_NULL(ctx->drm_connector);
    drmModeEncoder * encoder = NULL;
    for(int i = 0; i < ctx->drm_resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(ctx->fd, ctx->drm_resources->encoders[i]);
        if(!encoder) {
            continue;
        }
        for(int j = 0; j < ctx->drm_connector->count_encoders; j++) {
            if(encoder->encoder_id == ctx->drm_connector->encoders[j]) {
                return encoder;
            }
        }
        drmModeFreeEncoder(encoder);
        encoder = NULL;
    }
    return encoder;
}

static void * drm_create_window(void * driver_data, const lv_egl_native_window_properties_t * properties)
{
    lv_drm_ctx_t * ctx = (lv_drm_ctx_t *)driver_data;
    LV_ASSERT_NULL(ctx->gbm_dev);

    uint32_t format = properties->visual_id;

    if(format == 0) {
        LV_LOG_ERROR("Invalid format requested");
        return NULL;
    }

    if(ctx->gbm_surface) {
        drm_destroy_window(driver_data, ctx->gbm_surface);
    }

    ctx->gbm_surface = gbm_surface_create(ctx->gbm_dev, ctx->drm_mode->hdisplay, ctx->drm_mode->vdisplay, format,
                                          GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if(!ctx->gbm_surface) {
        ctx->gbm_surface = gbm_surface_create(ctx->gbm_dev, ctx->drm_mode->hdisplay, ctx->drm_mode->vdisplay, format,
                                              GBM_BO_USE_RENDERING);
    }
    if(!ctx->gbm_surface) {
        LV_LOG_ERROR("Failed to create GBM surface (fmt %#x %ux%u)",
                     format, ctx->drm_mode->hdisplay, ctx->drm_mode->vdisplay);
        return NULL;
    }
    ctx->gbm_fourcc = format;
    LV_LOG_USER("GBM surface fmt %#x %ux%u", format, ctx->drm_mode->hdisplay, ctx->drm_mode->vdisplay);
    return (void *)ctx->gbm_surface;
}

static void drm_destroy_window(void * driver_data, void * native_window)
{
    lv_drm_ctx_t * ctx = (lv_drm_ctx_t *)driver_data;
    LV_ASSERT(native_window == ctx->gbm_surface);

    if(!ctx->gbm_surface) {
        return;
    }

    if(ctx->gbm_bo_pending) {
        gbm_surface_release_buffer(ctx->gbm_surface, ctx->gbm_bo_pending);
        ctx->gbm_bo_pending = NULL;
    }
    if(ctx->gbm_bo_flipped) {
        gbm_surface_release_buffer(ctx->gbm_surface, ctx->gbm_bo_flipped);
        ctx->gbm_bo_flipped = NULL;
    }
    if(ctx->gbm_bo_presented) {
        gbm_surface_release_buffer(ctx->gbm_surface, ctx->gbm_bo_presented);
        ctx->gbm_bo_presented = NULL;
    }
    gbm_surface_destroy(ctx->gbm_surface);
    ctx->gbm_surface = NULL;
}


#endif /*LV_USE_LINUX_DRM && LV_LINUX_DRM_USE_EGL*/
