#include "lv_draw_evgpu_c_r_t.h"
#if LV_USE_DRAW_EVGPU_C_R_T

#include "lv_draw_evgpu_c_r_t_private.h"
#include "lv_evgpu_c_r_t_fbo.h"
#include "../../core/lv_refr_private.h"
#include "../../misc/lv_port_layer_trace.h"
#include "../evgpu/lv_draw_evgpu_private.h"
#include <GLES2/gl2.h>

#ifndef GL_BGRA
    #ifdef GL_BGRA_EXT
        #define GL_BGRA GL_BGRA_EXT
    #else
        #define GL_BGRA 0x80E1
    #endif
#endif

static int32_t draw_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer);
static int32_t draw_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task);
static int32_t draw_delete(lv_draw_unit_t * draw_unit);
static void draw_event_cb(lv_event_t * e);

static void draw_execute(lv_draw_evgpu_c_r_t_unit_t * u, lv_draw_task_t * t);

static void on_layer_changed(lv_layer_t * new_layer);
static void on_layer_readback(lv_draw_evgpu_c_r_t_unit_t * u, lv_layer_t * layer);

void lv_draw_evgpu_c_r_t_init(void)
{
    lv_draw_evgpu_c_r_t_unit_t * unit = lv_draw_create_unit(sizeof(lv_draw_evgpu_c_r_t_unit_t));
    unit->base_unit.dispatch_cb = draw_dispatch;
    unit->base_unit.evaluate_cb = draw_evaluate;
    unit->base_unit.delete_cb = draw_delete;
    unit->base_unit.event_cb = draw_event_cb;
    unit->base_unit.name = "EVGPU_C_R_T";

    lv_evgpu_c_r_t_gl_init(&unit->gl);

    LV_LOG_INFO("DrawUnitEVGPU_C_R_T ready (unit_id=%d)", EVGPU_C_R_T_UNIT_ID);
}

void lv_evgpu_c_r_t_end_frame(lv_draw_evgpu_c_r_t_unit_t * u)
{
    lv_evgpu_c_r_t_gl_flush(&u->gl);
    glFlush();
}

void lv_evgpu_c_r_t_clean_up(lv_draw_evgpu_c_r_t_unit_t * u)
{
    LV_UNUSED(u);
    glFinish();
}

