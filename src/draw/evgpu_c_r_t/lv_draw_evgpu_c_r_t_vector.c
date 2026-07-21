/**
 * @file lv_draw_evgpu_c_r_t_vector.c
 *
 * P0 VECTOR path for EVGPU_C_R_T: flatten curves → ear-clip fill +
 * thick-segment stroke → existing solid GLES2 shaders. Gradients,
 * patterns, dashes land in later phases.
 */

#include "../../drivers/opengles/lv_opengles_private.h"
#define LV_EVGPU_C_R_T_SKIP_SYSTEM_GLES 1
#include "lv_draw_evgpu_c_r_t_private.h"

#if LV_USE_DRAW_EVGPU_C_R_T && LV_USE_VECTOR_GRAPHIC

#include "../lv_draw_vector_private.h"
#include <math.h>
#include <float.h>

#define CRT_VEC_MAX_PTS   2048
#define CRT_VEC_MAX_TRIS  (CRT_VEC_MAX_PTS * 3)
#define CRT_VEC_CURVE_STEPS_DEFAULT 8

typedef struct {
    float x;
    float y;
} crt_vec_pt_t;

typedef struct {
    lv_draw_evgpu_c_r_t_unit_t * u;
} crt_vec_ctx_t;

static uint32_t color32_to_gl(lv_color32_t c, lv_opa_t opa)
{
    uint8_t a = (uint8_t)LV_UDIV255((uint16_t)c.alpha * opa);
    return ((uint32_t)a << 24) | ((uint32_t)c.red << 16) | ((uint32_t)c.green << 8) | (uint32_t)c.blue;
}

static void xform_pt(const lv_matrix_t * m, float x, float y, float * ox, float * oy)
{
    if(m == NULL) {
        *ox = x;
        *oy = y;
        return;
    }
    float w = x * m->m[2][0] + y * m->m[2][1] + m->m[2][2];
    if(LV_ABS(w) < FLT_EPSILON) {
        *ox = x;
        *oy = y;
        return;
    }
    float inv = 1.f / w;
    *ox = (x * m->m[0][0] + y * m->m[0][1] + m->m[0][2]) * inv;
    *oy = (x * m->m[1][0] + y * m->m[1][1] + m->m[1][2]) * inv;
}

static int curve_steps(lv_vector_path_quality_t q)
{
    switch(q) {
        case LV_VECTOR_PATH_QUALITY_LOW:
            return 4;
        case LV_VECTOR_PATH_QUALITY_HIGH:
            return 16;
        case LV_VECTOR_PATH_QUALITY_MEDIUM:
        default:
            return CRT_VEC_CURVE_STEPS_DEFAULT;
    }
}

static bool push_pt(crt_vec_pt_t * pts, uint32_t * n, float x, float y)
{
    if(*n >= CRT_VEC_MAX_PTS) return false;
    if(*n > 0) {
        float dx = x - pts[*n - 1].x;
        float dy = y - pts[*n - 1].y;
        if(dx * dx + dy * dy < 1e-8f) return true;
    }
    pts[*n].x = x;
    pts[*n].y = y;
    (*n)++;
    return true;
}

static void flatten_quad(crt_vec_pt_t * pts, uint32_t * n,
                         float x0, float y0, float x1, float y1, float x2, float y2, int steps)
{
    for(int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.f - t;
        float x = u * u * x0 + 2.f * u * t * x1 + t * t * x2;
        float y = u * u * y0 + 2.f * u * t * y1 + t * t * y2;
        if(!push_pt(pts, n, x, y)) return;
    }
}

static void flatten_cubic(crt_vec_pt_t * pts, uint32_t * n,
                          float x0, float y0, float x1, float y1,
                          float x2, float y2, float x3, float y3, int steps)
{
    for(int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.f - t;
        float uu = u * u;
        float tt = t * t;
        float x = uu * u * x0 + 3.f * uu * t * x1 + 3.f * u * tt * x2 + tt * t * x3;
        float y = uu * u * y0 + 3.f * uu * t * y1 + 3.f * u * tt * y2 + tt * t * y3;
        if(!push_pt(pts, n, x, y)) return;
    }
}

