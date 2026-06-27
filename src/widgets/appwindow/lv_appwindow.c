/**
 * @file lv_appwindow.c
 */

#include "lv_appwindow_private.h"
#include "../../include/lvgl/widgets/lv_appwindow.h"

#if LV_USE_APPWINDOW

#if !LV_USE_SNAPSHOT
    #error "LV_USE_APPWINDOW requires LV_USE_SNAPSHOT"
#endif

#include "../../core/lv_obj_class_private.h"

#if LV_USE_SNAPSHOT
#include "../../include/lvgl/draw/lv_snapshot.h"
#endif

#define MY_CLASS (&lv_appwindow_class)

static void lv_appwindow_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_appwindow_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static bool appwindow_capture_thumbnail(lv_appwindow_t * aw);

const lv_obj_class_t lv_appwindow_class = {
    .constructor_cb = lv_appwindow_constructor,
    .destructor_cb = lv_appwindow_destructor,
    .width_def = LV_DPI_DEF,
    .height_def = LV_DPI_DEF,
    .instance_size = sizeof(lv_appwindow_t),
    .base_class = &lv_obj_class,
    .name = "appwindow",
};

lv_obj_t * lv_appwindow_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

lv_obj_t * lv_appwindow_get_content(lv_obj_t * appwindow)
{
    LV_ASSERT_OBJ(appwindow, MY_CLASS);
    lv_appwindow_t * aw = (lv_appwindow_t *)appwindow;
    return aw->content;
}

lv_appwindow_state_t lv_appwindow_get_state(lv_obj_t * appwindow)
{
    LV_ASSERT_OBJ(appwindow, MY_CLASS);
    return ((lv_appwindow_t *)appwindow)->state;
}

bool lv_appwindow_is_resumed(lv_obj_t * appwindow)
{
    return lv_appwindow_get_state(appwindow) == LV_APPWINDOW_STATE_RESUMED;
}

void lv_appwindow_set_lifecycle_cb(lv_obj_t * appwindow,
                                   lv_appwindow_lifecycle_cb_t on_resume,
                                   lv_appwindow_lifecycle_cb_t on_pause,
                                   void * user_data)
{
    LV_ASSERT_OBJ(appwindow, MY_CLASS);
    lv_appwindow_t * aw = (lv_appwindow_t *)appwindow;
    aw->on_resume = on_resume;
    aw->on_pause = on_pause;
    aw->user_data = user_data;
}

void lv_appwindow_resume(lv_obj_t * appwindow)
{
    LV_ASSERT_OBJ(appwindow, MY_CLASS);
    lv_appwindow_t * aw = (lv_appwindow_t *)appwindow;

    if(aw->state == LV_APPWINDOW_STATE_RESUMED) return;

    lv_obj_remove_flag(appwindow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(appwindow);
    aw->state = LV_APPWINDOW_STATE_RESUMED;

    if(aw->on_resume) aw->on_resume(appwindow, aw->user_data);
    lv_obj_invalidate(appwindow);
}

void lv_appwindow_pause(lv_obj_t * appwindow)
{
    LV_ASSERT_OBJ(appwindow, MY_CLASS);
    lv_appwindow_t * aw = (lv_appwindow_t *)appwindow;

    if(aw->state == LV_APPWINDOW_STATE_PAUSED) return;

    appwindow_capture_thumbnail(aw);

    lv_obj_add_flag(appwindow, LV_OBJ_FLAG_HIDDEN);
    aw->state = LV_APPWINDOW_STATE_PAUSED;

    if(aw->on_pause) aw->on_pause(appwindow, aw->user_data);
}

bool lv_appwindow_capture_thumbnail(lv_obj_t * appwindow)
{
    LV_ASSERT_OBJ(appwindow, MY_CLASS);
    return appwindow_capture_thumbnail((lv_appwindow_t *)appwindow);
}

#if LV_USE_SNAPSHOT
const lv_draw_buf_t * lv_appwindow_get_thumbnail(lv_obj_t * appwindow)
{
    LV_ASSERT_OBJ(appwindow, MY_CLASS);
    lv_appwindow_t * aw = (lv_appwindow_t *)appwindow;
    return aw->thumb_buf;
}
#endif

#if LV_USE_3D && LV_USE_SNAPSHOT
lv_3d_snapshot_id_t lv_appwindow_get_snapshot_id(lv_obj_t * appwindow)
{
    LV_ASSERT_OBJ(appwindow, MY_CLASS);
    lv_appwindow_t * aw = (lv_appwindow_t *)appwindow;
    if(aw->snap_id == LV_3D_SNAPSHOT_ID_NONE && aw->content) {
        appwindow_capture_thumbnail(aw);
    }
    return aw->snap_id;
}
#endif

static void lv_appwindow_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_appwindow_t * aw = (lv_appwindow_t *)obj;

    aw->state = LV_APPWINDOW_STATE_CREATED;
#if LV_USE_SNAPSHOT
    aw->thumb_buf = NULL;
#endif
#if LV_USE_3D && LV_USE_SNAPSHOT
    aw->snap_id = LV_3D_SNAPSHOT_ID_NONE;
#endif
    aw->on_resume = NULL;
    aw->on_pause = NULL;
    aw->user_data = NULL;

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);

    aw->content = lv_obj_create(obj);
    lv_obj_set_size(aw->content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(aw->content, 0, 0);
    lv_obj_set_style_border_width(aw->content, 0, 0);
    lv_obj_remove_flag(aw->content, LV_OBJ_FLAG_SCROLLABLE);

    aw->state = LV_APPWINDOW_STATE_PAUSED;
}

static void lv_appwindow_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_appwindow_t * aw = (lv_appwindow_t *)obj;

#if LV_USE_SNAPSHOT
    if(aw->thumb_buf) {
        lv_draw_buf_destroy(aw->thumb_buf);
        aw->thumb_buf = NULL;
    }
#endif
#if LV_USE_3D && LV_USE_SNAPSHOT
    if(aw->content) lv_3d_plane_invalidate(aw->content);
    aw->snap_id = LV_3D_SNAPSHOT_ID_NONE;
#endif
}

static bool appwindow_capture_thumbnail(lv_appwindow_t * aw)
{
    if(!aw || !aw->content) return false;

    lv_obj_update_layout(aw->content);

#if LV_USE_SNAPSHOT
    lv_draw_buf_t * snap = lv_snapshot_take(aw->content, LV_COLOR_FORMAT_ARGB8888);
    if(!snap) return false;

    if(aw->thumb_buf) lv_draw_buf_destroy(aw->thumb_buf);
    aw->thumb_buf = snap;
#else
    return false;
#endif

#if LV_USE_3D && LV_USE_SNAPSHOT
    aw->snap_id = lv_3d_plane_bake(aw->content, LV_3D_PLANE_SRC_SNAPSHOT);
#endif

    return true;
}

#endif /*LV_USE_APPWINDOW*/
