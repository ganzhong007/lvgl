/**
 * @file lv_draw_evgpu_layer.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU

#include "lv_evgpu_utils.h"
#include "lv_evgpu_fbo_cache.h"
#include "../lv_draw_image_private.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_evgpu_layer(lv_draw_task_t * t, const lv_draw_image_dsc_t * draw_dsc,
                          const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;
    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;
    lv_layer_t * layer = (lv_layer_t *)draw_dsc->src;

    if(!layer->user_data) {
        LV_PROFILER_DRAW_END;
        return;
    }

    int image_handle = lv_evgpu_fb_get_image_handle(lv_evgpu_fbo_cache_entry_to_fb(layer->user_data));
    if(image_handle <= 0) {
        LV_LOG_WARN("Invalid image handle: %d", image_handle);
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_image_dsc_t new_draw_dsc = *draw_dsc;
    new_draw_dsc.src = NULL;
    lv_draw_evgpu_image(t, &new_draw_dsc, coords, image_handle);

    lv_evgpu_end_frame(u);

    LV_PROFILER_DRAW_END;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* LV_USE_DRAW_EVGPU */
