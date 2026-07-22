#ifndef LV_DRAW_EVGPU_C_R_T_PRIVATE_H
#define LV_DRAW_EVGPU_C_R_T_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU_C_R_T

#include "../lv_draw_private.h"
#include "../../misc/lv_area_private.h"
#include "lv_evgpu_c_r_t_gl.h"

typedef struct _lv_draw_evgpu_c_r_t_unit_t {
    lv_draw_unit_t base_unit;
    lv_layer_t * current_layer;
    bool is_started;
    int32_t buf_w;
    int32_t buf_h;

    lv_evgpu_c_r_t_gl_t gl;

    lv_cache_t * fbo_cache;
    lv_cache_t * tex_cache;
} lv_draw_evgpu_c_r_t_unit_t;

void lv_draw_evgpu_c_r_t_fill(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_border(lv_draw_task_t * t, const lv_draw_border_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_box_shadow(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_letter(lv_draw_task_t * t, const lv_draw_letter_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_label(lv_draw_task_t * t, const lv_draw_label_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_image(lv_draw_task_t * t, const lv_draw_image_dsc_t * dsc, const lv_area_t * coords, int image_handle);
void lv_draw_evgpu_c_r_t_layer(lv_draw_task_t * t, const lv_draw_image_dsc_t * draw_dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_line(lv_draw_task_t * t, const lv_draw_line_dsc_t * dsc);
void lv_draw_evgpu_c_r_t_arc(lv_draw_task_t * t, const lv_draw_arc_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_triangle(lv_draw_task_t * t, const lv_draw_triangle_dsc_t * dsc);
void lv_draw_evgpu_c_r_t_mask_rect(lv_draw_task_t * t, const lv_draw_mask_rect_dsc_t * dsc);
void lv_draw_evgpu_c_r_t_blur(lv_draw_task_t * t, const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_gradient(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords);

#if LV_USE_VECTOR_GRAPHIC
void lv_draw_evgpu_c_r_t_vector(lv_draw_task_t * t, const lv_draw_vector_dsc_t * dsc);
#endif

#if LV_USE_3DTEXTURE
void lv_draw_evgpu_c_r_t_3d(lv_draw_task_t * t, const lv_draw_3d_dsc_t * dsc, const lv_area_t * coords);
#endif

#if LV_USE_3D_DRAW_TASKS
#include "../../include/lvgl/draw/lv_draw_3d_viewport.h"
#include "../../include/lvgl/draw/lv_draw_3d_clear.h"
#include "../../include/lvgl/draw/lv_draw_3d_line.h"
#include "../../include/lvgl/draw/lv_draw_3d_callback.h"
#include "../../include/lvgl/draw/lv_draw_3d_mesh.h"
#if LV_USE_GLTF
#include "../../include/lvgl/draw/lv_draw_3d_scene.h"
#endif

void lv_draw_evgpu_c_r_t_3d_viewport(lv_draw_task_t * t, const lv_draw_3d_viewport_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_c_r_t_3d_clear(lv_draw_task_t * t, const lv_draw_3d_clear_dsc_t * dsc);
void lv_draw_evgpu_c_r_t_3d_line_init(void);
void lv_draw_evgpu_c_r_t_3d_line_deinit(void);
void lv_draw_evgpu_c_r_t_3d_line(lv_draw_task_t * t, const lv_draw_3d_line_dsc_t * dsc);
void lv_draw_evgpu_c_r_t_3d_cb_init(void);
void lv_draw_evgpu_c_r_t_3d_cb(lv_draw_task_t * t, const lv_draw_3d_callback_dsc_t * dsc);
void lv_draw_evgpu_c_r_t_3d_mesh_init(void);
void lv_draw_evgpu_c_r_t_3d_mesh_deinit(void);
void lv_draw_evgpu_c_r_t_3d_mesh(lv_draw_task_t * t, const lv_draw_3d_mesh_dsc_t * dsc);
#if LV_USE_GLTF
void lv_draw_evgpu_c_r_t_3d_scene_init(void);
void lv_draw_evgpu_c_r_t_3d_scene_deinit(void);
void lv_draw_evgpu_c_r_t_3d_scene(lv_draw_task_t * t, const lv_draw_3d_scene_dsc_t * dsc);
#endif
#endif /* LV_USE_3D_DRAW_TASKS */

void lv_evgpu_c_r_t_end_frame(lv_draw_evgpu_c_r_t_unit_t * u);
void lv_evgpu_c_r_t_clean_up(lv_draw_evgpu_c_r_t_unit_t * u);

#endif

#ifdef __cplusplus
}
#endif

#endif