/** Flatten path ops into polyline points (world space after `m`). */
static uint32_t flatten_path(const lv_vector_path_t * path, const lv_matrix_t * m,
                             crt_vec_pt_t * pts, uint32_t cap, bool * closed)
{
    LV_UNUSED(cap);
    const lv_vector_path_op_t * ops = lv_array_front(&path->ops);
    const lv_fpoint_t * point = lv_array_front(&path->points);
    const uint32_t op_size = lv_array_size(&path->ops);
    const int steps = curve_steps(path->quality);
    uint32_t n = 0;
    float cx = 0.f, cy = 0.f;
    float sx = 0.f, sy = 0.f;
    bool have = false;
    *closed = false;

    for(uint32_t i = 0; i < op_size; i++) {
        switch(ops[i]) {
            case LV_VECTOR_PATH_OP_MOVE_TO: {
                    float x, y;
                    xform_pt(m, point->x, point->y, &x, &y);
                    n = 0;
                    push_pt(pts, &n, x, y);
                    cx = sx = x;
                    cy = sy = y;
                    have = true;
                    *closed = false;
                    point++;
                }
                break;
            case LV_VECTOR_PATH_OP_LINE_TO: {
                    float x, y;
                    xform_pt(m, point->x, point->y, &x, &y);
                    push_pt(pts, &n, x, y);
                    cx = x;
                    cy = y;
                    point++;
                }
                break;
            case LV_VECTOR_PATH_OP_QUAD_TO: {
                    float x1, y1, x2, y2;
                    xform_pt(m, point[0].x, point[0].y, &x1, &y1);
                    xform_pt(m, point[1].x, point[1].y, &x2, &y2);
                    flatten_quad(pts, &n, cx, cy, x1, y1, x2, y2, steps);
                    cx = x2;
                    cy = y2;
                    point += 2;
                }
                break;
            case LV_VECTOR_PATH_OP_CUBIC_TO: {
                    float x1, y1, x2, y2, x3, y3;
                    xform_pt(m, point[0].x, point[0].y, &x1, &y1);
                    xform_pt(m, point[1].x, point[1].y, &x2, &y2);
                    xform_pt(m, point[2].x, point[2].y, &x3, &y3);
                    flatten_cubic(pts, &n, cx, cy, x1, y1, x2, y2, x3, y3, steps);
                    cx = x3;
                    cy = y3;
                    point += 3;
                }
                break;
            case LV_VECTOR_PATH_OP_CLOSE: {
                    if(have && n > 0) {
                        push_pt(pts, &n, sx, sy);
                        *closed = true;
                        cx = sx;
                        cy = sy;
                    }
                }
                break;
            default:
                break;
        }
    }
    return n;
}

