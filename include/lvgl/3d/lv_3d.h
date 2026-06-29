/**
 * @file lv_3d.h
 * LVGL 3D subsystem public types
 */

#ifndef LV_3D_H
#define LV_3D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config/lv_conf_internal.h"

#if LV_USE_3D

#include "../draw/lv_color.h"
#include "../lv_types.h"

typedef uint32_t lv_3d_mesh_id_t;
#define LV_3D_MESH_ID_NONE 0

typedef enum {
    LV_3D_MAT_OPAQUE = 0,
    LV_3D_MAT_ALPHA,
    LV_3D_MAT_WIREFRAME,
    LV_3D_MAT_PLANE_SNAPSHOT,     /* baked 2D subtree texture on 3D quad */
    LV_3D_MAT_SHADED_BOX,         /* opaque box: color = sides, top_color = roof */
} lv_3d_material_kind_t;

typedef struct {
    lv_3d_material_kind_t kind;
    lv_color_t color;
    lv_color_t top_color;
    lv_opa_t opa;
} lv_3d_material_t;

typedef uint32_t lv_3d_snapshot_id_t;
#define LV_3D_SNAPSHOT_ID_NONE 0U

typedef struct {
    float x;
    float y;
    float z;
} lv_vec3_t;

typedef lv_vec3_t lv_point3d_t;

void lv_3d_material_init(lv_3d_material_t * mat, lv_3d_material_kind_t kind, lv_color_t color, lv_opa_t opa);

#endif /*LV_USE_3D*/

#ifdef __cplusplus
}
#endif

#endif /*LV_3D_H*/
