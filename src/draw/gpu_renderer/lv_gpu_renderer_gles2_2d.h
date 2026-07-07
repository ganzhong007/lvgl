/**
 * @file lv_gpu_renderer_gles2_2d.h — GLES2 2D overlay batch [GL2]
 */

#ifndef LV_GPU_RENDERER_GLES2_2D_H
#define LV_GPU_RENDERER_GLES2_2D_H

#ifdef __cplusplus
extern "C" {
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "../../lv_conf_internal.h"

#include "../../misc/lv_area.h"
#include "../../misc/lv_color.h"
#include "../../include/lvgl/draw/lv_draw_label.h"
#include "../../include/lvgl/draw/lv_draw_image.h"
#include "../../include/lvgl/draw/lv_draw_rect.h"
#include "../../include/lvgl/draw/lv_draw_line.h"
#include "../../include/lvgl/draw/lv_draw_arc.h"
#include "../../include/lvgl/draw/lv_draw_triangle.h"
#include "../../include/lvgl/draw/lv_draw_mask.h"
#include "../../include/lvgl/draw/lv_draw_blur.h"
#if LV_USE_VECTOR_GRAPHIC
#include "../../include/lvgl/draw/lv_draw_vector.h"
#include "../../draw/lv_draw_vector_private.h"
#endif

#define LV_GPU_RENDERER_GLES2_LINE_PT_MAX 64

typedef enum {
    LV_GPU_RENDERER_GLES2_CMD_FILL = 0,
    LV_GPU_RENDERER_GLES2_CMD_BORDER,
    LV_GPU_RENDERER_GLES2_CMD_LABEL,
    LV_GPU_RENDERER_GLES2_CMD_LETTER,
    LV_GPU_RENDERER_GLES2_CMD_IMAGE,
    LV_GPU_RENDERER_GLES2_CMD_LINE,
    LV_GPU_RENDERER_GLES2_CMD_ARC,
    LV_GPU_RENDERER_GLES2_CMD_BOX_SHADOW,
    LV_GPU_RENDERER_GLES2_CMD_TRIANGLE,
    LV_GPU_RENDERER_GLES2_CMD_MASK_RECT,
    LV_GPU_RENDERER_GLES2_CMD_BLUR,
    LV_GPU_RENDERER_GLES2_CMD_VECTOR,
    LV_GPU_RENDERER_GLES2_CMD_MASK_BITMAP,
} lv_gpu_renderer_gles2_cmd_type_t;

typedef struct {
    lv_gpu_renderer_gles2_cmd_type_t type;
    lv_area_t area;
    lv_area_t clip;
    /**Copied polyline points when line dsc uses `points[]`. */
    lv_point_precise_t line_pts[LV_GPU_RENDERER_GLES2_LINE_PT_MAX];
    uint8_t line_pts_n;
    union {
        lv_draw_fill_dsc_t fill;
        struct {
            lv_color_t color;
            lv_opa_t opa;
            int32_t width;
            int32_t radius;
            lv_border_side_t side;
        } border;
        lv_draw_label_dsc_t label;
        lv_draw_letter_dsc_t letter;
        lv_draw_image_dsc_t image;
        lv_draw_line_dsc_t line;
        lv_draw_arc_dsc_t arc;
        lv_draw_box_shadow_dsc_t box_shadow;
        lv_draw_triangle_dsc_t triangle;
        lv_draw_mask_rect_dsc_t mask_rect;
        struct {
            lv_draw_blur_dsc_t blur;
            lv_area_t coords;
        } blur;
#if LV_USE_VECTOR_GRAPHIC
        lv_draw_vector_dsc_t vector;
#endif
        struct {
            const lv_image_dsc_t * mask_src;
            lv_area_t mask_area;
            lv_area_t blend_area;
        } mask_bitmap;
    } u;
} lv_gpu_renderer_gles2_cmd_t;

void lv_gpu_renderer_gles2_2d_init(void);
void lv_gpu_renderer_gles2_2d_deinit(void);

void lv_gpu_renderer_gles2_2d_queue_reset(void);
uint32_t lv_gpu_renderer_gles2_2d_queue_count(void);
uint32_t lv_gpu_renderer_gles2_2d_count_shader_batches(void);
bool lv_gpu_renderer_gles2_2d_is_raster_nest(void);

bool lv_gpu_renderer_gles2_2d_queue_fill(const lv_area_t * area, const lv_area_t * clip,
                                          lv_color_t color, lv_opa_t opa, int32_t radius);
bool lv_gpu_renderer_gles2_2d_queue_fill_dsc(const lv_area_t * area, const lv_area_t * clip,
                                              const lv_draw_fill_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_line(const lv_area_t * area, const lv_area_t * clip,
                                            const lv_draw_line_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_arc(const lv_area_t * area, const lv_area_t * clip,
                                         const lv_draw_arc_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_box_shadow(const lv_area_t * area, const lv_area_t * clip,
                                                const lv_draw_box_shadow_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_triangle(const lv_area_t * area, const lv_area_t * clip,
                                                const lv_draw_triangle_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_mask_rect(const lv_area_t * area, const lv_area_t * clip,
                                               const lv_draw_mask_rect_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_blur(const lv_area_t * area, const lv_area_t * clip,
                                          const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords);
bool lv_gpu_renderer_gles2_2d_queue_border(const lv_area_t * area, const lv_area_t * clip,
                                            lv_color_t color, lv_opa_t opa, int32_t width,
                                            int32_t radius, lv_border_side_t side);
bool lv_gpu_renderer_gles2_2d_queue_label(const lv_area_t * area, const lv_area_t * clip,
                                             const lv_draw_label_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_letter(const lv_area_t * area, const lv_area_t * clip,
                                              const lv_draw_letter_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_image(const lv_area_t * area, const lv_area_t * clip,
                                           const lv_draw_image_dsc_t * dsc);
#if LV_USE_VECTOR_GRAPHIC
bool lv_gpu_renderer_gles2_2d_queue_vector(const lv_area_t * area, const lv_area_t * clip,
                                              const lv_draw_vector_dsc_t * dsc);
#endif
bool lv_gpu_renderer_gles2_2d_queue_mask_bitmap(const lv_area_t * area, const lv_area_t * clip,
                                                   const lv_image_dsc_t * mask_src,
                                                   const lv_area_t * mask_area, const lv_area_t * blend_area);

/** Copy last queued cmd into out (for framegraph node storage). */
bool lv_gpu_renderer_gles2_2d_copy_last_cmd(lv_gpu_renderer_gles2_cmd_t * out);

uint32_t lv_gpu_renderer_gles2_2d_render_batch(unsigned int color_tex, int32_t dw, int32_t dh,
                                                uint32_t * sw_raster_out);

/** Render explicit cmd list (framegraph DAG execute). */
uint32_t lv_gpu_renderer_gles2_2d_render_cmd_list(const lv_gpu_renderer_gles2_cmd_t * cmds, uint32_t count,
                                                   unsigned int color_tex, int32_t dw, int32_t dh,
                                                   uint32_t * sw_raster_out);

/** GPU blit layer/source texture onto dst FBO with image_dsc transform (rotation/scale/skew). */
bool lv_gpu_renderer_gles2_2d_composite_layer(unsigned int dst_tex, unsigned int src_tex,
                                               int32_t src_w, int32_t src_h,
                                               const lv_draw_image_dsc_t * dsc, const lv_area_t * coords,
                                               int32_t dw, int32_t dh);

uint32_t lv_gpu_renderer_gles2_2d_glyph_overflow_count(void);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_GLES2_2D_H*/
