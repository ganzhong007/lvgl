/**
 * @file lv_draw_evgpu_private.h
 *
 */

#ifndef LV_DRAW_EVGPU_PRIVATE_H
#define LV_DRAW_EVGPU_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

#include "../lv_draw_private.h"
#include "../../misc/lv_area_private.h"

#if !LV_USE_MATRIX
#error "Require LV_USE_MATRIX = 1"
#endif

#include "../../libs/evgpu/evgpu_evgr.h"
#include "lv_evgpu_context.h"
#include "lv_evgpu_shader.h"

struct _lv_evgpu_label_text_cache_t;

/*********************
 *      DEFINES
 *********************/

/* DrawUnitEVGPU is GLES2-only */
#if LV_EVGR_BACKEND != LV_EVGR_BACKEND_GLES2
#error "LV_USE_DRAW_EVGPU requires LV_EVGR_BACKEND_GLES2"
#endif
#define EVGR_GLES2_IMPLEMENTATION

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_pending_t;
struct EVGRLUframebuffer;
struct EVGRLUblurState;

typedef struct _lv_draw_evgpu_unit_t {
    lv_draw_unit_t base_unit;
    lv_layer_t * current_layer;
    EVGRcontext * evgr;
    bool is_started;
    lv_draw_buf_t * image_buf;

    lv_cache_t * image_cache;
    struct _lv_pending_t * image_pending;
    lv_ll_t image_drop_ll;
    const void * image_drop_src;

    lv_cache_t * letter_cache;
    struct _lv_pending_t * letter_pending;

    lv_cache_t * fbo_cache;

    lv_evgpu_context_t ctx;
    lv_evgpu_shader_t shader;

    struct _lv_evgpu_label_text_cache_t * label_text_cache;

    struct EVGRLUblurState * blur_state;
} lv_draw_evgpu_unit_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

#if LV_USE_3DTEXTURE
/**
 * Draw 3D texture on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a 3D draw descriptor
 * @param coords the coordinates of the 3D texture
 */
void lv_draw_evgpu_3d(lv_draw_task_t * t, const lv_draw_3d_dsc_t * dsc, const lv_area_t * coords);
#endif

#if LV_USE_3D_DRAW_TASKS
#include "../../include/lvgl/draw/lv_draw_3d_viewport.h"
#include "../../include/lvgl/draw/lv_draw_3d_clear.h"
void lv_draw_evgpu_3d_viewport(lv_draw_task_t * t, const lv_draw_3d_viewport_dsc_t * dsc, const lv_area_t * coords);
void lv_draw_evgpu_3d_clear(lv_draw_task_t * t, const lv_draw_3d_clear_dsc_t * dsc);
void lv_draw_evgpu_3d_line_init(void);
void lv_draw_evgpu_3d_line_deinit(void);
void lv_draw_evgpu_3d_line(lv_draw_task_t * t, const lv_draw_3d_line_dsc_t * dsc);
void lv_draw_evgpu_3d_cb_init(void);
void lv_draw_evgpu_3d_cb(lv_draw_task_t * t, const lv_draw_3d_callback_dsc_t * dsc);
void lv_draw_evgpu_3d_mesh_init(void);
void lv_draw_evgpu_3d_mesh_deinit(void);
void lv_draw_evgpu_3d_mesh(lv_draw_task_t * t, const lv_draw_3d_mesh_dsc_t * dsc);
#if LV_USE_GLTF
void lv_draw_evgpu_3d_scene_init(void);
void lv_draw_evgpu_3d_scene_deinit(void);
void lv_draw_evgpu_3d_scene(lv_draw_task_t * t, const lv_draw_3d_scene_dsc_t * dsc);
#endif
#endif

/**
 * Draw arc on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to an arc descriptor
 * @param coords the coordinates of the arc
 */
