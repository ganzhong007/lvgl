/**
 * @file lv_gpu_renderer_batch_3d.c
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER && LV_USE_3D

#include "lv_gpu_renderer_batch_3d.h"

static uint32_t material_kind_key(const lv_3d_draw_item_t * it)
{
    return (uint32_t)it->material.kind;
}

void lv_gpu_renderer_batch_3d_sort_opaque(const lv_3d_draw_item_t * items, uint32_t item_count,
                                           uint32_t * opaque_order, uint32_t opaque_count)
{
    LV_UNUSED(item_count);
    if(opaque_count <= 1) return;

    for(uint32_t a = 1; a < opaque_count; a++) {
        uint32_t key = opaque_order[a];
        uint32_t key_mat = material_kind_key(&items[key]);
        uint32_t b = a;
        while(b > 0) {
            uint32_t prev = opaque_order[b - 1];
            if(material_kind_key(&items[prev]) <= key_mat) break;
            opaque_order[b] = prev;
            b--;
        }
        opaque_order[b] = key;
    }
}

uint32_t lv_gpu_renderer_batch_3d_count_material_runs(const lv_3d_draw_item_t * items,
                                                       const uint32_t * opaque_order,
                                                       uint32_t opaque_count)
{
    if(opaque_count == 0) return 0;

    uint32_t runs = 1;
    uint32_t prev = material_kind_key(&items[opaque_order[0]]);
    for(uint32_t i = 1; i < opaque_count; i++) {
        uint32_t k = material_kind_key(&items[opaque_order[i]]);
        if(k != prev) {
            runs++;
            prev = k;
        }
    }
    return runs;
}

#endif /*LV_USE_DRAW_GPU_RENDERER && LV_USE_3D*/
