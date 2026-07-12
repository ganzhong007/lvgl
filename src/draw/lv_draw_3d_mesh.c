/**
 * @file lv_draw_3d_mesh.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_mesh.h"

#if LV_USE_3D_DRAW_TASKS

void lv_draw_3d_mesh_dsc_init(lv_draw_3d_mesh_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_3d_mesh_dsc_t));
    dsc->base.dsc_size = sizeof(lv_draw_3d_mesh_dsc_t);
    dsc->color = lv_color32_make(0xFF, 0xFF, 0xFF, LV_OPA_COVER);
    dsc->flags = LV_3D_MESH_FLAG_DEPTH_TEST | LV_3D_MESH_FLAG_CULL_FACE;
    dsc->model_matrix[0] = dsc->model_matrix[5] = dsc->model_matrix[10] = dsc->model_matrix[15] = 1.f;
}

lv_draw_3d_mesh_dsc_t * lv_draw_task_get_3d_mesh_dsc(lv_draw_task_t * task)
{
    return task->type == LV_DRAW_TASK_TYPE_3D_MESH ? (lv_draw_3d_mesh_dsc_t *)task->draw_dsc : NULL;
}

void lv_draw_3d_mesh(lv_layer_t * pass_layer, const lv_draw_3d_mesh_dsc_t * dsc)
{
    if(pass_layer == NULL || dsc == NULL || dsc->vertices == NULL || dsc->vertex_count == 0
       || dsc->indices == NULL || dsc->index_count == 0) {
        return;
    }

    LV_PROFILER_DRAW_BEGIN;

    lv_area_t area = pass_layer->buf_area;
    lv_draw_task_t * t = lv_draw_add_task(pass_layer, &area, LV_DRAW_TASK_TYPE_3D_MESH);
    lv_draw_3d_mesh_dsc_t * new_dsc = t->draw_dsc;

    new_dsc->base = dsc->base;
    new_dsc->base.dsc_size = sizeof(lv_draw_3d_mesh_dsc_t);
    new_dsc->color = dsc->color;
    new_dsc->flags = dsc->flags;
    new_dsc->vertex_count = dsc->vertex_count;
    new_dsc->index_count = dsc->index_count;
    lv_memcpy(new_dsc->model_matrix, dsc->model_matrix, sizeof(new_dsc->model_matrix));

    new_dsc->vertices = lv_malloc(sizeof(float) * 3 * dsc->vertex_count);
    new_dsc->indices = lv_malloc(sizeof(uint16_t) * dsc->index_count);
    if(new_dsc->vertices == NULL || new_dsc->indices == NULL) {
        if(new_dsc->vertices) lv_free((void *)new_dsc->vertices);
        if(new_dsc->indices) lv_free((void *)new_dsc->indices);
        new_dsc->vertices = NULL;
        new_dsc->indices = NULL;
        t->state = LV_DRAW_TASK_STATE_FINISHED;
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_memcpy((void *)new_dsc->vertices, dsc->vertices, sizeof(float) * 3 * dsc->vertex_count);
    lv_memcpy((void *)new_dsc->indices, dsc->indices, sizeof(uint16_t) * dsc->index_count);

    lv_draw_finalize_task_creation(pass_layer, t);

    LV_PROFILER_DRAW_END;
}

#endif /*LV_USE_3D_DRAW_TASKS*/
