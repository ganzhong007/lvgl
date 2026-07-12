/**
 * @file lv_draw_g100_vector.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_g100_private.h"

#if (LV_USE_DRAW_G100) && LV_USE_VECTOR_GRAPHIC

#include "lv_g100_utils.h"
#include "lv_g100_image_cache.h"
#include "../lv_draw_vector_private.h"
#include "../lv_image_decoder_private.h"
#include <float.h>
#include <math.h>

/*********************
*      DEFINES
*********************/

#define OPA_MIX(opa1, opa2) LV_UDIV255((opa1) * (opa2))

/**********************
*      TYPEDEFS
**********************/

/**********************
*  STATIC PROTOTYPES
**********************/

static void task_draw_cb(void * ctx, const lv_vector_path_t * path, const lv_vector_path_ctx_t * dsc);
static void lv_path_to_nvg(NVGcontext * ctx, const lv_vector_path_t * src, lv_fpoint_t * offset);
static void lv_path_stroke_dashed(NVGcontext * ctx, const lv_vector_path_t * src,
                                  const lv_vector_stroke_dsc_t * stroke_dsc);
static enum NVGcompositeOperation lv_blend_to_nvg(lv_vector_blend_t blend);
static enum NVGwinding lv_fill_to_nvg(lv_vector_fill_t fill_rule);
static int lv_stroke_cap_to_nvg(lv_vector_stroke_cap_t cap);
static int lv_stroke_join_to_nvg(lv_vector_stroke_join_t join);

/**********************
*  STATIC VARIABLES
**********************/

/**********************
*      MACROS
**********************/

/**********************
*   GLOBAL FUNCTIONS
**********************/

void lv_draw_g100_vector(lv_draw_task_t * t, const lv_draw_vector_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;
    if(dsc->task_list == NULL) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_layer_t * layer = dsc->base.layer;
    if(layer->draw_buf == NULL) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_g100_unit_t * u = (lv_draw_g100_unit_t *)t->draw_unit;

    nvgGlobalAlpha(u->vg, t->opa / (float)LV_OPA_COVER);

    lv_vector_for_each_destroy_tasks(dsc->task_list, task_draw_cb, u);
    LV_PROFILER_DRAW_END;
}

/**********************
*   STATIC FUNCTIONS
**********************/

static NVGcolor lv_color32_to_nvg(lv_color32_t color, lv_opa_t opa)
{
    uint8_t a = LV_UDIV255(color.alpha * opa);
    return nvgRGBA(color.red, color.green, color.blue, a);
}

static void draw_fill(lv_draw_g100_unit_t * u, const lv_vector_fill_dsc_t * fill_dsc, const lv_fpoint_t * offset,
                      enum NVGcompositeOperation comp_op)
{
    LV_PROFILER_DRAW_BEGIN;

    const enum NVGwinding winding = lv_fill_to_nvg(fill_dsc->fill_rule);

    lv_g100_transform(u->vg, &fill_dsc->matrix);

    switch(fill_dsc->style) {
        case LV_VECTOR_DRAW_STYLE_SOLID: {
                lv_g100_fill(u->vg, winding, comp_op, lv_color32_to_nvg(fill_dsc->color, fill_dsc->opa));
            }
            break;
        case LV_VECTOR_DRAW_STYLE_PATTERN: {
                const lv_draw_image_dsc_t * img_dsc = &fill_dsc->img_dsc;
                lv_image_header_t header;
                int image_handle = lv_g100_image_cache_get_handle(u, img_dsc->src, 0, &header);
                if(image_handle < 0) {
                    LV_PROFILER_DRAW_END;
                    return;
                }

                float offset_x = 0;
                float offset_y = 0;

                if(fill_dsc->fill_units == LV_VECTOR_FILL_UNITS_OBJECT_BOUNDING_BOX) {
                    offset_x = offset->x;
                    offset_y = offset->y;
                }

                NVGpaint paint = nvgImagePattern(u->vg, offset_x, offset_y, header.w, header.h, 0, image_handle,
                                                 img_dsc->opa / (float)LV_OPA_COVER);

                nvgFillPaint(u->vg, paint);
                nvgFill(u->vg);
            }
            break;
        case LV_VECTOR_DRAW_STYLE_GRADIENT: {
                lv_g100_draw_grad(u->vg, &fill_dsc->gradient, winding, comp_op);
            }
            break;
        default:
            LV_LOG_WARN("unsupported style: %d", fill_dsc->style);
            break;
    }

    LV_PROFILER_DRAW_END;
}

