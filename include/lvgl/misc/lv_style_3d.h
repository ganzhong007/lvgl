/**
 * @file lv_style_3d.h
 *
 */

#ifndef LV_STYLE_3D_H
#define LV_STYLE_3D_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../config/lv_conf_internal.h"
#include "../core/lv_style.h"

#if LV_USE_3D_DRAW_TASKS

/*********************
 *      DEFINES
 *********************/

#define LV_3D_STYLE_SHININESS_SCALE     10
#define LV_3D_STYLE_AMBIENT_SCALE       1000
#define LV_3D_STYLE_INTENSITY_SCALE     100

#define LV_3D_STYLE_SHININESS_TO_NUM(v) ((int32_t)((v) * LV_3D_STYLE_SHININESS_SCALE))
#define LV_3D_STYLE_NUM_TO_SHININESS(n) ((float)(n) / LV_3D_STYLE_SHININESS_SCALE)

#define LV_3D_STYLE_AMBIENT_TO_NUM(v)   ((int32_t)((v) * LV_3D_STYLE_AMBIENT_SCALE))
#define LV_3D_STYLE_NUM_TO_AMBIENT(n)   ((float)(n) / LV_3D_STYLE_AMBIENT_SCALE)

#define LV_3D_STYLE_INTENSITY_TO_NUM(v) ((int32_t)((v) * LV_3D_STYLE_INTENSITY_SCALE))
#define LV_3D_STYLE_NUM_TO_INTENSITY(n) ((float)(n) / LV_3D_STYLE_INTENSITY_SCALE)

/**********************
 * GLOBAL PROTOTYPES
 **********************/

extern lv_style_prop_t LV_STYLE_3D_CLEAR_COLOR;
extern lv_style_prop_t LV_STYLE_3D_CLEAR_OPA;
extern lv_style_prop_t LV_STYLE_3D_GRID_VISIBLE;
extern lv_style_prop_t LV_STYLE_3D_GRID_COLOR;
extern lv_style_prop_t LV_STYLE_3D_MESH_COLOR;
extern lv_style_prop_t LV_STYLE_3D_MESH_OPA;
extern lv_style_prop_t LV_STYLE_3D_SHININESS;
extern lv_style_prop_t LV_STYLE_3D_AMBIENT;
extern lv_style_prop_t LV_STYLE_3D_PHONG;
extern lv_style_prop_t LV_STYLE_3D_LIGHT_COLOR;
extern lv_style_prop_t LV_STYLE_3D_LIGHT_OPA;
extern lv_style_prop_t LV_STYLE_3D_LIGHT_INTENSITY;

void lv_style_3d_init(void);

static inline void lv_style_set_3d_clear_color(lv_style_t * style, lv_color_t value)
{
    lv_style_value_t v = { .color = value };
    lv_style_set_prop(style, LV_STYLE_3D_CLEAR_COLOR, v);
}

static inline void lv_style_set_3d_clear_opa(lv_style_t * style, lv_opa_t value)
{
    lv_style_value_t v = { .num = (int32_t)value };
    lv_style_set_prop(style, LV_STYLE_3D_CLEAR_OPA, v);
}

static inline void lv_style_set_3d_grid_visible(lv_style_t * style, bool visible)
{
    lv_style_value_t v = { .num = visible ? 1 : 0 };
    lv_style_set_prop(style, LV_STYLE_3D_GRID_VISIBLE, v);
}

static inline void lv_style_set_3d_grid_color(lv_style_t * style, lv_color_t value)
{
    lv_style_value_t v = { .color = value };
    lv_style_set_prop(style, LV_STYLE_3D_GRID_COLOR, v);
}

static inline void lv_style_set_3d_mesh_color(lv_style_t * style, lv_color_t value)
{
    lv_style_value_t v = { .color = value };
    lv_style_set_prop(style, LV_STYLE_3D_MESH_COLOR, v);
}

static inline void lv_style_set_3d_mesh_opa(lv_style_t * style, lv_opa_t value)
{
    lv_style_value_t v = { .num = (int32_t)value };
    lv_style_set_prop(style, LV_STYLE_3D_MESH_OPA, v);
}

static inline void lv_style_set_3d_shininess(lv_style_t * style, float value)
{
    lv_style_value_t v = { .num = LV_3D_STYLE_SHININESS_TO_NUM(value) };
    lv_style_set_prop(style, LV_STYLE_3D_SHININESS, v);
}

static inline void lv_style_set_3d_ambient(lv_style_t * style, float value)
{
    lv_style_value_t v = { .num = LV_3D_STYLE_AMBIENT_TO_NUM(value) };
    lv_style_set_prop(style, LV_STYLE_3D_AMBIENT, v);
}

static inline void lv_style_set_3d_phong(lv_style_t * style, bool enable)
{
    lv_style_value_t v = { .num = enable ? 1 : 0 };
    lv_style_set_prop(style, LV_STYLE_3D_PHONG, v);
}

static inline void lv_style_set_3d_light_color(lv_style_t * style, lv_color_t value)
{
    lv_style_value_t v = { .color = value };
    lv_style_set_prop(style, LV_STYLE_3D_LIGHT_COLOR, v);
}

static inline void lv_style_set_3d_light_opa(lv_style_t * style, lv_opa_t value)
{
    lv_style_value_t v = { .num = (int32_t)value };
    lv_style_set_prop(style, LV_STYLE_3D_LIGHT_OPA, v);
}

static inline void lv_style_set_3d_light_intensity(lv_style_t * style, float value)
{
    lv_style_value_t v = { .num = LV_3D_STYLE_INTENSITY_TO_NUM(value) };
    lv_style_set_prop(style, LV_STYLE_3D_LIGHT_INTENSITY, v);
}

void lv_3dstyle_apply_viewport(lv_obj_t * obj);
void lv_3dstyle_apply_mesh(lv_obj_t * obj);
void lv_3dstyle_apply_light(lv_obj_t * obj);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_3D_DRAW_TASKS*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_STYLE_3D_H*/
