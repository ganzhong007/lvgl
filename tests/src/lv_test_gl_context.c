/**
 * @file lv_test_gl_context.c
 * Headless EGL pbuffer + GLES2 for Unity EVGPU / C_R_T screenshot tests.
 */

#include "../../lvgl.h"
#include "lv_test_gl_context.h"

#if LV_BUILD_TEST && ((defined(LV_USE_DRAW_EVGPU) && LV_USE_DRAW_EVGPU) || \
                      (defined(LV_USE_DRAW_EVGPU_C_R_T) && LV_USE_DRAW_EVGPU_C_R_T))

#include "../../src/display/lv_display_private.h"
#include "../../src/draw/lv_draw_private.h"
#include "lvgl/drivers/opengles/lv_opengles_driver.h"
#include "../../src/drivers/opengles/glad/include/glad/gles2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

typedef struct {
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
    bool ready;
} lv_test_egl_t;

static lv_test_egl_t s_egl;
static bool s_layer_fbo_ready;

static bool egl_create_pbuffer(int32_t w, int32_t h)
{
    EGLDisplay dpy = EGL_NO_DISPLAY;

    const char * exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_plat = NULL;
    if(exts && strstr(exts, "EGL_EXT_platform_base")) {
        get_plat = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    }
    if(get_plat) {
        dpy = get_plat(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    }
    if(dpy == EGL_NO_DISPLAY) {
        dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }
    if(dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "lv_test_gl: eglGetDisplay failed (0x%x)\n", eglGetError());
        return false;
    }

    EGLint major = 0, minor = 0;
    if(!eglInitialize(dpy, &major, &minor)) {
        fprintf(stderr, "lv_test_gl: eglInitialize failed (0x%x)\n", eglGetError());
        return false;
    }

    if(!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "lv_test_gl: eglBindAPI failed (0x%x)\n", eglGetError());
        return false;
    }

    const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint n = 0;
    if(!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) {
        fprintf(stderr, "lv_test_gl: eglChooseConfig failed (0x%x)\n", eglGetError());
        return false;
    }

    const EGLint pb_attr[] = {
        EGL_WIDTH, w,
        EGL_HEIGHT, h,
        EGL_NONE
    };
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb_attr);
    if(surf == EGL_NO_SURFACE) {
        fprintf(stderr, "lv_test_gl: eglCreatePbufferSurface failed (0x%x)\n", eglGetError());
        return false;
    }

    const EGLint ctx_attr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if(ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "lv_test_gl: eglCreateContext failed (0x%x)\n", eglGetError());
        eglDestroySurface(dpy, surf);
        return false;
    }

    if(!eglMakeCurrent(dpy, surf, surf, ctx)) {
        fprintf(stderr, "lv_test_gl: eglMakeCurrent failed (0x%x)\n", eglGetError());
        eglDestroyContext(dpy, ctx);
        eglDestroySurface(dpy, surf);
        return false;
    }

    s_egl.display = dpy;
    s_egl.config = cfg;
    s_egl.context = ctx;
    s_egl.surface = surf;
    return true;
}

bool lv_test_gl_context_init(void)
{
    if(s_egl.ready) return true;

    if(!egl_create_pbuffer(800, 480)) {
        return false;
    }

    if(!gladLoadGLES2((GLADloadfunc)eglGetProcAddress)) {
        fprintf(stderr, "lv_test_gl: gladLoadGLES2 failed\n");
        lv_test_gl_context_deinit();
        return false;
    }

    fprintf(stderr, "lv_test_gl: %s / %s\n",
            (const char *)glGetString(GL_VERSION),
            (const char *)glGetString(GL_RENDERER));

    lv_opengles_init();
    s_egl.ready = true;
    s_layer_fbo_ready = false;
    return true;
}

void lv_test_gl_context_deinit(void)
{
    if(s_egl.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s_egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if(s_egl.context != EGL_NO_CONTEXT) {
            eglDestroyContext(s_egl.display, s_egl.context);
        }
        if(s_egl.surface != EGL_NO_SURFACE) {
            eglDestroySurface(s_egl.display, s_egl.surface);
        }
        eglTerminate(s_egl.display);
    }
    memset(&s_egl, 0, sizeof(s_egl));
    s_egl.display = EGL_NO_DISPLAY;
    s_egl.context = EGL_NO_CONTEXT;
    s_egl.surface = EGL_NO_SURFACE;
    s_layer_fbo_ready = false;
}

static void ensure_layer_fbo(lv_display_t * disp)
{
    lv_layer_t * layer = disp->layer_head;
    if(!layer) return;

    const int32_t w = lv_display_get_horizontal_resolution(disp);
    const int32_t h = lv_display_get_vertical_resolution(disp);

    if(s_layer_fbo_ready && layer->user_data != NULL) {
        return;
    }

    if(layer->user_data != NULL) {
        lv_draw_unit_send_event(NULL, LV_EVENT_CHILD_DELETED, layer);
        layer->user_data = NULL;
    }

    layer->buf_area.x1 = 0;
    layer->buf_area.y1 = 0;
    layer->buf_area.x2 = w - 1;
    layer->buf_area.y2 = h - 1;
    layer->draw_buf = lv_display_get_buf_active(disp);
    layer->color_format = lv_display_get_color_format(disp);

    lv_draw_unit_send_event(NULL, LV_EVENT_CHILD_CREATED, layer);
    s_layer_fbo_ready = (layer->user_data != NULL);
    if(!s_layer_fbo_ready) {
        fprintf(stderr, "lv_test_gl: CHILD_CREATED did not attach FBO\n");
    }
}

void lv_test_gl_prepare_screenshot(void)
{
    if(!s_egl.ready) return;

    lv_display_t * disp = lv_display_get_default();
    if(!disp || !disp->layer_head) return;

    ensure_layer_fbo(disp);

    lv_obj_t * scr = lv_screen_active();
    if(scr) lv_obj_invalidate(scr);
    lv_refr_now(disp);

    /* GPU content lives on the layer FBO; pull into CPU draw_buf for compare. */
    lv_draw_unit_send_event(NULL, LV_EVENT_SCREEN_LOAD_START, disp->layer_head);
}

#else /* !EVGPU && !C_R_T */

bool lv_test_gl_context_init(void)
{
    return true;
}

void lv_test_gl_context_deinit(void) {}

void lv_test_gl_prepare_screenshot(void) {}

#endif
