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

typedef enum {
    LV_GPU_RENDERER_GLES2_CMD_FILL = 0,
    LV_GPU_RENDERER_GLES2_CMD_BORDER,
    LV_GPU_RENDERER_GLES2_CMD_LABEL,
    LV_GPU_RENDERER_GLES2_CMD_LETTER,
    LV_GPU_RENDERER_GLES2_CMD_IMAGE,
} lv_gpu_renderer_gles2_cmd_type_t;

typedef struct {
    lv_gpu_renderer_gles2_cmd_type_t type;
    lv_area_t area;
    lv_area_t clip;
    union {
        struct {
            lv_color_t color;
            lv_opa_t opa;
            int32_t radius;
        } fill;
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
bool lv_gpu_renderer_gles2_2d_queue_border(const lv_area_t * area, const lv_area_t * clip,
                                            lv_color_t color, lv_opa_t opa, int32_t width,
                                            int32_t radius, lv_border_side_t side);
bool lv_gpu_renderer_gles2_2d_queue_label(const lv_area_t * area, const lv_area_t * clip,
                                             const lv_draw_label_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_letter(const lv_area_t * area, const lv_area_t * clip,
                                              const lv_draw_letter_dsc_t * dsc);
bool lv_gpu_renderer_gles2_2d_queue_image(const lv_area_t * area, const lv_area_t * clip,
                                           const lv_draw_image_dsc_t * dsc);

/** Copy last queued cmd into out (for framegraph node storage). */
bool lv_gpu_renderer_gles2_2d_copy_last_cmd(lv_gpu_renderer_gles2_cmd_t * out);

uint32_t lv_gpu_renderer_gles2_2d_render_batch(unsigned int color_tex, int32_t dw, int32_t dh,
                                                uint32_t * sw_raster_out);

/** Render explicit cmd list (framegraph DAG execute). */
uint32_t lv_gpu_renderer_gles2_2d_render_cmd_list(const lv_gpu_renderer_gles2_cmd_t * cmds, uint32_t count,
                                                   unsigned int color_tex, int32_t dw, int32_t dh,
                                                   uint32_t * sw_raster_out);

#endif /*LV_USE_DRAW_GPU_RENDERER*/

#ifdef __cplusplus
}
#endif

#endif /*LV_GPU_RENDERER_GLES2_2D_H*/
