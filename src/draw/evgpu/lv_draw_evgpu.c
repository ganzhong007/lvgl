/**
 * @file lv_draw_evgpu.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu.h"

#if LV_USE_DRAW_EVGPU

#if LV_USE_DRAW_NANOVG
    #error "LV_USE_DRAW_EVGPU and LV_USE_DRAW_NANOVG cannot both be enabled."
#endif

#if LV_USE_DRAW_OPENGLES
    #error "LV_USE_DRAW_EVGPU and LV_USE_DRAW_OPENGLES cannot both be enabled."
#endif

#include "../../core/lv_refr_private.h"
#include "lv_draw_evgpu_private.h"
#include "lv_evgpu_utils.h"
#include "lv_evgpu_image_cache.h"
#include "lv_evgpu_fbo_cache.h"
#include "lv_evgpu_fbo_pool.h"
#include "lv_evgpu_context.h"
#include "lv_evgpu_shader.h"
#if LV_USE_EVGPU_LIB
#include "../../libs/evgpu/lv_evgpu_lib.h"
#endif
#include "lv_evgpu_grad.h"
#include "lv_evgpu_solid.h"
#include "lv_evgpu_tex.h"
#include "lv_evgpu_blur_kawase.h"
#if LV_USE_3D_DRAW_TASKS
#include "lv_evgpu_3d_pass.h"
#include "../../include/lvgl/draw/lv_draw_3d_viewport.h"
#include "../../include/lvgl/draw/lv_draw_3d_clear.h"
#include "../../include/lvgl/draw/lv_draw_3d_line.h"
#include "../../include/lvgl/draw/lv_draw_3d_callback.h"
#include "../../include/lvgl/draw/lv_draw_3d_mesh.h"
#include "../../include/lvgl/draw/lv_draw_3d_scene.h"
#endif

#if LV_USE_OPENGLES && LV_USE_EGL
    #include "../../drivers/opengles/lv_opengles_private.h"
#else
    #define EVGR_GL_STATIC_LINK
#endif

#if defined(EVGR_GL2_IMPLEMENTATION)
    #ifdef EVGR_GL_STATIC_LINK
        #include <GL/glew.h>
    #endif
    #define EVGR_CTX_CREATE evgrCreateGL2
    #define EVGR_CTX_DELETE evgrDeleteGL2
#elif defined(EVGR_GL3_IMPLEMENTATION)
    #ifdef EVGR_GL_STATIC_LINK
        #include <GL/glew.h>
    #endif
    #define EVGR_CTX_CREATE evgrCreateGL3
    #define EVGR_CTX_DELETE evgrDeleteGL3
#elif defined(EVGR_GLES2_IMPLEMENTATION)
    #ifdef EVGR_GL_STATIC_LINK
        #include <GLES2/gl2.h>
    #endif
    #define EVGR_CTX_CREATE evgrCreateGLES2
    #define EVGR_CTX_DELETE evgrDeleteGLES2
#elif defined(EVGR_GLES3_IMPLEMENTATION)
    #ifdef EVGR_GL_STATIC_LINK
        #include <GLES3/gl3.h>
    #endif
    #define EVGR_CTX_CREATE evgrCreateGLES3
    #define EVGR_CTX_DELETE evgrDeleteGLES3
#else
    #error "No EVGR implementation defined"
#endif

#include "../../libs/evgpu/evgpu_evgr_gl.h"
#include "../../libs/evgpu/evgpu_evgr_gl_utils.h"
#include "../../misc/lv_port_layer_trace.h"

/* GL_BGRA may not be defined on all platforms */
#ifndef GL_BGRA
    #ifdef GL_BGRA_EXT
        #define GL_BGRA GL_BGRA_EXT
    #else
        #define GL_BGRA 0x80E1
    #endif
#endif

/*********************
 *      DEFINES
 *********************/

#define EVGPU_DRAW_UNIT_ID 11

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static int32_t draw_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer);
static int32_t draw_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task);
static int32_t draw_delete(lv_draw_unit_t * draw_unit);
static void draw_event_cb(lv_event_t * e);

