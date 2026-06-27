/**
 * @file lv_appwindow_private.h
 */

#ifndef LV_APPWINDOW_PRIVATE_H
#define LV_APPWINDOW_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../core/lv_obj_private.h"
#include "../../lvgl_public.h"

#if LV_USE_APPWINDOW

#include "../../include/lvgl/widgets/lv_appwindow.h"

#if LV_USE_SNAPSHOT
#include "../../include/lvgl/draw/lv_draw_buf.h"
#endif
#if LV_USE_3D && LV_USE_SNAPSHOT
#include "../../include/lvgl/3d/lv_3d_plane_bake.h"
#endif

typedef struct {
    lv_obj_t obj;
    lv_obj_t * content;
    lv_appwindow_state_t state;
#if LV_USE_SNAPSHOT
    lv_draw_buf_t * thumb_buf;
#endif
#if LV_USE_3D && LV_USE_SNAPSHOT
    lv_3d_snapshot_id_t snap_id;
#endif
    lv_appwindow_lifecycle_cb_t on_resume;
    lv_appwindow_lifecycle_cb_t on_pause;
    void * user_data;
} lv_appwindow_t;

extern const lv_obj_class_t lv_appwindow_class;

#endif /*LV_USE_APPWINDOW*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_APPWINDOW_PRIVATE_H*/
