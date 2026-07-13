/**
 * @file lv_draw_evgpu_mask_rect.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU

#include "lv_evgpu_utils.h"

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

void lv_draw_evgpu_mask_rect(lv_draw_task_t * t, const lv_draw_mask_rect_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;
    lv_area_t draw_area;

    if(!lv_area_intersect(&draw_area, &dsc->area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;

    evgrBeginPath(u->evgr);

    /* Nesting cropping regions using rounded rectangles and normal rectangles */
    lv_evgpu_path_append_rect(
        u->evgr,
        dsc->area.x1, dsc->area.y1,
        lv_area_get_width(&dsc->area), lv_area_get_height(&dsc->area),
        dsc->radius);
    lv_evgpu_path_append_rect(
        u->evgr,
        t->clip_area.x1, t->clip_area.y1,
        lv_area_get_width(&t->clip_area), lv_area_get_height(&t->clip_area),
        0);

    /* Use EVGR_DESTINATION_IN (Sa * D) blending mode to make the corners transparent */
    lv_evgpu_fill(
        u->evgr,
        EVGR_CCW,
        EVGR_DESTINATION_IN,
        evgrRGBA(0, 0, 0, 0));

    LV_PROFILER_DRAW_END;
}

/**********************
*   STATIC FUNCTIONS
**********************/

#endif /* LV_USE_DRAW_EVGPU */
