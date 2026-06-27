/**
 * @file lv_3dstack.c
 */

#include "../../include/lvgl/widgets/lv_3dstack.h"

#if LV_USE_3D && LV_USE_3DSTACK

#include "../../core/lv_obj_class_private.h"
#include "lv_3dmesh_private.h"

typedef struct {
    lv_obj_t obj;
    uint8_t cols;
    uint8_t rows;
    float row_z[8];
    float x_gap;
    float y_gap;
} lv_3dstack_t;

static const lv_obj_class_t lv_3dstack_class = {
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .instance_size = sizeof(lv_3dstack_t),
    .base_class = &lv_obj_class,
    .name = "3dstack",
};

lv_obj_t * lv_3dstack_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(&lv_3dstack_class, parent);
    lv_obj_class_init_obj(obj);
    lv_3dstack_t * stack = (lv_3dstack_t *)obj;
    stack->cols = 3;
    stack->rows = 3;
    stack->row_z[0] = -400;
    stack->row_z[1] = -700;
    stack->row_z[2] = -1000;
    stack->x_gap = 220;
    stack->y_gap = 160;
    return obj;
}

void lv_3dstack_set_grid(lv_obj_t * obj, uint8_t cols, uint8_t rows)
{
    lv_3dstack_t * stack = (lv_3dstack_t *)obj;
    stack->cols = cols;
    stack->rows = rows;
}

void lv_3dstack_set_row_depth(lv_obj_t * obj, uint8_t row, float z)
{
    lv_3dstack_t * stack = (lv_3dstack_t *)obj;
    if(row < 8) stack->row_z[row] = z;
}

void lv_3dstack_set_cell_spacing(lv_obj_t * obj, float x_gap, float y_gap)
{
    lv_3dstack_t * stack = (lv_3dstack_t *)obj;
    stack->x_gap = x_gap;
    stack->y_gap = y_gap;
}

void lv_3dstack_layout(lv_obj_t * obj)
{
    lv_3dstack_t * stack = (lv_3dstack_t *)obj;
    uint32_t n = lv_obj_get_child_count(obj);
    uint32_t idx = 0;
    for(uint32_t r = 0; r < stack->rows && idx < n; r++) {
        for(uint32_t c = 0; c < stack->cols && idx < n; c++) {
            lv_obj_t * tile = lv_obj_get_child(obj, idx);
            if(lv_obj_check_type(tile, &lv_3dmesh_class)) {
                float cx = (float)c - (stack->cols - 1) * 0.5f;
                float cy = (float)r - (stack->rows - 1) * 0.5f;
                lv_3dmesh_set_position(tile, cx * stack->x_gap, cy * stack->y_gap, stack->row_z[r]);
                float sc = 1.0f - r * 0.08f;
                lv_3dmesh_set_scale(tile, sc, sc, sc);
            }
            idx++;
        }
    }
}

#endif /*LV_USE_3D && LV_USE_3DSTACK*/
