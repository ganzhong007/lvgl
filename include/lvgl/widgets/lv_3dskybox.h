/**
 * @file lv_3dskybox.h
 *
 * 3D Skybox widget - 6-face cubemap background for 3D viewports.
 *
 * A skybox is rendered as a cube centered on the camera, with the
 * camera always at the cube's center. As the camera rotates, the skybox
 * appears infinitely far away (parallax-free).
 *
 * Three rendering modes are supported:
 *   - CUBEMAP     : 6 separate 2D textures (one per cube face)
 *   - EQUIRECT    : a single 2:1 equirectangular panorama texture
 *   - SOLID_COLOR : flat background (no texture, just clear color)
 *
 * Example:
 *
 *   lv_3dskybox_t * sky = lv_3dskybox_create(NULL);
 *   lv_3dskybox_set_cubemap(sky, px_id, nx_id, py_id, ny_id, pz_id, nz_id);
 *   lv_3dskybox_set_size(sky, 1000.0f);     // 1000-unit cube
 *   lv_3dskybox_submit(my_viewport);        // draw it before other 3D objects
 */

#ifndef LV_3DSKYBOX_H
#define LV_3DSKYBOX_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../config/lv_conf_internal.h"
#if LV_USE_3DVIEWPORT

#include "../core/lv_obj.h"
#include "../lv_types.h"          /* for lv_3dtexture_id_t, lv_3dpoint_t, lv_3dray_t */
#include "../draw/lv_draw_3d_camera.h"

/*********************
 *      DEFINES
 *********************/

#ifndef LV_3DSKYBOX_DEFAULT_SIZE
#define LV_3DSKYBOX_DEFAULT_SIZE   1000.0f
#endif

/**********************
 *      TYPEDEFS
 ***********************

 * Skybox rendering mode.
 * CUBEMAP    : 6 separate 2D textures, one per cube face (+X,-X,+Y,-Y,+Z,-Z)
 * EQUIRECT   : a single 2:1 equirectangular panorama texture
 * SOLID_COLOR : a flat color (set via lv_3dskybox_set_color); no textures needed
 */
typedef enum {
    LV_3DSKYBOX_MODE_CUBEMAP = 0,
    LV_3DSKYBOX_MODE_EQUIRECT,
    LV_3DSKYBOX_MODE_SOLID_COLOR,
} lv_3dskybox_mode_t;

LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_3dskybox_class;

typedef struct _lv_3dskybox {
    lv_obj_t obj;          /* base widget */
} lv_3dskybox_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a 3D skybox widget.
 * The skybox defaults to CUBEMAP mode, size = LV_3DSKYBOX_DEFAULT_SIZE,
 * no textures set (will render as a colored cube using the viewport's
 * clear color until textures are assigned).
 * @param parent    parent object (NULL for free-standing)
 * @return          new skybox widget
 */
lv_obj_t * lv_3dskybox_create(lv_obj_t * parent);

/**
 * Set the rendering mode.
 * @param obj    a lv_3dskybox widget
 * @param mode   one of LV_3DSKYBOX_MODE_CUBEMAP / EQUIRECT / SOLID_COLOR
 */
void lv_3dskybox_set_mode(lv_obj_t * obj, lv_3dskybox_mode_t mode);

/**
 * Get the rendering mode.
 * @param obj    a lv_3dskybox widget
 * @return       current mode
 */
lv_3dskybox_mode_t lv_3dskybox_get_mode(const lv_obj_t * obj);

/**
 * Set the cubemap textures (one per face).
 * The textures are LVGL 3D-texture handles (e.g. OpenGL texture IDs).
 * Face order follows the OpenGL cubemap convention:
 *   0 = +X (right)  1 = -X (left)  2 = +Y (top)
 *   3 = -Y (bottom) 4 = +Z (front) 5 = -Z (back)
 * Only takes effect when mode = LV_3DSKYBOX_MODE_CUBEMAP.
 * @param obj          a lv_3dskybox widget
 * @param px_id        +X face texture id
 * @param nx_id        -X face texture id
 * @param py_id        +Y face texture id
 * @param ny_id        -Y face texture id
 * @param pz_id        +Z face texture id
 * @param nz_id        -Z face texture id
 */
void lv_3dskybox_set_cubemap(lv_obj_t * obj,
                              lv_3dtexture_id_t px_id,
                              lv_3dtexture_id_t nx_id,
                              lv_3dtexture_id_t py_id,
                              lv_3dtexture_id_t ny_id,
                              lv_3dtexture_id_t pz_id,
                              lv_3dtexture_id_t nz_id);

/**
 * Set the equirectangular panorama texture.
 * Only takes effect when mode = LV_3DSKYBOX_MODE_EQUIRECT.
 * @param obj     a lv_3dskybox widget
 * @param tex_id  equirect texture id (2:1 aspect ratio)
 */
void lv_3dskybox_set_equirect(lv_obj_t * obj, lv_3dtexture_id_t tex_id);

/**
 * Set the solid background color (for mode = LV_3DSKYBOX_MODE_SOLID_COLOR).
 * @param obj    a lv_3dskybox widget
 * @param color  background color
 * @param opa    opacity (0..255)
 */
void lv_3dskybox_set_color(lv_obj_t * obj, lv_color_t color, lv_opa_t opa);

/**
 * Set the cube size (edge length). The skybox is rendered as a cube
 * centered on the camera with this edge length. The default is
 * LV_3DSKYBOX_DEFAULT_SIZE (1000). Larger values push the sky farther
 * from the camera and reduce parallax artifacts.
 * @param obj   a lv_3dskybox widget
 * @param size  cube edge length in world units
 */
void lv_3dskybox_set_size(lv_obj_t * obj, float size);

/**
 * Get the cube size.
 * @param obj   a lv_3dskybox widget
 * @return      current edge length
 */
float lv_3dskybox_get_size(const lv_obj_t * obj);

/**
 * Submit the skybox for rendering into a 3D pass layer.
 * Typically called by the viewport's render callback BEFORE rendering
 * the rest of the scene, so the skybox acts as a background.
 * @param root       a skybox widget (or any object in the skybox tree)
 * @param pass_layer the 3D pass layer to draw into
 */
void lv_3dskybox_submit(lv_obj_t * root, lv_layer_t * pass_layer);

/**
 * Convenience: render the skybox in a viewport's render callback.
 * The skybox will use the viewport's current camera to anchor itself.
 * @param obj   a lv_3dskybox widget
 * @param cb    viewport render callback to register the skybox into
 */
void lv_3dskybox_attach(lv_obj_t * obj, lv_draw_3d_cb_t cb, void * user_data);

#endif /*LV_USE_3DVIEWPORT*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DSKYBOX_H*/
