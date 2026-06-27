/**
 * @file lv_draw_3d.c
 */

#include "lv_draw_private.h"

#if LV_USE_3DTEXTURE || LV_USE_3D

#if LV_USE_3D
#include "../../include/lvgl/3d/lv_3d.h"
#endif

void lv_draw_3d_dsc_init(lv_draw_3d_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_3d_dsc_t));
#if LV_USE_3DTEXTURE
    dsc->tex_id = LV_3DTEXTURE_ID_NULL;
    dsc->h_flip = false;
    dsc->v_flip = false;
#endif
#if LV_USE_3D
    dsc->kind = LV_3D_DRAW_KIND_VIEWPORT_PASS;
    dsc->mesh_id = LV_3D_MESH_ID_NONE;
    lv_3d_material_init(&dsc->material, LV_3D_MAT_OPAQUE, lv_color_white(), LV_OPA_COVER);
#endif
    dsc->opa = LV_OPA_COVER;
    dsc->base.dsc_size = sizeof(lv_draw_3d_dsc_t);
}

lv_draw_3d_dsc_t * lv_draw_task_get_3d_dsc(lv_draw_task_t * task)
{
    return task->type == LV_DRAW_TASK_TYPE_3D ? (lv_draw_3d_dsc_t *)task->draw_dsc : NULL;
}

void lv_draw_3d(lv_layer_t * layer, const lv_draw_3d_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_task_t * t = lv_draw_add_task(layer, coords, LV_DRAW_TASK_TYPE_3D);
    lv_memcpy(t->draw_dsc, dsc, sizeof(*dsc));
    lv_draw_finalize_task_creation(layer, t);

    LV_PROFILER_DRAW_END;
}

#endif /*LV_USE_3DTEXTURE || LV_USE_3D*/
