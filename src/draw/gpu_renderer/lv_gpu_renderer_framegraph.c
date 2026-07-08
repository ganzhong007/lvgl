/**
 * @file lv_gpu_renderer_framegraph.c — DAG record / build / execute
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include <stdlib.h>

#include "lv_gpu_renderer_framegraph.h"
#include "lv_draw_gpu_renderer.h"

#include "lv_gpu_renderer_batch_3d.h"
#include "lv_gpu_renderer_gles2_2d.h"
#include "lv_gpu_renderer_gles2_3d.h"
#include "lv_gpu_renderer_layer.h"
#include "../../include/lvgl/draw/lv_draw_vector.h"
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
#include "../../include/lvgl/draw/lv_draw_line.h"
#include "../../include/lvgl/draw/lv_draw_arc.h"
#include "../../3d/lv_3d_internal.h"

#if LV_USE_3D && LV_USE_SNAPSHOT
#include "../../include/lvgl/3d/lv_3d_plane_bake.h"
#endif

#ifndef LV_GPU_RENDERER_FLUSH_MAX_BATCHES
    #define LV_GPU_RENDERER_FLUSH_MAX_BATCHES 8
#endif

#ifndef LV_GPU_FG_MAX_NODES
    #define LV_GPU_FG_MAX_NODES 640
#endif

#define LV_GPU_FG_MAX_VP 8

typedef struct {
    lv_obj_t * scene;
    lv_obj_t * camera;
    lv_area_t area;
} lv_gpu_fg_vp_t;

static lv_gpu_ui_mode_t g_ui_mode = LV_GPU_UI_MODE_GENERIC;
static lv_gpu_fg_node_t g_nodes[LV_GPU_FG_MAX_NODES];
static uint32_t g_node_count;
static uint32_t g_sorted_idx[LV_GPU_FG_MAX_NODES];
static uint32_t g_sorted_count;
static lv_gpu_fg_vp_t g_vp_last[LV_GPU_FG_MAX_VP];
static uint32_t g_vp_last_count;
/** Last scanout texture that received a full 3D viewport draw (or blit copy). */
static unsigned int g_last_drawn_3d_tex_id;
static lv_gpu_renderer_fg_stats_t g_fg_stats;
static uint32_t g_record_serial;

static bool is_display_fb_layer(const lv_layer_t * layer)
{
    lv_display_t * disp = lv_refr_get_disp_refreshing();
    if(!disp || !layer || !layer->draw_buf) return false;

    lv_opengles_texture_t * tex = lv_display_get_driver_data(disp);
    if(!tex || !tex->fb1) return false;

    return layer->draw_buf->data == tex->fb1;
}

