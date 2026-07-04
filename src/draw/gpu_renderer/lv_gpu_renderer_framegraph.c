/**
 * @file lv_gpu_renderer_framegraph.c
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "lv_gpu_renderer_framegraph.h"
#include "lv_draw_gpu_renderer.h"

#include "lv_gpu_renderer_batch_3d.h"
#include "lv_gpu_renderer_gles2_2d.h"
#include "lv_gpu_renderer_gles2_3d.h"
#include "../../core/lv_refr_private.h"
#include "../../core/lv_obj_class_private.h"
#include "../../core/lv_obj_private.h"
#include "../../display/lv_display_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_texture_private.h"
#include "../../include/lvgl/draw/lv_draw_label.h"
#include "../../include/lvgl/draw/lv_draw_image.h"
#include "../../include/lvgl/draw/lv_draw_rect.h"
#include "../../3d/lv_3d_internal.h"

#if LV_USE_3D && LV_USE_SNAPSHOT
#include "../../include/lvgl/3d/lv_3d_plane_bake.h"
#endif

#ifndef LV_GPU_RENDERER_FLUSH_MAX_BATCHES
    #define LV_GPU_RENDERER_FLUSH_MAX_BATCHES 8
#endif

#define LV_GPU_FG_MAX_VP 8

typedef struct {
    lv_obj_t * scene;
    lv_obj_t * camera;
    lv_area_t area;
} lv_gpu_fg_vp_t;

static lv_gpu_ui_mode_t g_ui_mode = LV_GPU_UI_MODE_GENERIC;
static lv_gpu_fg_vp_t g_vp_queue[LV_GPU_FG_MAX_VP];
static uint32_t g_vp_count;
static lv_gpu_fg_vp_t g_vp_last[LV_GPU_FG_MAX_VP];
static uint32_t g_vp_last_count;
static lv_gpu_renderer_fg_stats_t g_fg_stats;

static bool is_display_fb_layer(const lv_layer_t * layer)
{
    lv_display_t * disp = lv_refr_get_disp_refreshing();
    if(!disp || !layer || !layer->draw_buf) return false;

    lv_opengles_texture_t * tex = lv_display_get_driver_data(disp);
    if(!tex || !tex->fb1) return false;

    return layer->draw_buf->data == tex->fb1;
}

static bool item_is_transparent_3d(const lv_3d_draw_item_t * it)
{
    if(it->wireframe || it->material.kind == LV_3D_MAT_WIREFRAME) return true;
    if(it->material.kind == LV_3D_MAT_ALPHA) return true;
    if(it->material.kind == LV_3D_MAT_PLANE_SNAPSHOT && it->snapshot_id != LV_3D_SNAPSHOT_ID_NONE) return true;
    return false;
}

static uint32_t fg_count_material_batches(const lv_3d_draw_item_t * items, uint32_t n)
{
    uint32_t opaque_order[LV_3D_MAX_DRAW_ITEMS];
    uint32_t opaque_count = 0;
    for(uint32_t i = 0; i < n && opaque_count < LV_3D_MAX_DRAW_ITEMS; i++) {
        if(item_is_transparent_3d(&items[i])) continue;
        opaque_order[opaque_count++] = i;
    }
    if(opaque_count == 0) return 0;
    lv_gpu_renderer_batch_3d_sort_opaque(items, n, opaque_order, opaque_count);
    return lv_gpu_renderer_batch_3d_count_material_runs(items, opaque_order, opaque_count);
}

static bool pass_3d_enabled(void)
{
    if(lv_gpu_renderer_debug_2d_only()) {
        return false;
    }
    switch(g_ui_mode) {
        case LV_GPU_UI_MODE_APP_FULLSCREEN:
            return false;
        default:
            return true;
    }
}

static bool pass_2d_overlay_enabled(void)
{
    switch(g_ui_mode) {
        case LV_GPU_UI_MODE_APP_FULLSCREEN:
            return false;
        default:
            return true;
    }
}

void lv_gpu_renderer_fg_init(void)
{
    lv_gpu_renderer_fg_record_reset();
    lv_memzero(&g_fg_stats, sizeof(g_fg_stats));
}

void lv_gpu_renderer_fg_deinit(void)
{
    lv_gpu_renderer_fg_record_reset();
}

void lv_gpu_renderer_fg_set_ui_mode(lv_gpu_ui_mode_t mode)
{
    g_ui_mode = mode;
}

lv_gpu_ui_mode_t lv_gpu_renderer_fg_get_ui_mode(void)
{
    return g_ui_mode;
}

void lv_gpu_renderer_fg_record_reset(void)
{
    g_vp_count = 0;
    g_fg_stats.gpu_2d_recorded = 0;
    g_fg_stats.gpu_3d_vp_recorded = 0;
}

bool lv_gpu_renderer_fg_record_viewport(lv_obj_t * scene, lv_obj_t * camera, const lv_area_t * area)
{
    if(!scene || !camera || !area) return false;
    if(g_vp_count >= LV_GPU_FG_MAX_VP) return false;

    for(uint32_t i = 0; i < g_vp_count; i++) {
        lv_gpu_fg_vp_t * q = &g_vp_queue[i];
        if(q->scene == scene && q->camera == camera
           && q->area.x1 == area->x1 && q->area.y1 == area->y1
           && q->area.x2 == area->x2 && q->area.y2 == area->y2) {
            return true;
        }
    }

    g_vp_queue[g_vp_count].scene = scene;
    g_vp_queue[g_vp_count].camera = camera;
    g_vp_queue[g_vp_count].area = *area;
    g_vp_count++;
    g_fg_stats.gpu_3d_vp_recorded++;
    return true;
}

bool lv_gpu_renderer_fg_record_2d_task(lv_draw_task_t * task)
{
    LV_UNUSED(task);
    g_fg_stats.gpu_2d_recorded++;
    return true;
}

static bool gpu_obj_is_3d_logical(const lv_obj_t * obj)
{
    if(!obj) return false;
    for(const lv_obj_class_t * c = obj->class_p; c; c = c->base_class) {
        if(!c->name) continue;
        if(!lv_strcmp(c->name, "3dscene")) return true;
        if(!lv_strcmp(c->name, "3dstack")) return true;
        if(!lv_strcmp(c->name, "3dcamera")) return true;
        if(!lv_strcmp(c->name, "3dviewport")) return true;
        if(!lv_strcmp(c->name, "3dmesh")) return true;
    }
    return false;
}

bool lv_gpu_renderer_fg_can_gpu_native_2d(const lv_draw_task_t * task)
{
    lv_draw_task_t * t = (lv_draw_task_t *)(uintptr_t)task;

    if(lv_gpu_renderer_gles2_2d_is_raster_nest()) return false;
    if(lv_refr_get_disp_refreshing() == NULL) return false;
    if(!is_display_fb_layer(task->target_layer)) return false;

    {
        const lv_draw_dsc_base_t * base = (const lv_draw_dsc_base_t *)t->draw_dsc;
        if(base && gpu_obj_is_3d_logical(base->obj)) return false;
    }

    switch(task->type) {
        case LV_DRAW_TASK_TYPE_FILL: {
            lv_draw_fill_dsc_t * fd = lv_draw_task_get_fill_dsc(t);
            if(!fd || fd->grad.dir != LV_GRAD_DIR_NONE) return false;
            if(fd->opa > LV_OPA_80) {
                lv_color32_t c = lv_color_to_32(fd->color, fd->opa);
                if(c.red > 240 && c.green > 240 && c.blue > 240) return false;
            }
            return true;
        }
        case LV_DRAW_TASK_TYPE_BORDER: {
            lv_draw_border_dsc_t * bd = lv_draw_task_get_border_dsc(t);
            return bd && bd->width > 0;
        }
        case LV_DRAW_TASK_TYPE_LABEL: {
            lv_draw_label_dsc_t * ld = lv_draw_task_get_label_dsc(t);
            return ld && ld->rotation == 0;
        }
        case LV_DRAW_TASK_TYPE_LETTER: {
            const lv_draw_letter_dsc_t * ld = task->draw_dsc;
            return ld && ld->rotation == 0;
        }
        case LV_DRAW_TASK_TYPE_IMAGE: {
            lv_draw_image_dsc_t * id = lv_draw_task_get_image_dsc(t);
            return id && id->rotation == 0 && id->scale_x == LV_SCALE_NONE && id->scale_y == LV_SCALE_NONE
                   && id->skew_x == 0 && id->skew_y == 0 && id->blend_mode == LV_BLEND_MODE_NORMAL;
        }
        default:
            return false;
    }
}

int32_t lv_gpu_renderer_fg_evaluate_score(lv_draw_task_t * task, lv_gpu_renderer_path_t * path_out)
{
    if(path_out) *path_out = LV_GPU_PATH_NONE;
    if(lv_refr_get_disp_refreshing() == NULL) return 0;

#if LV_USE_3D
    if(task->type == LV_DRAW_TASK_TYPE_3D) {
        lv_draw_3d_dsc_t * dsc = lv_draw_task_get_3d_dsc(task);
        if(dsc && dsc->kind == LV_3D_DRAW_KIND_VIEWPORT_PASS && dsc->scene && dsc->camera) {
            if(path_out) *path_out = LV_GPU_PATH_3D_VIEWPORT;
            return 10;
        }
    }
#endif

    if(lv_gpu_renderer_fg_can_gpu_native_2d(task)) {
        if(path_out) *path_out = LV_GPU_PATH_2D_NATIVE;
        return 20;
    }

    return 0;
}

bool lv_gpu_renderer_fg_has_pending(void)
{
    if(lv_gpu_renderer_debug_2d_only()) {
        return lv_gpu_renderer_gles2_2d_queue_count() > 0;
    }
    return g_vp_count > 0 || lv_gpu_renderer_gles2_2d_queue_count() > 0;
}

bool lv_gpu_renderer_fg_has_restorable_viewport(void)
{
    return g_vp_last_count > 0;
}

bool lv_gpu_renderer_fg_restore_last_viewport(void)
{
    if(g_vp_last_count == 0 || g_vp_last_count > LV_GPU_FG_MAX_VP) return false;
    lv_memcpy(g_vp_queue, g_vp_last, g_vp_last_count * sizeof(g_vp_queue[0]));
    g_vp_count = g_vp_last_count;
    return true;
}

bool lv_gpu_renderer_fg_queue_2d_task(lv_draw_task_t * t)
{
    switch(t->type) {
        case LV_DRAW_TASK_TYPE_FILL: {
            lv_draw_fill_dsc_t * fd = lv_draw_task_get_fill_dsc(t);
            if(!fd) return false;
            return lv_gpu_renderer_gles2_2d_queue_fill(&t->area, &t->clip_area, fd->color, fd->opa, fd->radius);
        }
        case LV_DRAW_TASK_TYPE_BORDER: {
            lv_draw_border_dsc_t * bd = lv_draw_task_get_border_dsc(t);
            if(!bd) return false;
            return lv_gpu_renderer_gles2_2d_queue_border(&t->area, &t->clip_area, bd->color, bd->opa, bd->width,
                                                          bd->radius, bd->side);
        }
        case LV_DRAW_TASK_TYPE_LABEL: {
            lv_draw_label_dsc_t * ld = lv_draw_task_get_label_dsc(t);
            if(!ld) return false;
            return lv_gpu_renderer_gles2_2d_queue_label(&t->area, &t->clip_area, ld);
        }
        case LV_DRAW_TASK_TYPE_LETTER: {
            lv_draw_letter_dsc_t * ld = (lv_draw_letter_dsc_t *)t->draw_dsc;
            if(!ld) return false;
            return lv_gpu_renderer_gles2_2d_queue_letter(&t->area, &t->clip_area, ld);
        }
        case LV_DRAW_TASK_TYPE_IMAGE: {
            lv_draw_image_dsc_t * id = lv_draw_task_get_image_dsc(t);
            if(!id) return false;
            return lv_gpu_renderer_gles2_2d_queue_image(&t->area, &t->clip_area, id);
        }
        default:
            return false;
    }
}

void lv_gpu_renderer_fg_execute(unsigned int tex_id, int32_t dw, int32_t dh,
                                 uint32_t * gpu_2d_out, uint32_t * gpu_3d_out,
                                 uint32_t * sw_raster_out, uint8_t * max_alpha_out)
{
    g_fg_stats.pass_count = 0;
    g_fg_stats.batch_count = 0;
    g_fg_stats.material_batches = 0;
    g_fg_stats.gl_flush_count = 0;
    g_fg_stats.gl_finish_count = 0;

    if(g_vp_count == 0) {
        lv_gpu_renderer_fg_restore_last_viewport();
    }

    uint32_t q2d = lv_gpu_renderer_gles2_2d_queue_count();
    const bool only2d = lv_gpu_renderer_debug_2d_only();
    if(tex_id == 0 || (only2d ? (q2d == 0) : (g_vp_count == 0 && q2d == 0))) {
        if(gpu_2d_out) *gpu_2d_out = 0;
        if(gpu_3d_out) *gpu_3d_out = 0;
        if(sw_raster_out) *sw_raster_out = 0;
        return;
    }

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

    uint32_t item_total = 0;
    uint8_t frame_max_a = 0;

    if(pass_3d_enabled() && g_vp_count > 0) {
        g_fg_stats.pass_count++;
#if LV_USE_3D
        for(uint32_t v = 0; v < g_vp_count; v++) {
            lv_gpu_fg_vp_t * vp = &g_vp_queue[v];
            lv_3d_draw_item_t items[LV_3D_MAX_DRAW_ITEMS];
            uint32_t n = lv_3d_scene_collect(vp->scene, items, LV_3D_MAX_DRAW_ITEMS);

            g_fg_stats.material_batches += fg_count_material_batches(items, n);
            g_fg_stats.batch_count++;

            float view[16], proj[16];
            lv_3d_camera_get_view_proj(vp->camera, dw, dh, view, proj);

            int32_t vx = vp->area.x1;
            int32_t vy = dh - vp->area.y2 - 1;
            int32_t vw = lv_area_get_width(&vp->area);
            int32_t vh = lv_area_get_height(&vp->area);

            uint8_t vp_max_a = 0;
            lv_gpu_renderer_gles2_render_viewport(tex_id, 0, vx, vy, vw, vh,
                                                   view, proj, items, n,
                                                   LV_GPU_RENDERER_AR_PASSTHROUGH, &vp_max_a);
            if(vp_max_a > frame_max_a) frame_max_a = vp_max_a;
            item_total += n;

            if(g_fg_stats.batch_count % LV_GPU_RENDERER_FLUSH_MAX_BATCHES == 0) {
                GL_CALL(glFlush());
                g_fg_stats.gl_flush_count++;
            }
        }
#endif
    }

    if(pass_2d_overlay_enabled() && q2d > 0) {
        g_fg_stats.pass_count++;
        if(only2d) {
            /* Debug backdrop: distinguish uninitialized tex from 2D output. */
            lv_gpu_renderer_clear_tex_for_debug(tex_id, dw, dh);
        }
        uint32_t sw_raster = 0;
        uint32_t rendered = lv_gpu_renderer_gles2_2d_render_batch(tex_id, dw, dh, &sw_raster);
        g_fg_stats.batch_count += lv_gpu_renderer_gles2_2d_count_shader_batches();
        if(gpu_2d_out) *gpu_2d_out = rendered;
        if(sw_raster_out) *sw_raster_out = sw_raster;
        lv_gpu_renderer_gles2_2d_queue_reset();
    }
    else {
        if(gpu_2d_out) *gpu_2d_out = 0;
        if(sw_raster_out) *sw_raster_out = 0;
    }

    if(max_alpha_out) *max_alpha_out = frame_max_a;
    if(gpu_3d_out) *gpu_3d_out = item_total;

    /* glFlush only — caller syncs once after overlay (e.g. DRM dma-buf present). */
    GL_CALL(glFlush());
    g_fg_stats.gl_flush_count++;

    /* 2D/3D batch may leave FBO bound to the display texture; restore for window blit. */
    lv_gpu_renderer_restore_default_framebuffer();

    if(g_vp_count > 0) {
        lv_memcpy(g_vp_last, g_vp_queue, g_vp_count * sizeof(g_vp_queue[0]));
        g_vp_last_count = g_vp_count;
    }

    g_vp_count = 0;
    g_fg_stats.gpu_2d_recorded = 0;
    g_fg_stats.gpu_3d_vp_recorded = 0;
}

const lv_gpu_renderer_fg_stats_t * lv_gpu_renderer_fg_get_stats(void)
{
    return &g_fg_stats;
}

#endif /*LV_USE_DRAW_GPU_RENDERER*/
