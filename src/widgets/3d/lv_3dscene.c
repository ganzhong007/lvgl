/**
 * @file lv_3dscene.c
 */

#include "../../include/lvgl/widgets/lv_3dscene.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../../core/lv_obj_class_private.h"

const lv_obj_class_t lv_3dscene_class = {
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .base_class = &lv_obj_class,
    .name = "3dscene",
};

lv_obj_t * lv_3dscene_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(&lv_3dscene_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/
