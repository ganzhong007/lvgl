/**
 * @file lv_draw_evgpu_line.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_evgpu_private.h"

#if LV_USE_DRAW_EVGPU

#include "lv_evgpu_utils.h"
#include "lv_evgpu_math.h"

/*********************
*      DEFINES
*********************/

#define SQ(x) ((x) * (x))

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

void lv_draw_evgpu_line(lv_draw_task_t * t, const lv_draw_line_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;
    lv_draw_evgpu_unit_t * u = (lv_draw_evgpu_unit_t *)t->draw_unit;

    float p1_x = dsc->p1.x;
    float p1_y = dsc->p1.y;
    float p2_x = dsc->p2.x;
    float p2_y = dsc->p2.y;

    if(p1_x == p2_x && p1_y == p2_y) {
        LV_PROFILER_DRAW_END;
        return;
    }

    float half_w = dsc->width * 0.5f;

    lv_area_t rel_clip_area;
    rel_clip_area.x1 = (int32_t)(LV_MIN(p1_x, p2_x) - half_w);
    rel_clip_area.x2 = (int32_t)(LV_MAX(p1_x, p2_x) + half_w);
    rel_clip_area.y1 = (int32_t)(LV_MIN(p1_y, p2_y) - half_w);
    rel_clip_area.y2 = (int32_t)(LV_MAX(p1_y, p2_y) + half_w);

    if(!lv_area_intersect(&rel_clip_area, &rel_clip_area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    int32_t dash_width = dsc->dash_width;
    int32_t dash_gap = dsc->dash_gap;
    int32_t dash_l = dash_width + dash_gap;

    float dx = p2_x - p1_x;
    float dy = p2_y - p1_y;
    float inv_dl = evgr_math_inv_sqrtf(SQ(dx) + SQ(dy));
    float w_dx = dsc->width * dy * inv_dl;
    float w_dy = dsc->width * dx * inv_dl;
    float w2_dx = w_dx / 2;
    float w2_dy = w_dy / 2;

    int32_t ndash = 0;
    if(dash_width && dash_l * inv_dl < 1.0f) {
        ndash = (int32_t)((1.0f / inv_dl + dash_l - 1) / dash_l);
    }

    evgrBeginPath(u->evgr);

    /* head point */
    float head_start_x = p1_x + w2_dx;
    float head_start_y = p1_y - w2_dy;
    float head_end_x = p1_x - w2_dx;
    float head_end_y = p1_y + w2_dy;

    /* tail point */
    float tail_start_x = p2_x - w2_dx;
    float tail_start_y = p2_y + w2_dy;
    float tail_end_x = p2_x + w2_dx;
    float tail_end_y = p2_y - w2_dy;

    /*
          head_start        tail_end
              *-----------------*
             /|                 |\
            / |                 | \
    arc_c *(  *p1             p2*  )* arc_c
            \ |                 | /
             \|                 |/
              *-----------------*
          head_end          tail_start
    */

    /* move to start point */
    evgrMoveTo(u->evgr, head_start_x, head_start_y);

    /* draw line head */
    if(dsc->round_start) {
        float arc_cx = p1_x - w2_dy;
        float arc_cy = p1_y - w2_dx;

        /* start 90deg arc */
        lv_evgpu_path_append_arc_right_angle(u->evgr,
                                              head_start_x, head_start_y,
                                              p1_x, p1_y,
                                              arc_cx, arc_cy);

        /* end 90deg arc */
        lv_evgpu_path_append_arc_right_angle(u->evgr,
                                              arc_cx, arc_cy,
                                              p1_x, p1_y,
                                              head_end_x, head_end_y);
    }
    else {
        evgrLineTo(u->evgr, head_end_x, head_end_y);
    }

    /* draw line body */
    evgrLineTo(u->evgr, tail_start_x, tail_start_y);

    /* draw line tail */
    if(dsc->round_end) {
        float arc_cx = p2_x + w2_dy;
        float arc_cy = p2_y + w2_dx;
        lv_evgpu_path_append_arc_right_angle(u->evgr,
                                              tail_start_x, tail_start_y,
                                              p2_x, p2_y,
                                              arc_cx, arc_cy);
        lv_evgpu_path_append_arc_right_angle(u->evgr,
                                              arc_cx, arc_cy,
                                              p2_x, p2_y,
                                              tail_end_x, tail_end_y);
    }
    else {
        evgrLineTo(u->evgr, tail_end_x, tail_end_y);
    }

    /* close draw line body */
    evgrLineTo(u->evgr, head_start_x, head_start_y);

    for(int32_t i = 0; i < ndash; i++) {
        float start_x = p1_x - w2_dx + dx * (i * dash_l + dash_width) * inv_dl;
        float start_y = p1_y + w2_dy + dy * (i * dash_l + dash_width) * inv_dl;

        evgrMoveTo(u->evgr, start_x, start_y);
        evgrLineTo(u->evgr,
                  p1_x + w2_dx + dx * (i * dash_l + dash_width) * inv_dl,
                  p1_y - w2_dy + dy * (i * dash_l + dash_width) * inv_dl);
        evgrLineTo(u->evgr,
                  p1_x + w2_dx + dx * (i + 1) * dash_l * inv_dl,
                  p1_y - w2_dy + dy * (i + 1) * dash_l * inv_dl);
        evgrLineTo(u->evgr,
                  p1_x - w2_dx + dx * (i + 1) * dash_l * inv_dl,
                  p1_y + w2_dy + dy * (i + 1) * dash_l * inv_dl);
        evgrLineTo(u->evgr, start_x, start_y);
    }

    lv_evgpu_fill(
        u->evgr,
        EVGR_CCW,
        EVGR_SOURCE_OVER,
        lv_evgpu_color_convert(dsc->color, dsc->opa));

    LV_PROFILER_DRAW_END;
}

/**********************
*   STATIC FUNCTIONS
**********************/

#endif /* LV_USE_DRAW_EVGPU */
