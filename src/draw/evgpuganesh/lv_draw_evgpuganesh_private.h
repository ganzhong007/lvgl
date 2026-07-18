#ifndef LV_DRAW_EVGPUGANESH_PRIVATE_H
#define LV_DRAW_EVGPUGANESH_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPUGANESH

#include "../lv_draw_private.h"
#include "lv_evgpuganesh_gl.h"

typedef struct _lv_draw_evgpuganesh_unit_t {
    lv_draw_unit_t base_unit;
    lv_layer_t * current_layer;
    bool is_started;
    int32_t buf_w;
    int32_t buf_h;

    lv_evgpuganesh_gl_t gl;

    lv_cache_t * fbo_cache;
    lv_cache_t * tex_cache;
} lv_draw_evgpuganesh_unit_t;

void lv_draw_evgpuganesh_fill(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpuganesh_border(lv_draw_task_t * t, const lv_draw_border_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpuganesh_box_shadow(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpuganesh_letter(lv_draw_task_t * t, const lv_draw_letter_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpuganesh_label(lv_draw_task_t * t, const lv_draw_label_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpuganesh_image(lv_draw_task_t * t, const lv_draw_image_dsc_t * dsc, const lv_area_t * coords, int image_handle);
void lv_draw_evgpuganesh_layer(lv_draw_task_t * t, const lv_draw_image_dsc_t * draw_dsc, const lv_area_t * coords);
void lv_draw_evgpuganesh_line(lv_draw_task_t * t, const lv_draw_line_dsc_t * dsc);
void lv_draw_evgpuganesh_arc(lv_draw_task_t * t, const lv_draw_arc_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpuganesh_triangle(lv_draw_task_t * t, const lv_draw_triangle_dsc_t * dsc);
void lv_draw_evgpuganesh_mask_rect(lv_draw_task_t * t, const lv_draw_mask_rect_dsc_t * dsc);
void lv_draw_evgpuganesh_blur(lv_draw_task_t * t, const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpuganesh_gradient(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords);

#if LV_USE_VECTOR_GRAPHIC
void lv_draw_evgpuganesh_vector(lv_draw_task_t * t, const lv_draw_vector_dsc_t * dsc);
#endif

void lv_evgpuganesh_end_frame(lv_draw_evgpuganesh_unit_t * u);
void lv_evgpuganesh_clean_up(lv_draw_evgpuganesh_unit_t * u);

#endif

#ifdef __cplusplus
}
#endif

#endif