static bool draw_stroke_prepare(lv_draw_g100_unit_t * u, const lv_vector_stroke_dsc_t * stroke_dsc)
{
    lv_g100_transform(u->vg, &stroke_dsc->matrix);

    nvgStrokeColor(u->vg, lv_color32_to_nvg(stroke_dsc->color, stroke_dsc->opa));
    nvgStrokeWidth(u->vg, stroke_dsc->width);
    nvgLineCap(u->vg, lv_stroke_cap_to_nvg(stroke_dsc->cap));
    nvgLineJoin(u->vg, lv_stroke_join_to_nvg(stroke_dsc->join));
    nvgMiterLimit(u->vg, stroke_dsc->miter_limit > 0 ? stroke_dsc->miter_limit : 4.0f);

    switch(stroke_dsc->style) {
        case LV_VECTOR_DRAW_STYLE_SOLID:
            break;

        case LV_VECTOR_DRAW_STYLE_GRADIENT: {
                NVGpaint paint;
                if(!lv_g100_grad_to_paint(u->vg, &stroke_dsc->gradient, &paint)) {
                    return false;
                }
                nvgStrokePaint(u->vg, paint);
            }
            break;

        default:
            LV_LOG_WARN("unsupported style: %d", stroke_dsc->style);
            return false;
    }

    return true;
}

static void draw_stroke(lv_draw_g100_unit_t * u, const lv_vector_stroke_dsc_t * stroke_dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    if(!draw_stroke_prepare(u, stroke_dsc)) {
        LV_PROFILER_DRAW_END;
        return;
    }
    nvgStroke(u->vg);

    LV_PROFILER_DRAW_END;
}

static void draw_stroke_dashed(lv_draw_g100_unit_t * u, const lv_vector_path_t * path,
                               const lv_vector_stroke_dsc_t * stroke_dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    if(!draw_stroke_prepare(u, stroke_dsc)) {
        LV_PROFILER_DRAW_END;
        return;
    }
    lv_path_stroke_dashed(u->vg, path, stroke_dsc);

    LV_PROFILER_DRAW_END;
}

static void task_draw_cb(void * ctx, const lv_vector_path_t * path, const lv_vector_path_ctx_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;
    lv_draw_g100_unit_t * u = ctx;

    /* clear area */
    if(!path) {
        NVGcolor c = lv_color32_to_nvg(dsc->fill_dsc.color, dsc->fill_dsc.opa);
        nvgBeginPath(u->vg);
        lv_g100_path_append_area(u->vg, &dsc->scissor_area);
        lv_g100_fill(u->vg, NVG_CCW, NVG_COPY, c);
        LV_PROFILER_DRAW_END;
        return;
    }

    if(dsc->fill_dsc.opa == LV_OPA_TRANSP && dsc->stroke_dsc.opa == LV_OPA_TRANSP) {
        LV_LOG_TRACE("Full transparent, no need to draw");
        LV_PROFILER_DRAW_END;
        return;
    }

    nvgSave(u->vg);
    lv_g100_transform(u->vg, &dsc->matrix);

    lv_fpoint_t offset = {0, 0};
    lv_path_to_nvg(u->vg, path, &offset);

    lv_g100_set_clip_area(u->vg, &dsc->scissor_area);

    const enum NVGcompositeOperation comp_op = lv_blend_to_nvg(dsc->blend_mode);
    nvgGlobalCompositeOperation(u->vg, comp_op);

    if(dsc->fill_dsc.opa) {
        draw_fill(u, &dsc->fill_dsc, &offset, comp_op);
    }

    if(dsc->stroke_dsc.opa) {
        if(lv_array_is_empty(&dsc->stroke_dsc.dash_pattern)) {
            draw_stroke(u, &dsc->stroke_dsc);
        }
        else {
            draw_stroke_dashed(u, path, &dsc->stroke_dsc);
        }
    }

    nvgRestore(u->vg);

    LV_PROFILER_DRAW_END;
}

