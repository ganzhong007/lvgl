/**
 * @file lv_draw_g100_private.h
 *
 */

#ifndef LV_DRAW_G100_PRIVATE_H
#define LV_DRAW_G100_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100

#include "../lv_draw_private.h"
#include "../../misc/lv_area_private.h"

#if !LV_USE_NANOVG
#error "Require LV_USE_NANOVG = 1"
#endif

#if !LV_USE_MATRIX
#error "Require LV_USE_MATRIX = 1"
#endif

#include "../../libs/nanovg/nanovg.h"

/*********************
 *      DEFINES
 *********************/

/* Select NanoVG OpenGL backend based on LV_NANOVG_BACKEND */
#if LV_NANOVG_BACKEND == LV_NANOVG_BACKEND_GL2
#define NANOVG_GL2_IMPLEMENTATION
#elif LV_NANOVG_BACKEND == LV_NANOVG_BACKEND_GL3
#define NANOVG_GL3_IMPLEMENTATION
#elif LV_NANOVG_BACKEND == LV_NANOVG_BACKEND_GLES2
#define NANOVG_GLES2_IMPLEMENTATION
#elif LV_NANOVG_BACKEND == LV_NANOVG_BACKEND_GLES3
#define NANOVG_GLES3_IMPLEMENTATION
#else
#error "Invalid LV_NANOVG_BACKEND value"
#endif

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_pending_t;
struct NVGLUframebuffer;
struct NVGLUblurState;

typedef struct _lv_draw_g100_unit_t {
    lv_draw_unit_t base_unit;
    lv_layer_t * current_layer;
    NVGcontext * vg;
    bool is_started;
    lv_draw_buf_t * image_buf;

    lv_cache_t * image_cache;
    struct _lv_pending_t * image_pending;
    lv_ll_t image_drop_ll;
    const void * image_drop_src;

    lv_cache_t * letter_cache;
    struct _lv_pending_t * letter_pending;

    lv_cache_t * fbo_cache;

    struct NVGLUblurState * blur_state;
} lv_draw_g100_unit_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

#if LV_USE_3DTEXTURE
/**
 * Draw 3D texture on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a 3D draw descriptor
 * @param coords the coordinates of the 3D texture
 */
void lv_draw_g100_3d(lv_draw_task_t * t, const lv_draw_3d_dsc_t * dsc, const lv_area_t * coords);
#endif

/**
 * Draw arc on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to an arc descriptor
 * @param coords the coordinates of the arc
 */
void lv_draw_g100_arc(lv_draw_task_t * t, const lv_draw_arc_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw border on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a border descriptor
 * @param coords the coordinates of the border
 */
void lv_draw_g100_border(lv_draw_task_t * t, const lv_draw_border_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw box on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a box descriptor
 * @param coords the coordinates of the box
 */
void lv_draw_g100_box_shadow(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc, const lv_area_t * coords);

/**
 * Fill a rectangle on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a fill descriptor
 * @param coords the coordinates of the rectangle
 */
void lv_draw_g100_fill(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw image on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to an image descriptor
 * @param coords the coordinates of the image
 * @param image_handle the handle of the image to draw
 */
void lv_draw_g100_image(lv_draw_task_t * t, const lv_draw_image_dsc_t * dsc, const lv_area_t * coords,
                          int image_handle);

/**
 * Initialize draw label on a NanoVG context
 * @param u pointer to a NanoVG unit
 */
void lv_draw_g100_label_init(lv_draw_g100_unit_t * u);

/**
 * Deinitialize draw label on a NanoVG context
 * @param u pointer to a NanoVG unit
 */
void lv_draw_g100_label_deinit(lv_draw_g100_unit_t * u);

/**
 * Draw letter on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a letter descriptor
 * @param coords the coordinates of the letter
 */
void lv_draw_g100_letter(lv_draw_task_t * t, const lv_draw_letter_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw label on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a label descriptor
 * @param coords the coordinates of the label
 */
void lv_draw_g100_label(lv_draw_task_t * t, const lv_draw_label_dsc_t * dsc, const lv_area_t * coords);

/**
 * Draw layer on a NanoVG context
 * @param t pointer to a drawing task
 * @param draw_dsc pointer to an image descriptor
 * @param coords the coordinates of the layer
 */
void lv_draw_g100_layer(lv_draw_task_t * t, const lv_draw_image_dsc_t * draw_dsc, const lv_area_t * coords);

/**
 * Draw line on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a line descriptor
 */
void lv_draw_g100_line(lv_draw_task_t * t, const lv_draw_line_dsc_t * dsc);

/**
 * Draw triangle on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a triangle descriptor
 */
void lv_draw_g100_triangle(lv_draw_task_t * t, const lv_draw_triangle_dsc_t * dsc);

/**
 * Draw mask rectangles on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a mask descriptor
 */
void lv_draw_g100_mask_rect(lv_draw_task_t * t, const lv_draw_mask_rect_dsc_t * dsc);

/**
 * Get image handle from framebuffer
 * @param fb the framebuffer to get the image handle from
 * @return the image handle
 */
int lv_g100_fb_get_image_handle(struct NVGLUframebuffer * fb);

/**
 * Initialize the blur draw unit state on the given nanovg unit
 * @param u pointer to the nanovg unit
 */
void lv_draw_g100_blur_init(lv_draw_g100_unit_t * u);

/**
 * Deinitialize the blur draw unit state on the given nanovg unit
 * @param u pointer to the nanovg unit
 */
void lv_draw_g100_blur_deinit(lv_draw_g100_unit_t * u);

/**
 * Apply a separable gaussian blur to the current layer using a fragment shader
 * @param t pointer to a drawing task
 * @param dsc pointer to a blur descriptor
 * @param coords the coordinates of the area to blur
 */
void lv_draw_g100_blur(lv_draw_task_t * t, const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords);

#if LV_USE_VECTOR_GRAPHIC
/**
 * Draw vector graphics on a NanoVG context
 * @param t pointer to a drawing task
 * @param dsc pointer to a vector descriptor
 */
void lv_draw_g100_vector(lv_draw_task_t * t, const lv_draw_vector_dsc_t * dsc);

/**
 * @brief Convert a gradient to a paint
 * @param ctx the nanovg context
 * @param grad the gradient descriptor
 * @param paint the paint to store the result
 */
bool lv_g100_grad_to_paint(NVGcontext * ctx, const lv_vector_gradient_t * grad, NVGpaint * paint);

/**
 * @brief Draw a gradient
 * @param ctx the nanovg context
 * @param grad the gradient descriptor
 * @param winding the fill rule
 * @param composite_operation the blend mode
 */
void lv_g100_draw_grad(
    NVGcontext * ctx,
    const lv_vector_gradient_t * grad,
    enum NVGwinding winding,
    enum NVGcompositeOperation composite_operation);

/**
 * @brief Draw a gradient with helper
 * @param ctx the nanovg context
 * @param area the area to draw the gradient on
 * @param grad_dsc the gradient descriptor
 * @param winding the fill rule
 * @param composite_operation the blend mode
 */
void lv_g100_draw_grad_helper(
    NVGcontext * ctx,
    const lv_area_t * area,
    const lv_grad_dsc_t * grad_dsc,
    enum NVGwinding winding,
    enum NVGcompositeOperation composite_operation);

#endif /*LV_USE_VECTOR_GRAPHIC*/

/**********************
 *      MACROS
 **********************/

#endif /* LV_USE_DRAW_G100 */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_G100_PRIVATE_H*/