static bool fg_is_gpu_2d_target(const lv_layer_t * layer)
{
    if(is_display_fb_layer(layer)) return true;
    return lv_gpu_renderer_layer_is_target(layer);
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

static bool fg_static_skip_allowed(void)
{
    const char * verify = getenv("LVGL_VERIFY");
    if(verify && verify[0] == '1') return false;
    return true;
}

static bool fg_space_enabled(lv_gpu_fg_space_t space)
{
    switch(g_ui_mode) {
        case LV_GPU_UI_MODE_APP_FULLSCREEN:
            return space == LV_GPU_FG_FULLSCREEN_APP || space == LV_GPU_FG_SCREEN;
        case LV_GPU_UI_MODE_WIREFRAME_BENCH:
            return space == LV_GPU_FG_VIEWPORT_3D || space == LV_GPU_FG_OVERLAY;
        case LV_GPU_UI_MODE_AR_LAUNCHER:
        case LV_GPU_UI_MODE_NAV_AR:
            return space != LV_GPU_FG_FULLSCREEN_APP;
        default:
            return space != LV_GPU_FG_FULLSCREEN_APP;
    }
}

static int fg_pass_rank(lv_gpu_fg_space_t space)
{
    switch(space) {
        case LV_GPU_FG_VIEWPORT_3D:
        case LV_GPU_FG_PLANE_3D:
            return 10;
        case LV_GPU_FG_SCREEN:
            return 20;
        case LV_GPU_FG_OVERLAY:
            return 30;
        case LV_GPU_FG_LAYER:
            return 40;
        case LV_GPU_FG_FULLSCREEN_APP:
            return 50;
        default:
            return 60;
    }
}

static bool fg_unified_pass_active(void)
{
    if(!lv_gpu_renderer_unified_pass_enabled()) return false;
    const lv_gpu_ui_mode_t mode = lv_gpu_renderer_fg_get_ui_mode();
    return mode == LV_GPU_UI_MODE_NAV_AR;
}

static bool fg_rank_is_overlay_only(int min_rank, int max_rank)
{
    if(min_rank < 0 || max_rank < 0) return false;
    const int overlay_rank = fg_pass_rank(LV_GPU_FG_OVERLAY);
    return min_rank >= overlay_rank && max_rank <= overlay_rank;
}

static uint32_t fg_shader_key_2d(const lv_gpu_renderer_gles2_cmd_t * cmd)
{
    if(!cmd) return 0;
    return cmd->type == LV_GPU_RENDERER_GLES2_CMD_FILL || cmd->type == LV_GPU_RENDERER_GLES2_CMD_BORDER ? 0U : 1U;
}

static bool fg_push_node(const lv_gpu_fg_node_t * node)
{
    if(g_node_count >= LV_GPU_FG_MAX_NODES) return false;
    g_nodes[g_node_count++] = *node;
    if(node->kind == LV_GPU_FG_NODE_VIEWPORT) g_fg_stats.gpu_3d_vp_recorded++;
    if(node->kind == LV_GPU_FG_NODE_2D) g_fg_stats.gpu_2d_recorded++;
    return true;
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
    g_node_count = 0;
    g_sorted_count = 0;
    g_record_serial = 0;
    g_last_drawn_3d_tex_id = 0;
    g_fg_stats.gpu_2d_recorded = 0;
    g_fg_stats.gpu_3d_vp_recorded = 0;
    lv_gpu_renderer_gles2_2d_queue_reset();
}

lv_gpu_fg_space_t lv_gpu_renderer_fg_classify_2d_space(const lv_draw_task_t * task)
{
    if(g_ui_mode == LV_GPU_UI_MODE_APP_FULLSCREEN) return LV_GPU_FG_FULLSCREEN_APP;

    const lv_draw_dsc_base_t * base = (const lv_draw_dsc_base_t *)task->draw_dsc;
    lv_obj_t * obj = base ? base->obj : NULL;
    if(obj) {
        lv_obj_t * scr = lv_obj_get_screen(obj);
        if(scr && obj->parent == scr) return LV_GPU_FG_OVERLAY;
    }
    return LV_GPU_FG_SCREEN;
}

bool lv_gpu_renderer_fg_record_viewport(lv_obj_t * scene, lv_obj_t * camera, const lv_area_t * area)
{
    if(!scene || !camera || !area) return false;

    for(uint32_t i = 0; i < g_node_count; i++) {
        lv_gpu_fg_node_t * n = &g_nodes[i];
        if(n->kind != LV_GPU_FG_NODE_VIEWPORT) continue;
        if(n->u.viewport.scene == scene && n->u.viewport.camera == camera
           && n->u.viewport.area.x1 == area->x1 && n->u.viewport.area.y1 == area->y1
           && n->u.viewport.area.x2 == area->x2 && n->u.viewport.area.y2 == area->y2) {
            return true;
        }
    }

    lv_gpu_fg_node_t node;
    lv_memzero(&node, sizeof(node));
    node.kind = LV_GPU_FG_NODE_VIEWPORT;
    node.space = LV_GPU_FG_VIEWPORT_3D;
    node.path = LV_GPU_PATH_3D_VIEWPORT;
    node.record_index = g_record_serial++;
    node.u.viewport.scene = scene;
    node.u.viewport.camera = camera;
    node.u.viewport.area = *area;
    return fg_push_node(&node);
}

bool lv_gpu_renderer_fg_record_2d_task(lv_draw_task_t * task)
{
    if(!task) return false;
    lv_gpu_renderer_gles2_cmd_t cmd;
    if(!lv_gpu_renderer_gles2_2d_copy_last_cmd(&cmd)) return false;

    if(lv_gpu_renderer_layer_is_target(task->target_layer)) {
        return lv_gpu_renderer_layer_push_cmd(task->target_layer, &cmd);
    }

    lv_gpu_fg_node_t node;
    lv_memzero(&node, sizeof(node));
    node.kind = LV_GPU_FG_NODE_2D;
    node.space = lv_gpu_renderer_fg_classify_2d_space(task);
    node.path = LV_GPU_PATH_2D_NATIVE;
    node.record_index = g_record_serial++;
    node.clip = task->clip_area;
    node.z_key = (uint32_t)task->area.y1;
    node.pixel_area = (uint32_t)lv_area_get_size(&task->area);
    node.u.cmd_2d = cmd;
    node.shader_key = fg_shader_key_2d(&node.u.cmd_2d);
    return fg_push_node(&node);
}

bool lv_gpu_renderer_fg_record_layer_task(lv_draw_task_t * task)
{
    lv_draw_image_dsc_t * id = lv_draw_task_get_image_dsc(task);
    if(!id) return false;

    lv_layer_t * sub = (lv_layer_t *)id->src;
    if(sub) {
        lv_gpu_renderer_layer_flush(sub);
    }

    lv_gpu_fg_node_t node;
    lv_memzero(&node, sizeof(node));
    node.kind = LV_GPU_FG_NODE_LAYER;
    node.space = LV_GPU_FG_LAYER;
    node.path = LV_GPU_PATH_DEFER_LAYER;
    node.record_index = g_record_serial++;
    node.clip = task->clip_area;
    node.pixel_area = (uint32_t)lv_area_get_size(&task->area);
    node.u.layer.image = *id;
    node.u.layer.area = task->area;
    node.u.layer.clip = task->clip_area;
    return fg_push_node(&node);
}

bool lv_gpu_renderer_fg_can_gpu_native_2d(const lv_draw_task_t * task)
{
    lv_draw_task_t * t = (lv_draw_task_t *)(uintptr_t)task;

    if(lv_gpu_renderer_gles2_2d_is_raster_nest()) return false;
    if(lv_refr_get_disp_refreshing() == NULL) return false;
    if(!fg_is_gpu_2d_target(task->target_layer)) return false;

    switch(task->type) {
        case LV_DRAW_TASK_TYPE_FILL: {
            lv_draw_fill_dsc_t * fd = lv_draw_task_get_fill_dsc(t);
            return fd != NULL;
        }
        case LV_DRAW_TASK_TYPE_BORDER: {
            lv_draw_border_dsc_t * bd = lv_draw_task_get_border_dsc(t);
            return bd && bd->width > 0;
        }
        case LV_DRAW_TASK_TYPE_LABEL: {
            lv_draw_label_dsc_t * ld = lv_draw_task_get_label_dsc(t);
            return ld != NULL;
        }
        case LV_DRAW_TASK_TYPE_LETTER: {
            const lv_draw_letter_dsc_t * ld = task->draw_dsc;
            return ld != NULL;
        }
        case LV_DRAW_TASK_TYPE_IMAGE: {
            lv_draw_image_dsc_t * id = lv_draw_task_get_image_dsc(t);
            if(!id) return false;
            const int32_t max_tex = lv_gpu_renderer_get_max_texture_size();
            if(max_tex > 0 && (id->header.w > max_tex || id->header.h > max_tex)) return false;
            return true;
        }
        case LV_DRAW_TASK_TYPE_LINE: {
            lv_draw_line_dsc_t * ld = lv_draw_task_get_line_dsc(t);
            return ld && ld->width > 0 && ld->opa > LV_OPA_MIN;
        }
        case LV_DRAW_TASK_TYPE_ARC: {
            lv_draw_arc_dsc_t * ad = lv_draw_task_get_arc_dsc(t);
            return ad && ad->width > 0 && ad->opa > LV_OPA_MIN;
        }
        case LV_DRAW_TASK_TYPE_BOX_SHADOW: {
            lv_draw_box_shadow_dsc_t * sd = lv_draw_task_get_box_shadow_dsc(t);
            return sd && sd->width > 0 && sd->opa > LV_OPA_MIN;
        }
        case LV_DRAW_TASK_TYPE_TRIANGLE: {
            lv_draw_triangle_dsc_t * td = lv_draw_task_get_triangle_dsc(t);
            return td && td->opa > LV_OPA_MIN;
        }
        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE: {
            lv_draw_mask_rect_dsc_t * md = lv_draw_task_get_mask_rect_dsc(t);
            return md && lv_color_format_has_alpha(task->target_layer->color_format);
        }
        case LV_DRAW_TASK_TYPE_BLUR: {
            lv_draw_blur_dsc_t * bd = lv_draw_task_get_blur_dsc(t);
            return bd && bd->blur_radius > 0;
        }
#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR:
            return true;
#endif
        case LV_DRAW_TASK_TYPE_MASK_BITMAP:
            return true;
        default:
            return false;
    }
}

bool lv_gpu_renderer_fg_can_gpu_layer(const lv_draw_task_t * task)
{
    if(task->type != LV_DRAW_TASK_TYPE_LAYER) return false;
    if(task->state == LV_DRAW_TASK_STATE_BLOCKED) return false;
    if(!fg_is_gpu_2d_target(task->target_layer)) return false;

    lv_draw_image_dsc_t * id = lv_draw_task_get_image_dsc(task);
    if(!id) return false;
    lv_layer_t * sub = (lv_layer_t *)id->src;
    if(!sub) return false;
    if(lv_gpu_renderer_layer_is_target(sub)) return true;
    if(!sub->draw_buf) return false;
    return true;
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

    if(lv_gpu_renderer_fg_can_gpu_layer(task)) {
        if(path_out) *path_out = LV_GPU_PATH_DEFER_LAYER;
        lv_draw_image_dsc_t * id = lv_draw_task_get_image_dsc(task);
        lv_layer_t * sub = id ? (lv_layer_t *)id->src : NULL;
        uint32_t area = sub ? (uint32_t)lv_area_get_size(&sub->buf_area) : 0;
        return 15 + (int32_t)(area >> 12);
    }

    if(lv_gpu_renderer_fg_can_gpu_native_2d(task)) {
        if(path_out) *path_out = LV_GPU_PATH_2D_NATIVE;
        return 20;
    }

    if(fg_is_gpu_2d_target(task->target_layer)) {
        const lv_draw_dsc_base_t * base = (const lv_draw_dsc_base_t *)task->draw_dsc;
        LV_UNUSED(base);

        const bool gpu_offscreen = lv_gpu_renderer_layer_is_target(task->target_layer)
                                   && !is_display_fb_layer(task->target_layer);
        const int32_t layer_score = gpu_offscreen ? 5 : 35;

        switch(task->type) {
            case LV_DRAW_TASK_TYPE_FILL:
            case LV_DRAW_TASK_TYPE_BORDER:
            case LV_DRAW_TASK_TYPE_LABEL:
            case LV_DRAW_TASK_TYPE_LETTER:
            case LV_DRAW_TASK_TYPE_IMAGE:
            case LV_DRAW_TASK_TYPE_LINE:
            case LV_DRAW_TASK_TYPE_ARC:
            case LV_DRAW_TASK_TYPE_BOX_SHADOW:
            case LV_DRAW_TASK_TYPE_TRIANGLE:
            case LV_DRAW_TASK_TYPE_MASK_RECTANGLE:
            case LV_DRAW_TASK_TYPE_BLUR:
#if LV_USE_VECTOR_GRAPHIC
            case LV_DRAW_TASK_TYPE_VECTOR:
#endif
            case LV_DRAW_TASK_TYPE_MASK_BITMAP:
                if(path_out) *path_out = LV_GPU_PATH_2D_RASTER;
                return layer_score;
            default:
                break;
        }
    }

    return 0;
}

bool lv_gpu_renderer_fg_has_pending(void)
{
    if(lv_gpu_renderer_debug_2d_only()) {
        return g_node_count > 0;
    }
    return g_node_count > 0;
}

bool lv_gpu_renderer_fg_has_restorable_viewport(void)
{
    return g_vp_last_count > 0;
}

bool lv_gpu_renderer_fg_restore_last_viewport(void)
{
    if(g_vp_last_count == 0 || g_vp_last_count > LV_GPU_FG_MAX_VP) return false;
    for(uint32_t i = 0; i < g_vp_last_count; i++) {
        lv_gpu_fg_vp_t * vp = &g_vp_last[i];
        lv_gpu_renderer_fg_record_viewport(vp->scene, vp->camera, &vp->area);
    }
    return true;
}

bool lv_gpu_renderer_fg_queue_2d_task(lv_draw_task_t * t)
{
    switch(t->type) {
        case LV_DRAW_TASK_TYPE_FILL: {
            lv_draw_fill_dsc_t * fd = lv_draw_task_get_fill_dsc(t);
            if(!fd) return false;
            return lv_gpu_renderer_gles2_2d_queue_fill_dsc(&t->area, &t->clip_area, fd);
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
        case LV_DRAW_TASK_TYPE_LINE: {
            lv_draw_line_dsc_t * ld = lv_draw_task_get_line_dsc(t);
            if(!ld) return false;
            return lv_gpu_renderer_gles2_2d_queue_line(&t->area, &t->clip_area, ld);
        }
        case LV_DRAW_TASK_TYPE_ARC: {
            lv_draw_arc_dsc_t * ad = lv_draw_task_get_arc_dsc(t);
            if(!ad) return false;
            return lv_gpu_renderer_gles2_2d_queue_arc(&t->area, &t->clip_area, ad);
        }
        case LV_DRAW_TASK_TYPE_BOX_SHADOW: {
            lv_draw_box_shadow_dsc_t * sd = lv_draw_task_get_box_shadow_dsc(t);
            if(!sd) return false;
            return lv_gpu_renderer_gles2_2d_queue_box_shadow(&t->area, &t->clip_area, sd);
        }
        case LV_DRAW_TASK_TYPE_TRIANGLE: {
            lv_draw_triangle_dsc_t * td = lv_draw_task_get_triangle_dsc(t);
            if(!td) return false;
            return lv_gpu_renderer_gles2_2d_queue_triangle(&t->area, &t->clip_area, td);
        }
        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE: {
            lv_draw_mask_rect_dsc_t * md = lv_draw_task_get_mask_rect_dsc(t);
            if(!md) return false;
            return lv_gpu_renderer_gles2_2d_queue_mask_rect(&t->area, &t->clip_area, md);
        }
        case LV_DRAW_TASK_TYPE_BLUR: {
            lv_draw_blur_dsc_t * bd = lv_draw_task_get_blur_dsc(t);
            if(!bd) return false;
            return lv_gpu_renderer_gles2_2d_queue_blur(&t->area, &t->clip_area, bd, &t->area);
        }
#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR: {
            lv_draw_vector_dsc_t * vd = lv_draw_task_get_vector_dsc(t);
            if(!vd) return false;
            return lv_gpu_renderer_gles2_2d_queue_vector(&t->area, &t->clip_area, vd);
        }
#endif
        case LV_DRAW_TASK_TYPE_MASK_BITMAP: {
            typedef struct {
                const lv_image_dsc_t * mask_src;
                lv_area_t mask_area;
                lv_area_t blend_area;
            } lv_gpu_mask_bitmap_dsc_t;
            const lv_gpu_mask_bitmap_dsc_t * md = (const lv_gpu_mask_bitmap_dsc_t *)t->draw_dsc;
            if(!md || !md->mask_src) return false;
            return lv_gpu_renderer_gles2_2d_queue_mask_bitmap(&t->area, &t->clip_area, md->mask_src,
                                                               &md->mask_area, &md->blend_area);
        }
        default:
            return false;
    }
}

static uint32_t fg_count_material_batches(const lv_3d_draw_item_t * items, uint32_t n)
{
    uint32_t opaque_order[LV_3D_MAX_DRAW_ITEMS];
    uint32_t opaque_count = 0;
    for(uint32_t i = 0; i < n && opaque_count < LV_3D_MAX_DRAW_ITEMS; i++) {
        if(items[i].wireframe || items[i].material.kind == LV_3D_MAT_WIREFRAME) continue;
        if(items[i].material.kind == LV_3D_MAT_ALPHA) continue;
        if(items[i].material.kind == LV_3D_MAT_PLANE_SNAPSHOT && items[i].snapshot_id != LV_3D_SNAPSHOT_ID_NONE) continue;
        opaque_order[opaque_count++] = i;
    }
    if(opaque_count == 0) return 0;
    lv_gpu_renderer_batch_3d_sort_opaque(items, n, opaque_order, opaque_count);
    return lv_gpu_renderer_batch_3d_count_material_runs(items, opaque_order, opaque_count);
}

void lv_gpu_renderer_fg_build(void)
{
    g_sorted_count = g_node_count;
    for(uint32_t i = 0; i < g_node_count; i++) g_sorted_idx[i] = i;

    for(uint32_t a = 1; a < g_sorted_count; a++) {
        const uint32_t key_i = g_sorted_idx[a];
        const lv_gpu_fg_node_t * key = &g_nodes[key_i];
        const int key_rank = fg_pass_rank(key->space);
        uint32_t b = a;
        while(b > 0) {
            const lv_gpu_fg_node_t * prev = &g_nodes[g_sorted_idx[b - 1]];
            const int prev_rank = fg_pass_rank(prev->space);
            if(prev_rank < key_rank) break;
            if(prev_rank == key_rank && prev->z_key <= key->z_key) break;
            if(prev_rank == key_rank && prev->z_key == key->z_key
               && prev->record_index <= key->record_index) break;
            g_sorted_idx[b] = g_sorted_idx[b - 1];
            b--;
        }
        g_sorted_idx[b] = key_i;
    }

    g_fg_stats.node_count = g_node_count;
}

static void fg_compute_energy(void)
{
    const uint32_t w1 = 4, w2 = 8, w3 = 1, w4 = 16, w5 = 1;
    g_fg_stats.energy_cost = w1 * g_fg_stats.draw_calls
                             + w2 * g_fg_stats.fbo_switches
                             + w3 * (g_fg_stats.sw_upload_bytes / 1024U)
                             + w4 * g_fg_stats.gl_finish_count
                             + w5 * (g_fg_stats.overdraw_pixels / 4096U);
}

void lv_gpu_renderer_fg_execute(unsigned int tex_id, int32_t dw, int32_t dh,
                                 uint32_t * gpu_2d_out, uint32_t * gpu_3d_out,
                                 uint32_t * sw_raster_out, uint8_t * max_alpha_out)
{
    g_fg_stats.pass_count = 0;
    g_fg_stats.batch_count = 0;
    g_fg_stats.material_batches = 0;
    g_fg_stats.gl_flush_count = 0;
    g_fg_stats.draw_calls = 0;
    g_fg_stats.fbo_switches = 0;
    g_fg_stats.sw_upload_bytes = 0;
    g_fg_stats.overdraw_pixels = 0;
    g_fg_stats.skipped_static_3d = 0;
    g_fg_stats.unified_overlay_merged = 0;

    if(g_node_count == 0) {
        lv_gpu_renderer_fg_restore_last_viewport();
    }

    lv_gpu_renderer_fg_build();

    const bool only2d = lv_gpu_renderer_debug_2d_only();
    if(tex_id == 0 || (only2d ? (g_node_count == 0) : (g_node_count == 0))) {
        if(gpu_2d_out) *gpu_2d_out = 0;
        if(gpu_3d_out) *gpu_3d_out = 0;
        if(sw_raster_out) *sw_raster_out = 0;
        return;
    }

#if LV_USE_3D && LV_USE_SNAPSHOT
    lv_3d_plane_upload_all();
#endif

    if(fg_unified_pass_active()) {
        /* Packed depth-stencil: depth for 3D, stencil for exact vector fills. */
        lv_gpu_renderer_tex_fbo_attach_depth(dw, dh);
    }
    else {
        /* 2D-only pass: attach a stencil buffer so vector paths fill exactly
         * (even-odd/nonzero) instead of falling back to earcut approximation. */
        lv_gpu_renderer_tex_fbo_attach_stencil(dw, dh);
    }

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
    uint32_t vp_saved = 0;

    lv_gpu_renderer_gles2_cmd_t cmd_batch[LV_GPU_FG_MAX_NODES];
    uint32_t cmd_batch_count = 0;
    int active_2d_rank = -1;
    int cmd_batch_min_rank = -1;
    int cmd_batch_max_rank = -1;
    bool viewport_drawn = false;

    for(uint32_t si = 0; si < g_sorted_count; si++) {
        const lv_gpu_fg_node_t * node = &g_nodes[g_sorted_idx[si]];
        if(!fg_space_enabled(node->space)) continue;

        if(node->kind == LV_GPU_FG_NODE_2D) {
            const int rank = fg_pass_rank(node->space);
            if(active_2d_rank >= 0 && rank != active_2d_rank && cmd_batch_count > 0) {
                g_fg_stats.pass_count++;
                uint32_t sw_r = 0;
                uint32_t rendered = lv_gpu_renderer_gles2_2d_render_cmd_list(cmd_batch, cmd_batch_count,
                                                                              tex_id, dw, dh, &sw_r);
                if(gpu_2d_out && *gpu_2d_out == 0) *gpu_2d_out = rendered;
                else if(gpu_2d_out) *gpu_2d_out += rendered;
                if(sw_raster_out) *sw_raster_out += sw_r;
                g_fg_stats.draw_calls += rendered;
                g_fg_stats.fbo_switches++;
                g_fg_stats.batch_count++;
                cmd_batch_count = 0;
                cmd_batch_min_rank = -1;
                cmd_batch_max_rank = -1;
            }
            active_2d_rank = rank;
            if(cmd_batch_min_rank < 0 || rank < cmd_batch_min_rank) cmd_batch_min_rank = rank;
            if(rank > cmd_batch_max_rank) cmd_batch_max_rank = rank;
            if(cmd_batch_count < LV_GPU_FG_MAX_NODES) {
                cmd_batch[cmd_batch_count++] = node->u.cmd_2d;
            }
            continue;
        }

        if(cmd_batch_count > 0) {
            g_fg_stats.pass_count++;
            uint32_t sw_r = 0;
            uint32_t rendered = lv_gpu_renderer_gles2_2d_render_cmd_list(cmd_batch, cmd_batch_count, tex_id, dw, dh, &sw_r);
            if(gpu_2d_out) *gpu_2d_out = rendered;
            if(sw_raster_out) *sw_raster_out = sw_r;
            g_fg_stats.draw_calls += rendered;
            g_fg_stats.fbo_switches++;
            g_fg_stats.batch_count++;
            cmd_batch_count = 0;
            active_2d_rank = -1;
        }

        if(node->kind == LV_GPU_FG_NODE_VIEWPORT) {
#if LV_USE_3D
            const lv_obj_t * scene = node->u.viewport.scene;
            const lv_obj_t * camera = node->u.viewport.camera;
            const bool volatile_scene = lv_3d_scene_has_volatile_meshes((lv_obj_t *)scene)
                                        || lv_3d_camera_is_dirty((lv_obj_t *)camera);

            if(fg_static_skip_allowed() && !volatile_scene && g_vp_last_count > 0) {
                if(g_last_drawn_3d_tex_id != 0 && g_last_drawn_3d_tex_id != tex_id) {
                    if(lv_gpu_renderer_blit_tex_to_tex(tex_id, g_last_drawn_3d_tex_id, dw, dh)) {
                        g_last_drawn_3d_tex_id = tex_id;
                        g_fg_stats.skipped_static_3d++;
                        g_fg_stats.pass_count++;
                        g_fg_stats.fbo_switches++;
                        g_fg_stats.draw_calls++;
                        continue;
                    }
                }
                else if(g_last_drawn_3d_tex_id == tex_id) {
                    g_fg_stats.skipped_static_3d++;
                    continue;
                }
            }

            lv_3d_draw_item_t items[LV_3D_MAX_DRAW_ITEMS];
            uint32_t n = lv_3d_scene_collect((lv_obj_t *)scene, items, LV_3D_MAX_DRAW_ITEMS);

            g_fg_stats.material_batches += fg_count_material_batches(items, n);
            g_fg_stats.batch_count++;
            g_fg_stats.pass_count++;

            float view[16], proj[16];
            lv_3d_camera_get_view_proj((lv_obj_t *)camera, dw, dh, view, proj);

            const lv_area_t * area = &node->u.viewport.area;
            int32_t vx = area->x1;
            int32_t vy = dh - area->y2 - 1;
            int32_t vw = lv_area_get_width(area);
            int32_t vh = lv_area_get_height(area);

            uint8_t vp_max_a = 0;
            lv_gpu_renderer_gles2_render_viewport(tex_id, 0, vx, vy, vw, vh,
                                                   view, proj, items, n,
                                                   LV_GPU_RENDERER_AR_PASSTHROUGH, &vp_max_a);
            g_last_drawn_3d_tex_id = tex_id;
            if(vp_max_a > frame_max_a) frame_max_a = vp_max_a;
            item_total += n;
            g_fg_stats.draw_calls += n;
            g_fg_stats.fbo_switches++;
            viewport_drawn = true;

            lv_3d_scene_clear_dirty((lv_obj_t *)scene);
            lv_3d_camera_clear_dirty((lv_obj_t *)camera);

            if(vp_saved < LV_GPU_FG_MAX_VP) {
                g_vp_last[vp_saved].scene = (lv_obj_t *)scene;
                g_vp_last[vp_saved].camera = (lv_obj_t *)camera;
                g_vp_last[vp_saved].area = *area;
                vp_saved++;
            }

            if(g_fg_stats.batch_count % LV_GPU_RENDERER_FLUSH_MAX_BATCHES == 0) {
                GL_CALL(glFlush());
                g_fg_stats.gl_flush_count++;
            }
#endif
        }
        else if(node->kind == LV_GPU_FG_NODE_LAYER) {
            g_fg_stats.pass_count++;
            lv_layer_t * sub = (lv_layer_t *)node->u.layer.image.src;
            if(sub && sub->draw_buf) {
                g_fg_stats.sw_upload_bytes += sub->draw_buf->data_size;
            }
            if(lv_gpu_renderer_composite_layer_to_tex(tex_id, &node->u.layer.image,
                                                        &node->u.layer.area, &node->u.layer.clip, dw, dh)) {
                g_fg_stats.draw_calls++;
                g_fg_stats.fbo_switches++;
                g_fg_stats.overdraw_pixels += node->pixel_area;
            }
        }
    }

    if(cmd_batch_count > 0) {
        const bool merge_overlay = fg_unified_pass_active() && viewport_drawn
                                   && fg_rank_is_overlay_only(cmd_batch_min_rank, cmd_batch_max_rank);
        if(!merge_overlay) {
            g_fg_stats.pass_count++;
        }
        else {
            g_fg_stats.unified_overlay_merged = 1;
        }
        uint32_t sw_r = 0;
        uint32_t rendered = lv_gpu_renderer_gles2_2d_render_cmd_list(cmd_batch, cmd_batch_count, tex_id, dw, dh, &sw_r);
        if(gpu_2d_out) {
            if(*gpu_2d_out == 0) *gpu_2d_out = rendered;
            else *gpu_2d_out += rendered;
        }
        if(sw_raster_out) *sw_raster_out += sw_r;
        g_fg_stats.draw_calls += rendered;
        if(!merge_overlay) {
            g_fg_stats.fbo_switches++;
        }
        g_fg_stats.batch_count++;
    }

    if(max_alpha_out) *max_alpha_out = frame_max_a;
    if(gpu_3d_out) *gpu_3d_out = item_total;

    GL_CALL(glFlush());
    g_fg_stats.gl_flush_count++;

    lv_gpu_renderer_restore_default_framebuffer();
    fg_compute_energy();

    g_vp_last_count = vp_saved;
    g_node_count = 0;
    g_sorted_count = 0;
    g_fg_stats.gpu_2d_recorded = 0;
    g_fg_stats.gpu_3d_vp_recorded = 0;
    lv_gpu_renderer_gles2_2d_queue_reset();
}

const lv_gpu_renderer_fg_stats_t * lv_gpu_renderer_fg_get_stats(void)
{
    return &g_fg_stats;
}

#endif /*LV_USE_DRAW_GPU_RENDERER*/