static void lv_path_to_nvg(NVGcontext * ctx, const lv_vector_path_t * src, lv_fpoint_t * offset)
{
    LV_PROFILER_DRAW_BEGIN;

    float min_x = FLT_MAX;
    float min_y = FLT_MAX;
    float max_x = -FLT_MAX;
    float max_y = -FLT_MAX;

#define CMP_BOUNDS(point)                           \
    do {                                            \
        if((point)->x < min_x) min_x = (point)->x;  \
        if((point)->y < min_y) min_y = (point)->y;  \
        if((point)->x > max_x) max_x = (point)->x;  \
        if((point)->y > max_y) max_y = (point)->y;  \
    } while(0)

    const lv_vector_path_op_t * ops = lv_array_front(&src->ops);
    const lv_fpoint_t * point = lv_array_front(&src->points);
    const uint32_t op_size = lv_array_size(&src->ops);

    nvgBeginPath(ctx);

    for(uint32_t i = 0; i < op_size; i++) {
        switch(ops[i]) {
            case LV_VECTOR_PATH_OP_MOVE_TO: {
                    nvgMoveTo(ctx, point->x, point->y);
                    CMP_BOUNDS(point);
                    point++;
                }
                break;
            case LV_VECTOR_PATH_OP_LINE_TO: {
                    nvgLineTo(ctx, point->x, point->y);
                    CMP_BOUNDS(point);
                    point++;
                }
                break;
            case LV_VECTOR_PATH_OP_QUAD_TO: {
                    nvgQuadTo(ctx, point[0].x, point[0].y, point[1].x, point[1].y);
                    CMP_BOUNDS(&point[0]);
                    CMP_BOUNDS(&point[1]);
                    point += 2;
                }
                break;
            case LV_VECTOR_PATH_OP_CUBIC_TO: {
                    nvgBezierTo(ctx, point[0].x, point[0].y, point[1].x, point[1].y, point[2].x, point[2].y);
                    CMP_BOUNDS(&point[0]);
                    CMP_BOUNDS(&point[1]);
                    CMP_BOUNDS(&point[2]);
                    point += 3;
                }
                break;
            case LV_VECTOR_PATH_OP_CLOSE: {
                    nvgClosePath(ctx);
                }
                break;
            default:
                LV_LOG_WARN("unknown op: %d", ops[i]);
                break;
        }
    }

    offset->x = lroundf(min_x);
    offset->y = lroundf(min_y);
    LV_PROFILER_DRAW_END;
}

static enum NVGcompositeOperation lv_blend_to_nvg(lv_vector_blend_t blend)
{
    switch(blend) {
        case LV_VECTOR_BLEND_SRC_OVER:
            return NVG_SOURCE_OVER;
        case LV_VECTOR_BLEND_SRC_IN:
            return NVG_SOURCE_IN;
        case LV_VECTOR_BLEND_DST_OVER:
            return NVG_DESTINATION_OVER;
        case LV_VECTOR_BLEND_DST_IN:
            return NVG_DESTINATION_IN;
        case LV_VECTOR_BLEND_NONE:
            return NVG_COPY;
        default:
            LV_LOG_INFO("Unknown supported blend mode: %d", blend);
            return NVG_SOURCE_OVER;
    }
}

static enum NVGwinding lv_fill_to_nvg(lv_vector_fill_t fill_rule)
{
    switch(fill_rule) {
        case LV_VECTOR_FILL_NONZERO:
            return NVG_CCW;
        case LV_VECTOR_FILL_EVENODD:
            return NVG_CW;
        default:
            LV_LOG_WARN("Unknown supported fill rule: %d", fill_rule);
            return NVG_CCW;
    }
}

static int lv_stroke_cap_to_nvg(lv_vector_stroke_cap_t cap)
{
    switch(cap) {
        case LV_VECTOR_STROKE_CAP_BUTT:
            return NVG_BUTT;
        case LV_VECTOR_STROKE_CAP_ROUND:
            return NVG_ROUND;
        case LV_VECTOR_STROKE_CAP_SQUARE:
            return NVG_SQUARE;
        default:
            return NVG_BUTT;
    }
}

static int lv_stroke_join_to_nvg(lv_vector_stroke_join_t join)
{
    switch(join) {
        case LV_VECTOR_STROKE_JOIN_MITER:
            return NVG_MITER;
        case LV_VECTOR_STROKE_JOIN_ROUND:
            return NVG_ROUND;
        case LV_VECTOR_STROKE_JOIN_BEVEL:
            return NVG_BEVEL;
        default:
            return NVG_MITER;
    }
}

#define G100_DASH_FLAT_MAX 512

static void flatten_quad(const lv_fpoint_t * p0, const lv_fpoint_t * p1, const lv_fpoint_t * p2,
                         lv_fpoint_t * out, uint32_t * out_cnt, uint32_t out_cap)
{
    const uint32_t steps = 12;
    for(uint32_t i = 1; i <= steps && *out_cnt < out_cap; i++) {
        const float t = (float)i / (float)steps;
        const float u = 1.0f - t;
        out[*out_cnt].x = u * u * p0->x + 2.0f * u * t * p1->x + t * t * p2->x;
        out[*out_cnt].y = u * u * p0->y + 2.0f * u * t * p1->y + t * t * p2->y;
        (*out_cnt)++;
    }
}

