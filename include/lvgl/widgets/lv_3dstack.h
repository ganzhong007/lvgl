/**
 * @file lv_3dstack.h
 */

#ifndef LV_3DSTACK_H
#define LV_3DSTACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D && LV_USE_3DSTACK

#include "../core/lv_obj.h"

lv_obj_t * lv_3dstack_create(lv_obj_t * parent);
void lv_3dstack_set_grid(lv_obj_t * stack, uint8_t cols, uint8_t rows);
void lv_3dstack_set_row_depth(lv_obj_t * stack, uint8_t row, float z);
void lv_3dstack_set_cell_spacing(lv_obj_t * stack, float x_gap, float y_gap);
void lv_3dstack_layout(lv_obj_t * stack);

#endif

#ifdef __cplusplus
}
#endif

#endif /*LV_3DSTACK_H*/
