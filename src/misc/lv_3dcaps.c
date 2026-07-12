/**
 * @file lv_3dcaps.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../include/lvgl/misc/lv_3dcaps.h"
#include "../lvgl_public.h"

#if LV_USE_3D_DRAW_TASKS

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

uint32_t lv_3dcaps_get(void)
{
    uint32_t caps = 0;
#if LV_USE_3DLIGHT
    caps |= LV_3D_CAP_PHONG;
#endif
#if LV_USE_3DMESH
    caps |= LV_3D_CAP_PICK | LV_3D_CAP_OBJ_LOADER;
#endif
    caps |= LV_3D_CAP_THEME;
    return caps;
}

bool lv_3dcaps_has(lv_3d_cap_flag_t flag)
{
    return (lv_3dcaps_get() & flag) != 0;
}

#endif /*LV_USE_3D_DRAW_TASKS*/