static void flatten_cubic(const lv_fpoint_t * p0, const lv_fpoint_t * p1, const lv_fpoint_t * p2,
                          const lv_fpoint_t * p3, lv_fpoint_t * out, uint32_t * out_cnt, uint32_t out_cap)
{
    const uint32_t steps = 16;
    for(uint32_t i = 1; i <= steps && *out_cnt < out_cap; i++) {
        const float t = (float)i / (float)steps;
        const float u = 1.0f - t;
        const float u2 = u * u;
        const float u3 = u2 * u;
        const float t2 = t * t;
        const float t3 = t2 * t;
        out[*out_cnt].x = u3 * p0->x + 3.0f * u2 * t * p1->x + 3.0f * u * t2 * p2->x + t3 * p3->x;
        out[*out_cnt].y = u3 * p0->y + 3.0f * u2 * t * p1->y + 3.0f * u * t2 * p2->y + t3 * p3->y;
        (*out_cnt)++;
    }
}

static void dash_polyline(NVGcontext * ctx, const lv_fpoint_t * pts, uint32_t pt_count,
                          const float * pattern, uint32_t pattern_count)
{
    if(pt_count < 2 || pattern_count == 0) return;

    uint32_t pat_idx = 0;
    float pat_remaining = pattern[0];
    bool drawing = true;

    lv_fpoint_t cur = pts[0];
    for(uint32_t i = 1; i < pt_count; i++) {
        const lv_fpoint_t end = pts[i];
        const float dx = end.x - cur.x;
        const float dy = end.y - cur.y;
        const float seg_len = sqrtf(dx * dx + dy * dy);
        if(seg_len < 1e-4f) {
            cur = end;
            continue;
        }

        const float ux = dx / seg_len;
        const float uy = dy / seg_len;
        float traveled = 0.0f;

        while(traveled < seg_len - 1e-4f) {
            const float step = LV_MIN(pat_remaining, seg_len - traveled);
            const float x0 = cur.x + ux * traveled;
            const float y0 = cur.y + uy * traveled;
            const float x1 = cur.x + ux * (traveled + step);
            const float y1 = cur.y + uy * (traveled + step);

            if(drawing) {
                nvgBeginPath(ctx);
                nvgMoveTo(ctx, x0, y0);
                nvgLineTo(ctx, x1, y1);
                nvgStroke(ctx);
            }

            traveled += step;
            pat_remaining -= step;
            if(pat_remaining <= 1e-4f) {
                pat_idx = (pat_idx + 1) % pattern_count;
                pat_remaining = pattern[pat_idx];
                drawing = (pat_idx % 2) == 0;
            }
        }

        cur = end;
    }
}

static void lv_path_stroke_dashed(NVGcontext * ctx, const lv_vector_path_t * src,
                                  const lv_vector_stroke_dsc_t * stroke_dsc)
{
    const lv_vector_path_op_t * ops = lv_array_front(&src->ops);
    const lv_fpoint_t * point = lv_array_front(&src->points);
    const uint32_t op_size = lv_array_size(&src->ops);
    const float * pattern = lv_array_front(&stroke_dsc->dash_pattern);
    const uint32_t pattern_count = lv_array_size(&stroke_dsc->dash_pattern);

    lv_fpoint_t flat[G100_DASH_FLAT_MAX];
    uint32_t flat_count = 0;
    lv_fpoint_t sub_start = {0, 0};
    lv_fpoint_t cur = {0, 0};
    bool has_cur = false;

    for(uint32_t i = 0; i < op_size; i++) {
        switch(ops[i]) {
            case LV_VECTOR_PATH_OP_MOVE_TO:
                if(flat_count >= 2) {
                    dash_polyline(ctx, flat, flat_count, pattern, pattern_count);
                }
                flat_count = 0;
                cur = *point;
                sub_start = *point;
                has_cur = true;
                flat[flat_count++] = cur;
                point++;
                break;

            case LV_VECTOR_PATH_OP_LINE_TO:
                if(!has_cur) break;
                cur = *point;
                if(flat_count < G100_DASH_FLAT_MAX) flat[flat_count++] = cur;
                point++;
                break;

            case LV_VECTOR_PATH_OP_QUAD_TO:
                if(!has_cur) break;
                flatten_quad(&cur, &point[0], &point[1], flat, &flat_count, G100_DASH_FLAT_MAX);
                cur = point[1];
                point += 2;
                break;

            case LV_VECTOR_PATH_OP_CUBIC_TO:
                if(!has_cur) break;
                flatten_cubic(&cur, &point[0], &point[1], &point[2], flat, &flat_count, G100_DASH_FLAT_MAX);
                cur = point[2];
                point += 3;
                break;

            case LV_VECTOR_PATH_OP_CLOSE:
                if(has_cur && flat_count < G100_DASH_FLAT_MAX) {
                    flat[flat_count++] = sub_start;
                }
                break;

            default:
                LV_LOG_WARN("unknown op: %d", ops[i]);
                break;
        }
    }

    if(flat_count >= 2) {
        dash_polyline(ctx, flat, flat_count, pattern, pattern_count);
    }
}

#endif /* LV_USE_DRAW_G100 */
