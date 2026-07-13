/**
 * @file lv_draw_evgpu_fill.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU

#include "lv_evgpu_utils.h"
#include "lv_evgpu_grad.h"
#include "lv_evgpu_solid.h"

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

void lv_draw_evgpu_fill(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc, const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;

    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;

    lv_area_t clip_area;
    if(!lv_area_intersect(&clip_area, coords, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    if(dsc->grad.dir != LV_GRAD_DIR_NONE) {
        if(lv_evgpu_grad_fill_rect(u, coords, &dsc->grad, (float)dsc->radius, &clip_area, &u->ctx.matrix)) {
            LV_PROFILER_DRAW_END;
            return;
        }
#if LV_USE_VECTOR_GRAPHIC
        evgrBeginPath(u->evgr);
        lv_evgpu_path_append_rect(u->evgr,
                                   coords->x1, coords->y1,
                                   lv_area_get_width(coords), lv_area_get_height(coords),
                                   dsc->radius);
        lv_evgpu_draw_grad_helper(u->evgr, coords, &dsc->grad, EVGR_CCW, EVGR_SOURCE_OVER);
#else
        LV_LOG_WARN("Gradient fill is not supported without VECTOR_GRAPHIC");
#endif
    }
    else {
        if(lv_evgpu_solid_fill_rect(u, coords, dsc->color, dsc->opa, (float)dsc->radius, &clip_area,
                                   &u->ctx.matrix)) {
            LV_PROFILER_DRAW_END;
            return;
        }

        evgrBeginPath(u->evgr);

        lv_evgpu_path_append_rect(u->evgr,
                                   coords->x1, coords->y1,
                                   lv_area_get_width(coords), lv_area_get_height(coords),
                                   dsc->radius);

        lv_evgpu_fill(u->evgr, EVGR_CCW, EVGR_SOURCE_OVER, lv_evgpu_color_convert(dsc->color, dsc->opa));
    }

    LV_PROFILER_DRAW_END;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* LV_USE_DRAW_EVGPU */
