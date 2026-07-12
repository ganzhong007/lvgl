/**
 * @file lv_draw_g100_mask_rect.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_g100_private.h"

#if LV_USE_DRAW_G100

#include "lv_g100_utils.h"

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

void lv_draw_g100_mask_rect(lv_draw_task_t * t, const lv_draw_mask_rect_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;
    lv_area_t draw_area;

    if(!lv_area_intersect(&draw_area, &dsc->area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_g100_unit_t * u = (lv_draw_g100_unit_t *)t->draw_unit;

    nvgBeginPath(u->vg);

    /* Nesting cropping regions using rounded rectangles and normal rectangles */
    lv_g100_path_append_rect(
        u->vg,
        dsc->area.x1, dsc->area.y1,
        lv_area_get_width(&dsc->area), lv_area_get_height(&dsc->area),
        dsc->radius);
    lv_g100_path_append_rect(
        u->vg,
        t->clip_area.x1, t->clip_area.y1,
        lv_area_get_width(&t->clip_area), lv_area_get_height(&t->clip_area),
        0);

    /* Use NVG_DESTINATION_IN (Sa * D) blending mode to make the corners transparent */
    lv_g100_fill(
        u->vg,
        NVG_CCW,
        NVG_DESTINATION_IN,
        nvgRGBA(0, 0, 0, 0));

    LV_PROFILER_DRAW_END;
}

/**********************
*   STATIC FUNCTIONS
**********************/

#endif /* LV_USE_DRAW_G100 */