#if LV_USE_PORT_LAYER_TRACE
static const char * evgr_draw_task_name(lv_draw_task_type_t type)
{
    switch(type) {
        case LV_DRAW_TASK_TYPE_FILL: return "FILL";
        case LV_DRAW_TASK_TYPE_BORDER: return "BORDER";
        case LV_DRAW_TASK_TYPE_LABEL: return "LABEL";
        case LV_DRAW_TASK_TYPE_IMAGE: return "IMAGE";
        default: return "OTHER";
    }
}
#endif

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_evgpu_init(void)
{
    static bool initialized = false;
    if(initialized) return;
    initialized = true;

    lv_draw_evgpu_unit_t * unit = lv_draw_create_unit(sizeof(lv_draw_evgpu_unit_t));
    unit->base_unit.dispatch_cb = draw_dispatch;
    unit->base_unit.evaluate_cb = draw_evaluate;
    unit->base_unit.delete_cb = draw_delete;
    unit->base_unit.event_cb = draw_event_cb;
    unit->base_unit.name = "EVGPU";

    unit->evgr = EVGR_CTX_CREATE(0);
    LV_ASSERT_MSG(unit->evgr != NULL, "EVGR init failed");

#if LV_USE_EVGPU_LIB
    lv_evgpu_lib_init(unit);
#else
    lv_evgpu_context_init(unit, &unit->ctx);
    lv_evgpu_shader_init(unit, &unit->ctx, &unit->shader);
#endif
    lv_evgpu_grad_init(unit);
    lv_evgpu_solid_init(unit);
    lv_evgpu_tex_init(unit);

    lv_evgpu_utils_init(unit);
    lv_evgpu_image_cache_init(unit);
    lv_evgpu_fbo_cache_init(unit);
    lv_evgpu_fbo_pool_init(unit);
    lv_draw_evgpu_label_init(unit);
    lv_draw_evgpu_blur_init(unit);

#if LV_USE_VECTOR_GRAPHIC
    LV_LOG_INFO("EVGPU vector core ready");
#endif

    LV_LOG_INFO("EVGPU G5 complete 2D ready (line/arc/layer/mask/grad-extend)");
#if LV_USE_3DTEXTURE
    LV_LOG_INFO("EVGPU 3D BLIT ready (LV_DRAW_TASK_TYPE_3D composite)");
#endif
#if LV_USE_3D_DRAW_TASKS
    lv_draw_evgpu_3d_line_init();
    lv_draw_evgpu_3d_cb_init();
    lv_draw_evgpu_3d_mesh_init();
#if LV_USE_GLTF
    lv_draw_evgpu_3d_scene_init();
#endif
    LV_LOG_INFO("EVGPU 3D viewport ready (3D_VIEWPORT + 3D_CLEAR resolve)");
#endif
    LV_LOG_INFO("DrawUnitEVGPU ready (bootstrap GLES2 backend, unit_id=%d)", EVGPU_DRAW_UNIT_ID);
}

