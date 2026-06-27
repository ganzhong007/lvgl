/**
 * @file lv_3d_material.c
 */

#include "../../include/lvgl/3d/lv_3d.h"

#if LV_USE_3D

void lv_3d_material_init(lv_3d_material_t * mat, lv_3d_material_kind_t kind, lv_color_t color, lv_opa_t opa)
{
    mat->kind = kind;
    mat->color = color;
    mat->opa = opa;
}

#endif /*LV_USE_3D*/