static float cross(float ax, float ay, float bx, float by, float cx, float cy)
{
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static bool point_in_tri(float px, float py,
                         float ax, float ay, float bx, float by, float cx, float cy)
{
    float c0 = cross(ax, ay, bx, by, px, py);
    float c1 = cross(bx, by, cx, cy, px, py);
    float c2 = cross(cx, cy, ax, ay, px, py);
    bool has_neg = (c0 < 0) || (c1 < 0) || (c2 < 0);
    bool has_pos = (c0 > 0) || (c1 > 0) || (c2 > 0);
    return !(has_neg && has_pos);
}

/** Ear-clip a simple polygon into a triangle list (x,y interleaved). */
static uint32_t ear_clip(const crt_vec_pt_t * in, uint32_t n, float * out_tris, uint32_t out_cap)
{
    if(n < 3) return 0;

    /* Drop duplicate closing vertex if present. */
    if(n > 1) {
        float dx = in[0].x - in[n - 1].x;
        float dy = in[0].y - in[n - 1].y;
        if(dx * dx + dy * dy < 1e-6f) n--;
    }
    if(n < 3) return 0;

    int16_t idx[CRT_VEC_MAX_PTS];
    if(n > CRT_VEC_MAX_PTS) n = CRT_VEC_MAX_PTS;
    for(uint32_t i = 0; i < n; i++) idx[i] = (int16_t)i;

    /* Ensure CCW for consistent ear test. */
    float area = 0.f;
    for(uint32_t i = 0; i < n; i++) {
        uint32_t j = (i + 1) % n;
        area += in[i].x * in[j].y - in[j].x * in[i].y;
    }
    if(area < 0.f) {
        for(uint32_t i = 0; i < n / 2; i++) {
            int16_t tmp = idx[i];
            idx[i] = idx[n - 1 - i];
            idx[n - 1 - i] = tmp;
        }
    }

    uint32_t remaining = n;
    uint32_t out_n = 0;
    uint32_t guard = n * n + 8;

    while(remaining > 3 && guard--) {
        bool clipped = false;
        for(uint32_t i = 0; i < remaining; i++) {
            uint32_t i0 = (i + remaining - 1) % remaining;
            uint32_t i1 = i;
            uint32_t i2 = (i + 1) % remaining;
            const crt_vec_pt_t * a = &in[idx[i0]];
            const crt_vec_pt_t * b = &in[idx[i1]];
            const crt_vec_pt_t * c = &in[idx[i2]];

            if(cross(a->x, a->y, b->x, b->y, c->x, c->y) <= 0.f) continue; /* not convex ear */

            bool ear = true;
            for(uint32_t k = 0; k < remaining; k++) {
                if(k == i0 || k == i1 || k == i2) continue;
                const crt_vec_pt_t * p = &in[idx[k]];
                if(point_in_tri(p->x, p->y, a->x, a->y, b->x, b->y, c->x, c->y)) {
                    ear = false;
                    break;
                }
            }
            if(!ear) continue;

            if(out_n + 6 > out_cap) return out_n / 2;
            out_tris[out_n++] = a->x;
            out_tris[out_n++] = a->y;
            out_tris[out_n++] = b->x;
            out_tris[out_n++] = b->y;
            out_tris[out_n++] = c->x;
            out_tris[out_n++] = c->y;

            for(uint32_t k = i1; k + 1 < remaining; k++) idx[k] = idx[k + 1];
            remaining--;
            clipped = true;
            break;
        }
        if(!clipped) break;
    }

    if(remaining == 3 && out_n + 6 <= out_cap) {
        const crt_vec_pt_t * a = &in[idx[0]];
        const crt_vec_pt_t * b = &in[idx[1]];
        const crt_vec_pt_t * c = &in[idx[2]];
        out_tris[out_n++] = a->x;
        out_tris[out_n++] = a->y;
        out_tris[out_n++] = b->x;
        out_tris[out_n++] = b->y;
        out_tris[out_n++] = c->x;
        out_tris[out_n++] = c->y;
    }

    return out_n / 2; /* vertex count */
}

static void draw_fill_solid(lv_draw_evgpu_c_r_t_unit_t * u, const crt_vec_pt_t * pts, uint32_t n,
                            uint32_t color)
{
    static float tris[CRT_VEC_MAX_TRIS * 2];
    uint32_t vcount = ear_clip(pts, n, tris, sizeof(tris) / sizeof(tris[0]));
    if(vcount < 3) return;
    lv_evgpu_c_r_t_gl_draw_triangles_solid(&u->gl, tris, (int)vcount, color, 255);
}

static void draw_stroke_solid(lv_draw_evgpu_c_r_t_unit_t * u, const crt_vec_pt_t * pts, uint32_t n,
                              float width, uint32_t color, bool closed)
{
    if(n < 2 || width <= 0.f) return;
    float hw = width * 0.5f;
    uint32_t seg_count = closed ? n : (n - 1);
    if(!closed) {
        /* drop duplicate close point for open stroke */
        float dx = pts[0].x - pts[n - 1].x;
        float dy = pts[0].y - pts[n - 1].y;
        if(dx * dx + dy * dy < 1e-6f && n > 2) {
            n--;
            seg_count = n - 1;
        }
    }

    for(uint32_t i = 0; i < seg_count; i++) {
        const crt_vec_pt_t * a = &pts[i];
        const crt_vec_pt_t * b = &pts[(i + 1) % n];
        float dx = b->x - a->x;
        float dy = b->y - a->y;
        float len = sqrtf(dx * dx + dy * dy);
        if(len < 1e-5f) continue;
        float nx = -dy / len * hw;
        float ny = dx / len * hw;

        float verts[12] = {
            a->x + nx, a->y + ny,
            a->x - nx, a->y - ny,
            b->x + nx, b->y + ny,
            a->x - nx, a->y - ny,
            b->x - nx, b->y - ny,
            b->x + nx, b->y + ny,
        };
        lv_evgpu_c_r_t_gl_draw_triangles_solid(&u->gl, verts, 6, color, 255);
    }
}

static void task_draw_cb(void * ctx, const lv_vector_path_t * path, const lv_vector_path_ctx_t * dsc)
{
    crt_vec_ctx_t * c = ctx;
    lv_draw_evgpu_c_r_t_unit_t * u = c->u;

    /* Clear / fill scissor area with no path. */
    if(path == NULL) {
        uint32_t color = color32_to_gl(dsc->fill_dsc.color, dsc->fill_dsc.opa);
        lv_area_t a = dsc->scissor_area;
        lv_evgpu_c_r_t_gl_set_scissor(&u->gl, a.x1, a.y1,
                                       lv_area_get_width(&a), lv_area_get_height(&a));
        lv_evgpu_c_r_t_gl_draw_quad_solid(&u->gl,
                                           (float)a.x1, (float)a.y1,
                                           (float)(a.x2 + 1), (float)(a.y2 + 1),
                                           color, 255);
        lv_evgpu_c_r_t_gl_flush(&u->gl);
        lv_evgpu_c_r_t_gl_disable_scissor();
        return;
    }

    if(dsc->fill_dsc.opa == LV_OPA_TRANSP && dsc->stroke_dsc.opa == LV_OPA_TRANSP) {
        return;
    }

    static crt_vec_pt_t pts[CRT_VEC_MAX_PTS];
    bool closed = false;
    uint32_t n = flatten_path(path, &dsc->matrix, pts, CRT_VEC_MAX_PTS, &closed);
    if(n < 2) return;

    lv_area_t clip = dsc->scissor_area;
    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip.x1, clip.y1,
                                   lv_area_get_width(&clip), lv_area_get_height(&clip));

    if(dsc->fill_dsc.opa > LV_OPA_TRANSP && n >= 3) {
        if(dsc->fill_dsc.style == LV_VECTOR_DRAW_STYLE_SOLID) {
            uint32_t color = color32_to_gl(dsc->fill_dsc.color, dsc->fill_dsc.opa);
            draw_fill_solid(u, pts, n, color);
        }
        else {
            /* P0: non-solid fills fall back to first gradient stop / solid color. */
            lv_color32_t c = dsc->fill_dsc.color;
            if(dsc->fill_dsc.style == LV_VECTOR_DRAW_STYLE_GRADIENT &&
               dsc->fill_dsc.gradient.stops_count > 0) {
                c = lv_color_to_32(dsc->fill_dsc.gradient.stops[0].color,
                                   dsc->fill_dsc.gradient.stops[0].opa);
            }
            uint32_t color = color32_to_gl(c, dsc->fill_dsc.opa);
            draw_fill_solid(u, pts, n, color);
        }
    }

    if(dsc->stroke_dsc.opa > LV_OPA_TRANSP && dsc->stroke_dsc.width > 0.f) {
        uint32_t color = color32_to_gl(dsc->stroke_dsc.color, dsc->stroke_dsc.opa);
        /* P0: ignore dash / stroke gradient. */
        draw_stroke_solid(u, pts, n, dsc->stroke_dsc.width, color, closed);
    }

    lv_evgpu_c_r_t_gl_flush(&u->gl);
    lv_evgpu_c_r_t_gl_disable_scissor();
}

void lv_draw_evgpu_c_r_t_vector(lv_draw_task_t * t, const lv_draw_vector_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;
    if(dsc == NULL || dsc->task_list == NULL) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;
    crt_vec_ctx_t ctx = { .u = u };

    lv_area_t clip = t->clip_area;
    lv_evgpu_c_r_t_gl_set_scissor(&u->gl, clip.x1, clip.y1,
                                   lv_area_get_width(&clip), lv_area_get_height(&clip));

    lv_vector_for_each_destroy_tasks(dsc->task_list, task_draw_cb, &ctx);

    lv_evgpu_c_r_t_gl_flush(&u->gl);
    lv_evgpu_c_r_t_gl_disable_scissor();
    LV_PROFILER_DRAW_END;
}

#endif /* LV_USE_DRAW_EVGPU_C_R_T && LV_USE_VECTOR_GRAPHIC */
