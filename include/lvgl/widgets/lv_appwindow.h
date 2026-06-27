/**
 * @file lv_appwindow.h
 * App UI container for launcher-style multi-app UI (Activity-like subtree + thumbnail).
 */

#ifndef LV_APPWINDOW_H
#define LV_APPWINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../config/lv_conf_internal.h"
#include "../core/lv_obj.h"

#if LV_USE_APPWINDOW

#if LV_USE_SNAPSHOT
#include "../draw/lv_draw.h"
#endif
#if LV_USE_3D && LV_USE_SNAPSHOT
#include "../3d/lv_3d_plane_bake.h"
#endif

/*********************
 *      DEFINES
 *********************/

LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_appwindow_class;

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    LV_APPWINDOW_STATE_CREATED,
    LV_APPWINDOW_STATE_RESUMED,
    LV_APPWINDOW_STATE_PAUSED,
} lv_appwindow_state_t;

typedef void (*lv_appwindow_lifecycle_cb_t)(lv_obj_t * appwindow, void * user_data);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create an app window (hidden by default; build UI under lv_appwindow_get_content()).
 */
lv_obj_t * lv_appwindow_create(lv_obj_t * parent);

/**
 * Content container for application widgets (labels, buttons, …).
 */
lv_obj_t * lv_appwindow_get_content(lv_obj_t * appwindow);

lv_appwindow_state_t lv_appwindow_get_state(lv_obj_t * appwindow);
bool lv_appwindow_is_resumed(lv_obj_t * appwindow);

/**
 * Show content and mark as foreground app. Calls optional on_resume callback.
 */
void lv_appwindow_resume(lv_obj_t * appwindow);

/**
 * Capture thumbnail from content, hide window, mark paused. Calls optional on_pause callback.
 */
void lv_appwindow_pause(lv_obj_t * appwindow);

/**
 * Capture content into thumbnail buffer (and plane snapshot when 3D bake is enabled).
 * Can be called while paused (e.g. before first launcher tile display).
 */
bool lv_appwindow_capture_thumbnail(lv_obj_t * appwindow);

#if LV_USE_SNAPSHOT
/** Last captured thumbnail pixels, or NULL if none. Do not free; owned by appwindow. */
const lv_draw_buf_t * lv_appwindow_get_thumbnail(lv_obj_t * appwindow);
#endif

#if LV_USE_3D && LV_USE_SNAPSHOT
/** Snapshot id for 3D PLANE tiles; LV_3D_SNAPSHOT_ID_NONE if unavailable. */
lv_3d_snapshot_id_t lv_appwindow_get_snapshot_id(lv_obj_t * appwindow);
#endif

void lv_appwindow_set_lifecycle_cb(lv_obj_t * appwindow,
                                  lv_appwindow_lifecycle_cb_t on_resume,
                                  lv_appwindow_lifecycle_cb_t on_pause,
                                  void * user_data);

#endif /*LV_USE_APPWINDOW*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_APPWINDOW_H*/