void lv_draw_evgpu_arc(lv_draw_task_t * t, const lv_draw_arc_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw border on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a border descriptor
 * @param coords the coordinates of the border
 */
void lv_draw_evgpu_border(lv_draw_task_t * t, const lv_draw_border_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw box on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a box descriptor
 * @param coords the coordinates of the box
 */
void lv_draw_evgpu_box_shadow(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc, const lv_area_t * coords);

/**
 * Fill a rectangle on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a fill descriptor
 * @param coords the coordinates of the rectangle
 */
void lv_draw_evgpu_fill(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw image on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to an image descriptor
 * @param coords the coordinates of the image
 * @param image_handle the handle of the image to draw
 */
void lv_draw_evgpu_image(lv_draw_task_t * t, const lv_draw_image_dsc_t * dsc, const lv_area_t * coords,
                          int image_handle);

/**
 * Initialize draw label on a EVGR context
 * @param u pointer to a EVGPU unit
 */
void lv_draw_evgpu_label_init(lv_draw_evgpu_unit_t * u);

/**
 * Deinitialize draw label on a EVGR context
 * @param u pointer to a EVGPU unit
 */
void lv_draw_evgpu_label_deinit(lv_draw_evgpu_unit_t * u);

/**
 * Draw letter on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a letter descriptor
 * @param coords the coordinates of the letter
 */
void lv_draw_evgpu_letter(lv_draw_task_t * t, const lv_draw_letter_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw label on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a label descriptor
 * @param coords the coordinates of the label
 */
void lv_draw_evgpu_label(lv_draw_task_t * t, const lv_draw_label_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw layer on a EVGR context
 * @param t pointer to a drawing task
 * @param draw_dsc pointer to an image descriptor
 * @param coords the coordinates of the layer
 */
void lv_draw_evgpu_layer(lv_draw_task_t * t, const lv_draw_image_dsc_t * draw_dsc, const lv_area_t * coords);

/**
 * Draw line on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a line descriptor
 */
void lv_draw_evgpu_line(lv_draw_task_t * t, const lv_draw_line_dsc_t * dsc);

/**
 * Draw triangle on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a triangle descriptor
 */
void lv_draw_evgpu_triangle(lv_draw_task_t * t, const lv_draw_triangle_dsc_t * dsc);

/**
 * Draw mask rectangles on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a mask descriptor
 */
void lv_draw_evgpu_mask_rect(lv_draw_task_t * t, const lv_draw_mask_rect_dsc_t * dsc);

/**
 * Get image handle from framebuffer
 * @param fb the framebuffer to get the image handle from
 * @return the image handle
 */
int lv_evgpu_fb_get_image_handle(struct EVGRLUframebuffer * fb);

/**
 * Initialize the blur draw unit state on the given EVGPU unit
 * @param u pointer to the EVGPU unit
 */
void lv_draw_evgpu_blur_init(lv_draw_evgpu_unit_t * u);

/**
 * Deinitialize the blur draw unit state on the given EVGPU unit
 * @param u pointer to the EVGPU unit
 */
void lv_draw_evgpu_blur_deinit(lv_draw_evgpu_unit_t * u);

/**
 * Apply a separable gaussian blur to the current layer using a fragment shader
 * @param t pointer to a drawing task
 * @param dsc pointer to a blur descriptor
 * @param coords the coordinates of the area to blur
 */
void lv_draw_evgpu_blur(lv_draw_task_t * t, const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords);

#if LV_USE_VECTOR_GRAPHIC
/**
 * Draw vector graphics on a EVGR context
 * @param t pointer to a drawing task
 * @param dsc pointer to a vector descriptor
 */
void lv_draw_evgpu_vector(lv_draw_task_t * t, const lv_draw_vector_dsc_t * dsc);

/**
 * @brief Convert a gradient to a paint
 * @param ctx the EVGR context
 * @param grad the gradient descriptor
 * @param paint the paint to store the result
 */
bool lv_evgpu_grad_to_paint(EVGRcontext * ctx, const lv_vector_gradient_t * grad, EVGRpaint * paint);

/**
 * @brief Draw a gradient
 * @param ctx the EVGR context
 * @param grad the gradient descriptor
 * @param winding the fill rule
 * @param composite_operation the blend mode
 */
void lv_evgpu_draw_grad(
    EVGRcontext * ctx,
    const lv_vector_gradient_t * grad,
    enum EVGRwinding winding,
    enum EVGRcompositeOperation composite_operation);

/**
 * @brief Draw a gradient with helper
 * @param ctx the EVGR context
 * @param area the area to draw the gradient on
 * @param grad_dsc the gradient descriptor
 * @param winding the fill rule
 * @param composite_operation the blend mode
 */
void lv_evgpu_draw_grad_helper(
    EVGRcontext * ctx,
    const lv_area_t * area,
    const lv_grad_dsc_t * grad_dsc,
    enum EVGRwinding winding,
    enum EVGRcompositeOperation composite_operation);

#endif /*LV_USE_VECTOR_GRAPHIC*/

/**********************
 *      MACROS
 **********************/

#endif /* LV_USE_DRAW_EVGPU */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_EVGPU_PRIVATE_H*/
