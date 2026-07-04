/**
 * @file lv_linux_drm_egl_private.h
 *
 */

#ifndef LV_LINUX_DRM_EGL_PRIVATE_H
#define LV_LINUX_DRM_EGL_PRIVATE_H


#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../../lvgl_public.h"

#if LV_USE_LINUX_DRM && LV_LINUX_DRM_USE_EGL

#include <xf86drmMode.h>
#include "lv_linux_drm_private.h"
#include "../../opengles/lv_opengles_texture_private.h"
#include "../../opengles/lv_opengles_egl.h"
#include "../../opengles/lv_opengles_egl_private.h"

/*********************
 *      DEFINES
 *********************/

#define DRM_EGL_MAX_PROPS 128
#define DRM_DMABUF_SCANOUT_BUFS 2

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    struct gbm_bo * bo;
    uint32_t fb_id;
    EGLImageKHR image;
    unsigned int texture_id;
} drm_dmabuf_buf_t;

typedef struct {
    lv_opengles_texture_t texture;
    lv_display_t * display;
    lv_opengles_egl_t * egl_ctx;
    lv_egl_interface_t egl_interface;

    drmModeRes * drm_resources;
    drmModeConnector * drm_connector;
    drmModeEncoder * drm_encoder;
    drmModeCrtc * drm_crtc;
    drmModeModeInfo * drm_mode;

    struct gbm_device * gbm_dev;
    struct gbm_surface * gbm_surface;
    struct gbm_bo * gbm_bo_pending;
    struct gbm_bo * gbm_bo_flipped;
    struct gbm_bo * gbm_bo_presented;

    /* ARGB GLES render → separate scanout BO when plane fmt != GBM (e.g. DPI RG24) */
    struct gbm_bo * scanout_bo;
    uint32_t scanout_fb_id;
    uint32_t scanout_w;
    uint32_t scanout_h;
    bool scanout_from_gl;

    /* Ping-pong GBM scanout BOs imported as EGLImage → GL texture (zero-copy present). */
    drm_dmabuf_buf_t dmabuf_bufs[DRM_DMABUF_SCANOUT_BUFS];
    uint32_t dmabuf_buf_count;
    uint32_t dmabuf_render_idx;
    bool dmabuf_scanout_ok;

    lv_linux_drm_select_mode_cb_t mode_select_cb;
    int fd;
    bool crtc_isset;

    /* Atomic KMS (ZynqMP / Mali — legacy SetCrtc often returns -EINVAL) */
    bool use_atomic;
    bool atomic_modeset_done;
    uint32_t plane_id;
    uint32_t plane_fourcc;
    uint32_t gbm_fourcc; /* EGL/GBM window surface format (e.g. ARGB8888) */
    uint32_t crtc_idx;
    uint32_t mode_blob_id;
    drmModeAtomicReqPtr atomic_req;
    uint32_t count_plane_props;
    uint32_t count_crtc_props;
    uint32_t count_conn_props;
    drmModePropertyPtr plane_props[DRM_EGL_MAX_PROPS];
    drmModePropertyPtr crtc_props[DRM_EGL_MAX_PROPS];
    drmModePropertyPtr conn_props[DRM_EGL_MAX_PROPS];
} lv_drm_ctx_t;



/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_LINUX_DRM && LV_LINUX_DRM_USE_EGL*/

#ifdef __cplusplus
} /*extern "C"*/
#endif


#endif /*LV_LINUX_DRM_EGL_PRIVATE_H*/
