/**
 * @file lv_3dscene.h
 */

#ifndef LV_3DSCENE_H
#define LV_3DSCENE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../core/lv_obj.h"

lv_obj_t * lv_3dscene_create(lv_obj_t * parent);

#endif

#ifdef __cplusplus
}
#endif

#endif /*LV_3DSCENE_H*/
