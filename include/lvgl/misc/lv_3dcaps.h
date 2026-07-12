/**
 * @file lv_3dcaps.h
 *
 */

#ifndef LV_3DCAPS_H
#define LV_3DCAPS_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../config/lv_conf_internal.h"
#include "../lv_types.h"

#if LV_USE_3D_DRAW_TASKS

/*********************
 *      DEFINES
 *********************/

typedef enum {
    LV_3D_CAP_NONE       = 0,
    LV_3D_CAP_PHONG      = 1 << 0,
    LV_3D_CAP_PICK       = 1 << 1,
    LV_3D_CAP_OBJ_LOADER = 1 << 2,
} lv_3d_cap_flag_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

uint32_t lv_3dcaps_get(void);

bool lv_3dcaps_has(lv_3d_cap_flag_t flag);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_3DCAPS_H*/
