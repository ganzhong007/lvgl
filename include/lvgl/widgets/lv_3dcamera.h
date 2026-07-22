/**
 * @file lv_3dcamera.h
 *
 * Independent 3D camera widget. Wraps the existing lv_3d_camera_t structure
 * from lv_draw_3d_camera.h into a widget so it can be shared across multiple
 * viewports and configured declaratively.
 *
 * The widget stores an lv_3d_camera_t internally and provides a getter
 * `lv_3dcamera_get_raw()` so draw layer code (viewport, viewport render
 * callback, mesh shader) can use it without knowing it's a widget.
 *
 * Example:
 *
 *   lv_3dcamera_t * cam = lv_3dcamera_create(NULL);
 *   lv_3dcamera_set_perspective(cam, 60.0f, 0.1f, 100.0f);
 *   lv_3dcamera_look_at(cam,
 *       0.0f, 0.0f, 5.0f,    // eye
 *       0.0f, 0.0f, 0.0f);   // target
 *
 *   lv_3dviewport_set_camera(my_viewport, cam);
 */

#ifndef LV_3DCAMERA_H
#define LV_3DCAMERA_H

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

#ifndef LV_3DCAMERA_DEFAULT_FOV_DEG
#define LV_3DCAMERA_DEFAULT_FOV_DEG   60.0f
#endif

#ifndef LV_3DCAMERA_DEFAULT_NEAR_Z
#define LV_3DCAMERA_DEFAULT_NEAR_Z    0.1f
#endif

#ifndef LV_3DCAMERA_DEFAULT_FAR_Z
#define LV_3DCAMERA_DEFAULT_FAR_Z     100.0f
#endif

/**********************
 *      TYPEDEFS
 ***********************

 * lv_3dcamera projection modes.
 * PERSPECTIVE : standard pinhole, fov_y in degrees
 * ORTHOGRAPHIC : parallel projection, frustum defined by half-extents
 */
typedef enum {
    LV_3DCAMERA_PROJ_PERSPECTIVE = 0,
    LV_3DCAMERA_PROJ_ORTHOGRAPHIC,
} lv_3dcamera_projection_t;

LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_3dcamera_class;

/* Inherit all fields of lv_3d_camera_t via embedding */
typedef struct _lv_3dcamera {
    lv_obj_t obj;          /* base widget */
    lv_3d_camera_t cam;    /* the underlying camera data (yaw/pitch/distance/fov/near/far) */
} lv_3dcamera_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a 3D camera widget.
 * The camera defaults to a perspective projection with FOV=60 deg,
 * near=0.1, far=100, at world origin looking down -Z.
 * @param parent    parent object (NULL to create as a free-standing data object)
 * @return          the new camera widget
 */
lv_obj_t * lv_3dcamera_create(lv_obj_t * parent);

/**
 * Get the raw lv_3d_camera_t pointer for use by 3D draw code.
 * @param obj    a lv_3dcamera widget
 * @return       pointer to the underlying lv_3d_camera_t (read-only expected
 *               for clients, but the camera widget itself may mutate it)
 */
const lv_3d_camera_t * lv_3dcamera_get_raw(const lv_obj_t * obj);

/**
 * Set a perspective (pinhole) projection.
 * @param obj       a lv_3dcamera widget
 * @param fov_y_deg vertical field of view in degrees
 * @param near_z    near clip plane Z (positive, > 0)
 * @param far_z     far clip plane Z (positive, > near_z)
 */
void lv_3dcamera_set_perspective(lv_obj_t * obj, float fov_y_deg,
                                  float near_z, float far_z);

/**
 * Set an orthographic (parallel) projection.
 * @param obj          a lv_3dcamera widget
 * @param half_width   frustum half-width (X extent)
 * @param half_height  frustum half-height (Y extent)
 * @param near_z       near clip plane Z
 * @param far_z        far clip plane Z
 */
void lv_3dcamera_set_orthographic(lv_obj_t * obj,
                                   float half_width, float half_height,
                                   float near_z, float far_z);

/**
 * Get the current projection mode.
 * @param obj    a lv_3dcamera widget
 * @return       LV_3DCAMERA_PROJ_PERSPECTIVE or LV_3DCAMERA_PROJ_ORTHOGRAPHIC
 */
lv_3dcamera_projection_t lv_3dcamera_get_projection(const lv_obj_t * obj);

/**
 * Set the camera position (eye) and look-at target.
 * @param obj              a lv_3dcamera widget
 * @param eye_x/y/z        world-space eye position
 * @param target_x/y/z     world-space target (look-at) position
 * @param up_x/y/z         world-space up vector (typically (0,1,0))
 */
void lv_3dcamera_look_at(lv_obj_t * obj,
                          float eye_x, float eye_y, float eye_z,
                          float target_x, float target_y, float target_z,
                          float up_x, float up_y, float up_z);

/**
 * Orbit the camera around the current target.
 * @param obj       a lv_3dcamera widget
 * @param yaw       rotation around world up (degrees)
 * @param pitch     rotation around horizontal axis (degrees, -90..+90)
 * @param distance  eye-to-target distance
 */
void lv_3dcamera_orbit(lv_obj_t * obj, float yaw, float pitch, float distance);

/**
 * Set the world-space target the camera looks at.
 * @param obj         a lv_3dcamera widget
 * @param target      new target point
 */
void lv_3dcamera_set_target(lv_obj_t * obj, lv_3dpoint_t target);

/**
 * Get the camera's current eye (position) in world space.
 * @param obj    a lv_3dcamera widget
 * @param eye    output: filled with the eye position
 */
void lv_3dcamera_get_eye(const lv_obj_t * obj, lv_3dpoint_t * eye);

/**
 * Compute the current model-view-projection matrix for the camera.
 * Writes 16 floats (column-major 4x4) into mvp_out.
 * @param obj       a lv_3dcamera widget
 * @param w          viewport width in pixels
 * @param h          viewport height in pixels
 * @param mvp_out    output buffer of at least LV_3D_CAMERA_MVP_SIZE floats
 */
void lv_3dcamera_compute_mvp(const lv_obj_t * obj, int32_t w, int32_t h,
                              float mvp_out[LV_3D_CAMERA_MVP_SIZE]);

/**
 * Compute a world-space ray from a screen-space pixel.
 * The ray can be used for picking or for ray-mesh intersection.
 * @param obj    a lv_3dcamera widget
 * @param w      viewport width in pixels
 * @param h      viewport height in pixels
 * @param px     screen-space x (0..w-1)
 * @param py     screen-space y (0..h-1)
 * @param ray    output: filled with origin + direction
 */
void lv_3dcamera_get_ray(const lv_obj_t * obj, int32_t w, int32_t h,
                          int32_t px, int32_t py, lv_3dray_t * ray);

#endif /*LV_USE_3DVIEWPORT*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DCAMERA_H*/