static void draw_execute(lv_draw_evgpu_c_r_t_unit_t * u, lv_draw_task_t * t)
{
    t->draw_unit = (lv_draw_unit_t *)u;

    lv_layer_t * layer = t->target_layer;

    lv_evgpu_c_r_t_gl_set_scissor(t->clip_area.x1, t->clip_area.y1,
                                   lv_area_get_width(&t->clip_area),
                                   lv_area_get_height(&t->clip_area));

    if(layer->draw_buf) {
        lv_draw_buf_set_flag(layer->draw_buf, LV_IMAGE_FLAGS_PREMULTIPLIED);
    }

#if LV_USE_PORT_LAYER_TRACE
    LV_PORT_LAYER_TRACE("L3-EVGPU_C_R_T", "execute task type=%d at (%d,%d)-(%d,%d)",
                        t->type,
                        (int)t->area.x1, (int)t->area.y1, (int)t->area.x2, (int)t->area.y2);
#endif

    switch(t->type) {
        case LV_DRAW_TASK_TYPE_FILL:
            lv_draw_evgpu_c_r_t_fill(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_BORDER:
            lv_draw_evgpu_c_r_t_border(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_BOX_SHADOW:
            lv_draw_evgpu_c_r_t_box_shadow(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_LETTER:
            lv_draw_evgpu_c_r_t_letter(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_LABEL:
            lv_draw_evgpu_c_r_t_label(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_IMAGE:
            lv_draw_evgpu_c_r_t_image(t, t->draw_dsc, &t->area, -1);
            break;

        case LV_DRAW_TASK_TYPE_LAYER:
            lv_draw_evgpu_c_r_t_layer(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_LINE:
            lv_draw_line_iterate(t, t->draw_dsc, lv_draw_evgpu_c_r_t_line);
            break;

        case LV_DRAW_TASK_TYPE_ARC:
            lv_draw_evgpu_c_r_t_arc(t, t->draw_dsc, &t->area);
            break;

        case LV_DRAW_TASK_TYPE_TRIANGLE:
            lv_draw_evgpu_c_r_t_triangle(t, t->draw_dsc);
            break;

        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE:
            lv_draw_evgpu_c_r_t_mask_rect(t, t->draw_dsc);
            break;

        case LV_DRAW_TASK_TYPE_BLUR:
            lv_draw_evgpu_c_r_t_blur(t, t->draw_dsc, &t->area);
            break;

#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR:
            lv_draw_evgpu_c_r_t_vector(t, t->draw_dsc);
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

        default:
            LV_LOG_ERROR("unknown draw task type: %d", t->type);
            break;
    }
}

static void on_layer_changed(lv_layer_t * new_layer)
{
    LV_PROFILER_DRAW_BEGIN;

    if(!new_layer->user_data) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_evgpu_c_r_t_fbo_t * fbo = (lv_evgpu_c_r_t_fbo_t *)new_layer->user_data;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    LV_PROFILER_DRAW_END;
}

static void on_layer_readback(lv_draw_evgpu_c_r_t_unit_t * u, lv_layer_t * layer)
{
    LV_PROFILER_DRAW_BEGIN;
    LV_ASSERT_NULL(u);
    LV_ASSERT_NULL(layer);

    lv_evgpu_c_r_t_fbo_t * fbo = (lv_evgpu_c_r_t_fbo_t *)layer->user_data;

    if(!fbo) {
        LV_LOG_WARN("No FBO available for layer: %p", (void *)layer);
        LV_PROFILER_DRAW_END;
        return;
    }

    if(!layer->draw_buf) {
        LV_LOG_WARN("No draw buffer available for layer: %p", (void *)layer);
        LV_PROFILER_DRAW_END;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);

    int32_t w = lv_area_get_width(&layer->buf_area);
    int32_t h = lv_area_get_height(&layer->buf_area);
    lv_draw_buf_t * draw_buf = layer->draw_buf;

    GLenum format;
    GLenum type;

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
        void * row = lv_draw_buf_goto_xy(draw_buf, 0, h - 1 - y);
        glReadPixels(0, y, w, 1, format, type, row);

        if(draw_buf->header.cf == LV_COLOR_FORMAT_RGB888) {
            lv_color_t * px = row;
            for(int32_t x = 0; x < w; x++) {
                uint8_t r = px->blue;
                px->blue = px->red;
                px->red = r;
                px++;
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    lv_draw_buf_flush_cache(draw_buf, NULL);

    LV_PROFILER_DRAW_END;
}

static int32_t draw_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)draw_unit;

    lv_draw_task_t * t = lv_draw_get_available_task(layer, NULL, EVGPU_C_R_T_UNIT_ID);
    if(!t || t->preferred_draw_unit_id != EVGPU_C_R_T_UNIT_ID) {
        lv_evgpu_c_r_t_end_frame(u);
        return LV_DRAW_UNIT_IDLE;
    }

    if(u->current_layer != layer) {
        lv_evgpu_c_r_t_end_frame(u);
        on_layer_changed(layer);
        u->current_layer = layer;
    }

    if(!u->is_started) {
        const int32_t buf_w = lv_area_get_width(&layer->buf_area);
        const int32_t buf_h = lv_area_get_height(&layer->buf_area);
        u->buf_w = buf_w;
        u->buf_h = buf_h;

        lv_evgpu_c_r_t_gl_set_projection(&u->gl, buf_w, buf_h);
        glViewport(0, 0, buf_w, buf_h);
        u->is_started = true;
    }

    t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;

    draw_execute(u, t);

    t->state = LV_DRAW_TASK_STATE_FINISHED;

    lv_draw_dispatch_request();

    return 1;
}

static int32_t draw_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task)
{
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
        case LV_DRAW_TASK_TYPE_BLUR:
            task->preference_score = 90;
            task->preferred_draw_unit_id = EVGPU_C_R_T_UNIT_ID;
            return 1;

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
#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR:
#endif
            return 0;

        default:
            return 0;
    }
}

static int32_t draw_delete(lv_draw_unit_t * draw_unit)
{
    lv_draw_evgpu_c_r_t_unit_t * unit = (lv_draw_evgpu_c_r_t_unit_t *)draw_unit;
    lv_evgpu_c_r_t_gl_deinit(&unit->gl);
    return 0;
}

static void draw_event_cb(lv_event_t * e)
{
    lv_draw_evgpu_c_r_t_unit_t * u = lv_event_get_current_target(e);
    lv_layer_t * layer = lv_event_get_param(e);

    switch(lv_event_get_code(e)) {
        case LV_EVENT_CANCEL:
            lv_evgpu_c_r_t_clean_up(u);
            break;

        case LV_EVENT_CHILD_CREATED: {
                lv_evgpu_c_r_t_fbo_t * fbo = lv_evgpu_c_r_t_fbo_create(
                    lv_area_get_width(&layer->buf_area),
                    lv_area_get_height(&layer->buf_area));
                layer->user_data = fbo;
            }
            break;

        case LV_EVENT_CHILD_DELETED: {
                lv_evgpu_c_r_t_fbo_t * fbo = (lv_evgpu_c_r_t_fbo_t *)layer->user_data;
                if(fbo) {
                    lv_evgpu_c_r_t_fbo_destroy(fbo);
                    layer->user_data = NULL;
                }
                if(u->current_layer == layer) {
                    u->current_layer = NULL;
                }
            }
            break;

        case LV_EVENT_SCREEN_LOAD_START:
            on_layer_readback(u, layer);
            break;

        case LV_EVENT_INVALIDATE_AREA:
            break;

        default:
            break;
    }
}

#endif