int lv_evgpu_fb_get_image_handle(struct EVGRLUframebuffer * fb)
{
    LV_ASSERT_NULL(fb);
    return fb->image;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void draw_execute(lv_draw_evgpu_unit_t * u, lv_draw_task_t * t)
{
    /* remember draw unit for access to unit's context */
    t->draw_unit = (lv_draw_unit_t *)u;
    lv_layer_t * layer = t->target_layer;

    lv_matrix_t global_matrix;
    lv_matrix_identity(&global_matrix);
    if(layer->buf_area.x1 || layer->buf_area.y1) {
        lv_matrix_translate(&global_matrix, -layer->buf_area.x1, -layer->buf_area.y1);
    }

#if LV_DRAW_TRANSFORM_USE_MATRIX
    lv_matrix_t layer_matrix = t->matrix;
    lv_matrix_multiply(&global_matrix, &layer_matrix);
#endif

    /* EVGR will output premultiplied image, set the flag correspondingly. */
    if(layer->draw_buf) {
        lv_draw_buf_set_flag(layer->draw_buf, LV_IMAGE_FLAGS_PREMULTIPLIED);
    }

    evgrReset(u->evgr);
    lv_evgpu_transform(u->evgr, &global_matrix);

    lv_evgpu_set_clip_area(u->evgr, &t->clip_area);

    lv_evgpu_context_set_draw_state(&u->ctx, &global_matrix, &t->clip_area,
                                   lv_area_get_width(&layer->buf_area),
                                   lv_area_get_height(&layer->buf_area));

#if LV_USE_PORT_LAYER_TRACE
    LV_PORT_LAYER_TRACE("L3-EVGPU", "execute %s at (%d,%d)-(%d,%d)",
                        evgr_draw_task_name(t->type),
                        (int)t->area.x1, (int)t->area.y1, (int)t->area.x2, (int)t->area.y2);
#endif

    switch(t->type) {
        case LV_DRAW_TASK_TYPE_FILL:
            lv_draw_evgpu_fill(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_BORDER:
            lv_draw_evgpu_border(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_BOX_SHADOW:
            lv_draw_evgpu_box_shadow(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_LETTER:
            lv_draw_evgpu_letter(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_LABEL:
            lv_draw_evgpu_label(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_IMAGE:
            lv_draw_evgpu_image(t, t->draw_dsc, &t->area, -1);
            break;

        case LV_DRAW_TASK_TYPE_LAYER:
            lv_draw_evgpu_layer(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_LINE:
            lv_draw_line_iterate(t, t->draw_dsc, lv_draw_evgpu_line);
            break;

        case LV_DRAW_TASK_TYPE_ARC:
            lv_draw_evgpu_arc(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_TRIANGLE:
            lv_draw_evgpu_triangle(t, t->draw_dsc);
            break;

        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE:
            lv_draw_evgpu_mask_rect(t, t->draw_dsc);
            break;

#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR:
            lv_draw_evgpu_vector(t, t->draw_dsc);
            break;
#endif

#if LV_USE_3DTEXTURE
        case LV_DRAW_TASK_TYPE_3D:
            lv_draw_evgpu_3d(t, t->draw_dsc, &t->area);
            break;
#endif

#if LV_USE_3D_DRAW_TASKS
        case LV_DRAW_TASK_TYPE_3D_VIEWPORT:
            lv_draw_evgpu_3d_viewport(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_3D_CLEAR:
            lv_draw_evgpu_3d_clear(t, t->draw_dsc);
            break;
        case LV_DRAW_TASK_TYPE_3D_LINE:
            lv_draw_evgpu_3d_line(t, t->draw_dsc);
            break;
        case LV_DRAW_TASK_TYPE_3D_CALLBACK:
            lv_draw_evgpu_3d_cb(t, t->draw_dsc);
            break;
        case LV_DRAW_TASK_TYPE_3D_MESH:
            lv_draw_evgpu_3d_mesh(t, t->draw_dsc);
            break;
#if LV_USE_GLTF
        case LV_DRAW_TASK_TYPE_3D_SCENE:
            lv_draw_evgpu_3d_scene(t, t->draw_dsc);
            break;
#endif
#endif

        case LV_DRAW_TASK_TYPE_BLUR:
            lv_draw_evgpu_blur(t, t->draw_dsc, &t->area);
            break;

        default:
            LV_LOG_ERROR("unknown draw task type: %d", t->type);
            break;
    }
}

static void on_layer_changed(lv_layer_t * new_layer)
{
    LV_PROFILER_DRAW_BEGIN;

#if LV_USE_3D_DRAW_TASKS
    if(lv_evgpu_3d_pass_layer_is(new_layer)) {
        evgrluBindFramebuffer(NULL);
        LV_PROFILER_DRAW_END;
        return;
    }
#endif

    if(!new_layer->user_data) {
        /* Bind the default framebuffer for normal rendering */
        evgrluBindFramebuffer(NULL);
        LV_PROFILER_DRAW_END;
        return;
    }

    LV_PROFILER_BEGIN_TAG("evgrBindFramebuffer");
    evgrluBindFramebuffer(lv_evgpu_fbo_cache_entry_to_fb(new_layer->user_data));
    LV_PROFILER_END_TAG("evgrBindFramebuffer");

    /* Clear the off-screen framebuffer */
    LV_PROFILER_DRAW_BEGIN_TAG("glClear");
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    LV_PROFILER_DRAW_END_TAG("glClear");

    LV_PROFILER_DRAW_END;
}

static void on_layer_readback(lv_draw_evgpu_unit_t * u, lv_layer_t * layer)
{
    LV_PROFILER_DRAW_BEGIN;
    LV_ASSERT_NULL(u);
    LV_ASSERT_NULL(layer);

    lv_cache_entry_t * entry = layer->user_data;

    if(!entry) {
        LV_LOG_WARN("No entry available for layer: %p", (void *)layer);
        LV_PROFILER_DRAW_END;
        return;
    }

    if(!layer->draw_buf) {
        LV_LOG_WARN("No draw buffer available for layer: %p", (void *)layer);
        LV_PROFILER_DRAW_END;
        return;
    }

    struct EVGRLUframebuffer * fb = lv_evgpu_fbo_cache_entry_to_fb(entry);
    if(!fb) {
        LV_LOG_ERROR("No framebuffer available for layer: %p", (void *)layer);
        LV_PROFILER_DRAW_END;
        return;
    }

    /* Bind the FBO for reading */
    evgrluBindFramebuffer(fb);

    int32_t w = lv_area_get_width(&layer->buf_area);
    int32_t h = lv_area_get_height(&layer->buf_area);
    lv_draw_buf_t * draw_buf = layer->draw_buf;

    /* Read pixels from FBO */
    GLenum format;
    GLenum type;

    /* OpenGL reads bottom-to-top, but LVGL expects top-to-bottom */
    switch(draw_buf->header.cf) {
        case LV_COLOR_FORMAT_ARGB8888:
        case LV_COLOR_FORMAT_XRGB8888:
        case LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:
            format = GL_BGRA;
            type = GL_UNSIGNED_BYTE;
            break;

        case LV_COLOR_FORMAT_RGB888:
            format = GL_RGB;
            type = GL_UNSIGNED_BYTE;
            break;

        case LV_COLOR_FORMAT_RGB565:
            format = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
            break;

        default:
            LV_LOG_WARN("Unsupported color format: %d", draw_buf->header.cf);
            LV_PROFILER_DRAW_END;
            return;
    }

    for(int32_t y = 0; y < h; y++) {
        /* Reverse Y coordinate */
        void * row = lv_draw_buf_goto_xy(draw_buf, 0, h - 1 - y);
        LV_PROFILER_DRAW_BEGIN_TAG("glReadPixels");
        glReadPixels(0, y, w, 1, format, type, row);
        LV_PROFILER_DRAW_END_TAG("glReadPixels");

        if(draw_buf->header.cf == LV_COLOR_FORMAT_RGB888) {
            /* Swizzle RGB -> BGR */
            lv_color_t * px = row;
            for(int32_t x = 0; x < w; x++) {
                uint8_t r = px->blue;
                px->blue = px->red;
                px->red = r;
                px++;
            }
        }
    }

    /* Bind back to default framebuffer */
    evgrluBindFramebuffer(NULL);

    /* Mark draw_buf as modified */
    lv_draw_buf_flush_cache(draw_buf, NULL);

    LV_PROFILER_DRAW_END;
}

static int32_t draw_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)draw_unit;

    lv_draw_task_t * t = lv_draw_get_available_task(layer, NULL, EVGPU_DRAW_UNIT_ID);
    if(!t || t->preferred_draw_unit_id != EVGPU_DRAW_UNIT_ID) {
        lv_evgpu_end_frame(u);
        return LV_DRAW_UNIT_IDLE;
    }

    if(u->current_layer != layer) {
        /* Flush any draws still queued for the previous layer before
         * rebinding to the new layer's FBO. Otherwise those queued draws
         * get rerouted to the new FBO when evgrEndFrame is eventually
         * called (and the previous layer ends up missing them). */
        lv_evgpu_end_frame(u);
        on_layer_changed(layer);
        u->current_layer = layer;
    }

#if LV_USE_3D_DRAW_TASKS
    if(lv_evgpu_3d_pass_layer_is(layer)) {
        t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
        draw_execute(u, t);
        t->state = LV_DRAW_TASK_STATE_FINISHED;
        lv_draw_dispatch_request();
        return 1;
    }
#endif

    if(!u->is_started) {
        const int32_t buf_w = lv_area_get_width(&layer->buf_area);
        const int32_t buf_h = lv_area_get_height(&layer->buf_area);

        glViewport(0, 0, buf_w, buf_h);
        LV_PROFILER_DRAW_BEGIN_TAG("evgrBeginFrame");
        evgrBeginFrame(u->evgr, buf_w, buf_h, 1.0f);
        LV_PROFILER_DRAW_END_TAG("evgrBeginFrame");
        LV_PORT_LAYER_TRACE("L3-EVGPU", "evgrBeginFrame %dx%d", (int)buf_w, (int)buf_h);
        u->is_started = true;
    }

    t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;

    draw_execute(u, t);

    t->state = LV_DRAW_TASK_STATE_FINISHED;

    /*The draw unit is free now. Request a new dispatching as it can get a new task*/
    lv_draw_dispatch_request();

    return 1;
}

static int32_t draw_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task)
{
    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)draw_unit;

    switch(task->type) {
        case LV_DRAW_TASK_TYPE_FILL:
        case LV_DRAW_TASK_TYPE_BORDER:
        case LV_DRAW_TASK_TYPE_BOX_SHADOW:
        case LV_DRAW_TASK_TYPE_LETTER:
        case LV_DRAW_TASK_TYPE_LABEL:
        case LV_DRAW_TASK_TYPE_IMAGE:
        case LV_DRAW_TASK_TYPE_LAYER:
        case LV_DRAW_TASK_TYPE_LINE:
        case LV_DRAW_TASK_TYPE_ARC:
        case LV_DRAW_TASK_TYPE_TRIANGLE:
        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE:
#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR:
#endif
#if LV_USE_3DTEXTURE
        case LV_DRAW_TASK_TYPE_3D:
#endif
#if LV_USE_3D_DRAW_TASKS
        case LV_DRAW_TASK_TYPE_3D_VIEWPORT:
        case LV_DRAW_TASK_TYPE_3D_CLEAR:
        case LV_DRAW_TASK_TYPE_3D_LINE:
        case LV_DRAW_TASK_TYPE_3D_CALLBACK:
        case LV_DRAW_TASK_TYPE_3D_MESH:
#if LV_USE_GLTF
        case LV_DRAW_TASK_TYPE_3D_SCENE:
#endif
#endif
            break;

        case LV_DRAW_TASK_TYPE_BLUR:
            if(u->blur_state == NULL && !lv_evgpu_blur_kawase_ready()) return 0;
            break;

        default:
            /*The draw unit is not able to draw this task. */
            return 0;
    }

    if(task->preference_score > 80) {
        /* The draw unit is able to draw this task. */
        task->preference_score = 80;
        task->preferred_draw_unit_id = EVGPU_DRAW_UNIT_ID;
    }

    return 1;
}

static int32_t draw_delete(lv_draw_unit_t * draw_unit)
{
    lv_draw_evgpu_unit_t * unit = (lv_draw_evgpu_unit_t *)draw_unit;
    lv_draw_evgpu_blur_deinit(unit);
#if LV_USE_3D_DRAW_TASKS
    lv_draw_evgpu_3d_line_deinit();
    lv_draw_evgpu_3d_mesh_deinit();
#if LV_USE_GLTF
    lv_draw_evgpu_3d_scene_deinit();
#endif
#endif
    lv_evgpu_fbo_pool_deinit(unit);
    lv_draw_evgpu_label_deinit(unit);
    lv_evgpu_fbo_cache_deinit(unit);
    lv_evgpu_image_cache_deinit(unit);
    lv_evgpu_utils_deinit(unit);
#if LV_USE_EVGPU_LIB
    lv_evgpu_lib_deinit(unit);
#else
    lv_evgpu_shader_deinit(&unit->shader);
    lv_evgpu_context_deinit(&unit->ctx);
#endif
    lv_evgpu_grad_deinit(unit);
    lv_evgpu_solid_deinit(unit);
    lv_evgpu_tex_deinit(unit);
    EVGR_CTX_DELETE(unit->evgr);
    unit->evgr = NULL;
    return 0;
}

static void draw_event_cb(lv_event_t * e)
{
    lv_draw_evgpu_unit_t * u = lv_event_get_current_target(e);
    lv_layer_t * layer = lv_event_get_param(e);

    switch(lv_event_get_code(e)) {
        case LV_EVENT_CANCEL:
            LV_PROFILER_DRAW_BEGIN_TAG("evgrCancelFrame");
            evgrCancelFrame(u->evgr);
            LV_PROFILER_DRAW_END_TAG("evgrCancelFrame");
            lv_evgpu_clean_up(u);
            break;
        case LV_EVENT_CHILD_CREATED: {
                /* The internal rendering uses RGBA format, which is switched to LVGL BGRA format during readback. */
                lv_cache_entry_t * entry = lv_evgpu_fbo_cache_get(u, lv_area_get_width(&layer->buf_area),
                                                                   lv_area_get_height(&layer->buf_area), 0, EVGR_TEXTURE_RGBA);
                layer->user_data = entry;
            }
            break;
        case LV_EVENT_CHILD_DELETED: {
                lv_cache_entry_t * entry = layer->user_data;
                if(entry) {
                    lv_evgpu_fbo_cache_release(u, entry);
                    layer->user_data = NULL;
                }

                /**
                 * Clear current_layer if it's being deleted, so next dispatch
                 * will properly call on_layer_changed even if layer address is reused
                 */
                if(u->current_layer == layer) {
                    u->current_layer = NULL;
                }
            }
            break;
        case LV_EVENT_SCREEN_LOAD_START:
            on_layer_readback(u, layer);
            break;
        case LV_EVENT_INVALIDATE_AREA:
            lv_evgpu_image_cache_drop(u, lv_event_get_param(e));
            break;
        default:
            break;
    }
}

#endif /* LV_USE_DRAW_EVGPU */
