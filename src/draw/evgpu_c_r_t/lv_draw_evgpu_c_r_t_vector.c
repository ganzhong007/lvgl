/**
 * @file lv_draw_evgpu_c_r_t_vector.c
 *
 * VECTOR for EVGPU_C_R_T: flatten curves → ear-clip fill + thick-segment stroke.
 * P0: solid fill/stroke via GLES2 solid shaders.
 * P1: linear/radial multi-stop fill + image pattern via 1D/2D tex + triangle UVs.
 * P2: dash pattern, round/square caps, miter/bevel/round joins, light fringe AA.
 * P3.1: stroke linear/radial gradient (same 1D ramp + UV as fill).
 * P3.2: tess — dual-winding ear-clip + centroid fan fallback.
 * P3.3a: pattern fill — OBJECT_BOUNDING_BOX UV origin (match EVGPU/SW).
 */

#include "../../drivers/opengles/lv_opengles_private.h"
#define LV_EVGPU_C_R_T_SKIP_SYSTEM_GLES 1
#include "lv_draw_evgpu_c_r_t_private.h"

#if LV_USE_DRAW_EVGPU_C_R_T && LV_USE_VECTOR_GRAPHIC

#include "../lv_draw_vector_private.h"
#include "../lv_image_decoder_private.h"
#include <math.h>
#include <float.h>
#include <string.h>
#include <GLES2/gl2.h>

#ifndef GL_BGRA
    #ifdef GL_BGRA_EXT
        #define GL_BGRA GL_BGRA_EXT
    #else
        #define GL_BGRA 0x80E1
    #endif
#endif

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
            return 12;
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

/** Ear-clip a simple polygon into a triangle list (x,y interleaved).
 *  Returns vertex count (multiples of 3). May leave gaps if stuck — caller
 *  should fall back via tess_polygon(). */
static uint32_t ear_clip_oriented(const crt_vec_pt_t * in, uint32_t n, float * out_tris,
                                  uint32_t out_cap, bool force_ccw)
{
    if(n < 3) return 0;

    if(n > 1) {
        float dx = in[0].x - in[n - 1].x;
        float dy = in[0].y - in[n - 1].y;
        if(dx * dx + dy * dy < 1e-6f) n--;
    }
    if(n < 3) return 0;
    if(n > CRT_VEC_MAX_PTS) n = CRT_VEC_MAX_PTS;

    int16_t idx[CRT_VEC_MAX_PTS];
    for(uint32_t i = 0; i < n; i++) idx[i] = (int16_t)i;

    float area = 0.f;
    for(uint32_t i = 0; i < n; i++) {
        uint32_t j = (i + 1) % n;
        area += in[i].x * in[j].y - in[j].x * in[i].y;
    }
    bool want_flip = force_ccw ? (area < 0.f) : (area > 0.f);
    if(want_flip) {
        for(uint32_t i = 0; i < n / 2; i++) {
            int16_t tmp = idx[i];
            idx[i] = idx[n - 1 - i];
            idx[n - 1 - i] = tmp;
        }
    }

    uint32_t remaining = n;
    uint32_t out_n = 0;
    uint32_t guard = n * n + 16;
    const float eps = -1e-5f;

    while(remaining > 3 && guard--) {
        bool clipped = false;
        for(uint32_t i = 0; i < remaining; i++) {
            uint32_t i0 = (i + remaining - 1) % remaining;
            uint32_t i1 = i;
            uint32_t i2 = (i + 1) % remaining;
            const crt_vec_pt_t * a = &in[idx[i0]];
            const crt_vec_pt_t * b = &in[idx[i1]];
            const crt_vec_pt_t * c = &in[idx[i2]];

            if(cross(a->x, a->y, b->x, b->y, c->x, c->y) <= eps) continue;

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
        remaining = 0;
    }

    /* Stash leftover count in a side channel via out_cap trick — not used.
     * Caller checks expected triangle count. */
    LV_UNUSED(remaining);
    return out_n / 2;
}

/** Fan from centroid — P3.2 fallback for concave / weakly self-intersecting paths. */
static uint32_t fan_from_centroid(const crt_vec_pt_t * in, uint32_t n, float * out_tris, uint32_t out_cap)
{
    if(n < 3) return 0;
    if(n > 1) {
        float dx = in[0].x - in[n - 1].x;
        float dy = in[0].y - in[n - 1].y;
        if(dx * dx + dy * dy < 1e-6f) n--;
    }
    if(n < 3) return 0;

    float cx = 0.f, cy = 0.f;
    for(uint32_t i = 0; i < n; i++) {
        cx += in[i].x;
        cy += in[i].y;
    }
    cx /= (float)n;
    cy /= (float)n;

    uint32_t out_n = 0;
    for(uint32_t i = 0; i < n; i++) {
        uint32_t j = (i + 1) % n;
        if(out_n + 6 > out_cap) break;
        out_tris[out_n++] = cx;
        out_tris[out_n++] = cy;
        out_tris[out_n++] = in[i].x;
        out_tris[out_n++] = in[i].y;
        out_tris[out_n++] = in[j].x;
        out_tris[out_n++] = in[j].y;
    }
    return out_n / 2;
}

/** P3.2 tess: ear-clip (both windings) then centroid fan fallback. */
static uint32_t tess_polygon(const crt_vec_pt_t * in, uint32_t n, float * out_tris, uint32_t out_cap)
{
    uint32_t nn = n;
    if(nn > 1) {
        float dx = in[0].x - in[nn - 1].x;
        float dy = in[0].y - in[nn - 1].y;
        if(dx * dx + dy * dy < 1e-6f) nn--;
    }
    uint32_t need = (nn >= 3) ? (nn - 2) * 3u : 0;

    uint32_t v = ear_clip_oriented(in, n, out_tris, out_cap, true);
    if(v >= need && v >= 3) return v;

    v = ear_clip_oriented(in, n, out_tris, out_cap, false);
    if(v >= need && v >= 3) return v;

    return fan_from_centroid(in, n, out_tris, out_cap);
}

static void draw_fill_solid(lv_draw_evgpu_c_r_t_unit_t * u, const crt_vec_pt_t * pts, uint32_t n,
                            uint32_t color)
{
    static float tris[CRT_VEC_MAX_TRIS * 2];
    uint32_t vcount = tess_polygon(pts, n, tris, sizeof(tris) / sizeof(tris[0]));
    if(vcount < 3) return;
    lv_evgpu_c_r_t_gl_draw_triangles_solid(&u->gl, tris, (int)vcount, color, 255);
}

static float apply_grad_spread(float t, lv_vector_gradient_spread_t spread)
{
    switch(spread) {
        case LV_VECTOR_GRADIENT_SPREAD_REPEAT:
            t = t - floorf(t);
            break;
        case LV_VECTOR_GRADIENT_SPREAD_REFLECT: {
                float p = t - 2.f * floorf(t * 0.5f);
                t = (p > 1.f) ? (2.f - p) : p;
                if(t < 0.f) t = -t;
            }
            break;
        case LV_VECTOR_GRADIENT_SPREAD_PAD:
        default:
            if(t < 0.f) t = 0.f;
            if(t > 1.f) t = 1.f;
            break;
    }
    return t;
}

/** Map fill-local (gx,gy) → world using fill_matrix then path_matrix. */
static void fill_to_world(const lv_matrix_t * path_m, const lv_matrix_t * fill_m,
                          float gx, float gy, float * ox, float * oy)
{
    float tx, ty;
    xform_pt(fill_m, gx, gy, &tx, &ty);
    xform_pt(path_m, tx, ty, ox, oy);
}

static bool build_xyuv_from_tris(const float * tris_xy, uint32_t vcount,
                                 float * xyuv, uint32_t xyuv_cap_floats,
                                 float (*uv_fn)(float x, float y, void * ud), void * ud)
{
    if(vcount * 4 > xyuv_cap_floats) return false;
    for(uint32_t i = 0; i < vcount; i++) {
        float x = tris_xy[i * 2];
        float y = tris_xy[i * 2 + 1];
        float t = uv_fn(x, y, ud);
        xyuv[i * 4 + 0] = x;
        xyuv[i * 4 + 1] = y;
        xyuv[i * 4 + 2] = t;
        xyuv[i * 4 + 3] = 0.5f;
    }
    return true;
}

typedef struct {
    float x0, y0, dx, dy, inv_len2;
    lv_vector_gradient_spread_t spread;
} crt_vec_lin_uv_t;

static float lin_uv_fn(float x, float y, void * ud)
{
    crt_vec_lin_uv_t * L = ud;
    float t = ((x - L->x0) * L->dx + (y - L->y0) * L->dy) * L->inv_len2;
    return apply_grad_spread(t, L->spread);
}

typedef struct {
    float cx, cy, inv_r;
    lv_vector_gradient_spread_t spread;
} crt_vec_rad_uv_t;

static float rad_uv_fn(float x, float y, void * ud)
{
    crt_vec_rad_uv_t * R = ud;
    float dx = x - R->cx;
    float dy = y - R->cy;
    float t = sqrtf(dx * dx + dy * dy) * R->inv_r;
    return apply_grad_spread(t, R->spread);
}

typedef struct {
    lv_matrix_t inv_path;
    lv_matrix_t inv_fill;
    bool have_inv_path;
    bool have_inv_fill;
    float origin_x;
    float origin_y;
    float img_w;
    float img_h;
} crt_vec_pat_uv_t;

/** Local-space path bounds (ignore path matrix) — for OBJECT_BOUNDING_BOX pattern origin. */
static void path_local_bounds(const lv_vector_path_t * path, float * min_x, float * min_y,
                              float * max_x, float * max_y)
{
    *min_x = FLT_MAX;
    *min_y = FLT_MAX;
    *max_x = -FLT_MAX;
    *max_y = -FLT_MAX;
    const lv_fpoint_t * point = lv_array_front(&path->points);
    const uint32_t n = lv_array_size(&path->points);
    for(uint32_t i = 0; i < n; i++) {
        float x = point[i].x;
        float y = point[i].y;
        if(x < *min_x) *min_x = x;
        if(y < *min_y) *min_y = y;
        if(x > *max_x) *max_x = x;
        if(y > *max_y) *max_y = y;
    }
    if(n == 0) {
        *min_x = *min_y = 0.f;
        *max_x = *max_y = 0.f;
    }
}

static void pat_uv_pair(float x, float y, void * ud, float * u, float * v)
{
    crt_vec_pat_uv_t * P = ud;
    float lx = x, ly = y;
    if(P->have_inv_path) xform_pt(&P->inv_path, x, y, &lx, &ly);
    /* Match EVGPU/SW: image origin at path bbox min when OBJECT_BOUNDING_BOX. */
    lx -= P->origin_x;
    ly -= P->origin_y;
    float fx = lx, fy = ly;
    if(P->have_inv_fill) xform_pt(&P->inv_fill, lx, ly, &fx, &fy);
    *u = (P->img_w > 0.f) ? (fx / P->img_w) : 0.f;
    *v = (P->img_h > 0.f) ? (fy / P->img_h) : 0.f;
}

static void draw_fill_gradient(lv_draw_evgpu_c_r_t_unit_t * u, const crt_vec_pt_t * pts, uint32_t n,
                               const lv_vector_fill_dsc_t * fill, const lv_matrix_t * path_m)
{
    const lv_vector_gradient_t * g = &fill->gradient;
    if(g->stops_count == 0) {
        uint32_t color = color32_to_gl(fill->color, fill->opa);
        draw_fill_solid(u, pts, n, color);
        return;
    }

    static float tris[CRT_VEC_MAX_TRIS * 2];
    static float xyuv[CRT_VEC_MAX_TRIS * 4];
    uint32_t vcount = tess_polygon(pts, n, tris, sizeof(tris) / sizeof(tris[0]));
    if(vcount < 3) return;

    GLuint tex = lv_evgpu_c_r_t_gl_create_grad_tex_stops(g->stops, g->stops_count, fill->opa);
    if(tex == 0) {
        lv_color32_t c = lv_color_to_32(g->stops[0].color, g->stops[0].opa);
        draw_fill_solid(u, pts, n, color32_to_gl(c, fill->opa));
        return;
    }

    bool ok = false;
    if(g->style == LV_VECTOR_GRADIENT_STYLE_RADIAL) {
        float wcx, wcy, wrx, wry;
        fill_to_world(path_m, &fill->matrix, g->cx, g->cy, &wcx, &wcy);
        fill_to_world(path_m, &fill->matrix, g->cx + g->cr, g->cy, &wrx, &wry);
        float rr = sqrtf((wrx - wcx) * (wrx - wcx) + (wry - wcy) * (wry - wcy));
        if(rr < 1e-3f) rr = 1.f;
        crt_vec_rad_uv_t R = { .cx = wcx, .cy = wcy, .inv_r = 1.f / rr, .spread = g->spread };
        ok = build_xyuv_from_tris(tris, vcount, xyuv, sizeof(xyuv) / sizeof(xyuv[0]), rad_uv_fn, &R);
    }
    else {
        float x0, y0, x1, y1;
        fill_to_world(path_m, &fill->matrix, g->x1, g->y1, &x0, &y0);
        fill_to_world(path_m, &fill->matrix, g->x2, g->y2, &x1, &y1);
        float dx = x1 - x0;
        float dy = y1 - y0;
        float len2 = dx * dx + dy * dy;
        if(len2 < 1e-6f) {
            lv_color32_t c = lv_color_to_32(g->stops[0].color, g->stops[0].opa);
            draw_fill_solid(u, pts, n, color32_to_gl(c, fill->opa));
            glDeleteTextures(1, &tex);
            return;
        }
        crt_vec_lin_uv_t L = {
            .x0 = x0, .y0 = y0, .dx = dx, .dy = dy, .inv_len2 = 1.f / len2, .spread = g->spread
        };
        ok = build_xyuv_from_tris(tris, vcount, xyuv, sizeof(xyuv) / sizeof(xyuv[0]), lin_uv_fn, &L);
    }

    if(ok) {
        lv_evgpu_c_r_t_gl_draw_triangles_tex(&u->gl, xyuv, (int)vcount, tex, 0, 0, 255, true);
    }
    glDeleteTextures(1, &tex);
}

static bool upload_pattern_tex(const lv_draw_image_dsc_t * img_dsc, GLuint * out_tex,
                               int32_t * out_w, int32_t * out_h)
{
    lv_image_decoder_dsc_t decoder;
    lv_result_t res = lv_image_decoder_open(&decoder, img_dsc->src, NULL);
    if(res != LV_RESULT_OK || decoder.decoded == NULL || decoder.decoded->data == NULL) {
        if(res == LV_RESULT_OK) lv_image_decoder_close(&decoder);
        return false;
    }

    const lv_draw_buf_t * decoded = decoder.decoded;
    int32_t img_w = (int32_t)decoded->header.w;
    int32_t img_h = (int32_t)decoded->header.h;
    lv_color_format_t cf = decoded->header.cf;
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
    GLint internal = GL_RGBA;

    switch(cf) {
        case LV_COLOR_FORMAT_ARGB8888:
        case LV_COLOR_FORMAT_XRGB8888:
        case LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:
            format = GL_BGRA;
            internal = GL_BGRA;
            break;
        case LV_COLOR_FORMAT_RGB888:
            format = GL_RGB;
            internal = GL_RGB;
            break;
        case LV_COLOR_FORMAT_RGB565:
            format = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
            internal = GL_RGB;
            break;
        case LV_COLOR_FORMAT_A8:
            format = GL_ALPHA;
            internal = GL_ALPHA;
            break;
        case LV_COLOR_FORMAT_L8:
            format = GL_LUMINANCE;
            internal = GL_LUMINANCE;
            break;
        default:
            format = GL_BGRA;
            internal = GL_BGRA;
            break;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, img_w, img_h, 0, format, type, decoded->data);

    lv_image_decoder_close(&decoder);
    *out_tex = texture;
    *out_w = img_w;
    *out_h = img_h;
    return true;
}

static void draw_fill_pattern(lv_draw_evgpu_c_r_t_unit_t * u, const crt_vec_pt_t * pts, uint32_t n,
                              const lv_vector_fill_dsc_t * fill, const lv_matrix_t * path_m,
                              const lv_vector_path_t * path)
{
    GLuint tex = 0;
    int32_t img_w = 0, img_h = 0;
    if(!upload_pattern_tex(&fill->img_dsc, &tex, &img_w, &img_h)) {
        uint32_t color = color32_to_gl(fill->color, fill->opa);
        draw_fill_solid(u, pts, n, color);
        return;
    }

    static float tris[CRT_VEC_MAX_TRIS * 2];
    static float xyuv[CRT_VEC_MAX_TRIS * 4];
    uint32_t vcount = tess_polygon(pts, n, tris, sizeof(tris) / sizeof(tris[0]));
    if(vcount < 3) {
        glDeleteTextures(1, &tex);
        return;
    }

    crt_vec_pat_uv_t P;
    lv_matrix_identity(&P.inv_path);
    lv_matrix_identity(&P.inv_fill);
    P.have_inv_path = lv_matrix_inverse(&P.inv_path, path_m);
    P.have_inv_fill = lv_matrix_inverse(&P.inv_fill, &fill->matrix);
    P.origin_x = 0.f;
    P.origin_y = 0.f;
    if(fill->fill_units == LV_VECTOR_FILL_UNITS_OBJECT_BOUNDING_BOX && path != NULL) {
        float bx0, by0, bx1, by1;
        path_local_bounds(path, &bx0, &by0, &bx1, &by1);
        P.origin_x = bx0;
        P.origin_y = by0;
    }
    P.img_w = (float)img_w;
    P.img_h = (float)img_h;

    for(uint32_t i = 0; i < vcount; i++) {
        float x = tris[i * 2];
        float y = tris[i * 2 + 1];
        float uu, vv;
        pat_uv_pair(x, y, &P, &uu, &vv);
        xyuv[i * 4 + 0] = x;
        xyuv[i * 4 + 1] = y;
        xyuv[i * 4 + 2] = uu;
        xyuv[i * 4 + 3] = vv;
    }

    uint32_t recolor = lv_evgpu_c_r_t_color_to_gl_alpha(fill->img_dsc.recolor, fill->img_dsc.recolor_opa);
    lv_evgpu_c_r_t_gl_draw_triangles_tex(&u->gl, xyuv, (int)vcount, tex, recolor,
                                         fill->img_dsc.recolor_opa, fill->opa, false);
    glDeleteTextures(1, &tex);
}

typedef struct {
    lv_draw_evgpu_c_r_t_unit_t * u;
    bool use_grad;
    uint32_t solid_color;
    GLuint grad_tex;
    float (*t_fn)(float x, float y, void * ud);
    void * t_ud;
} crt_vec_stroke_paint_t;

static float stroke_t_at(const crt_vec_stroke_paint_t * p, float x, float y)
{
    if(!p->use_grad || p->t_fn == NULL) return 0.f;
    return p->t_fn(x, y, p->t_ud);
}

static void paint_tri3(const crt_vec_stroke_paint_t * p,
                       float x0, float y0, float x1, float y1, float x2, float y2)
{
    if(p->use_grad && p->grad_tex) {
        float xyuv[12] = {
            x0, y0, stroke_t_at(p, x0, y0), 0.5f,
            x1, y1, stroke_t_at(p, x1, y1), 0.5f,
            x2, y2, stroke_t_at(p, x2, y2), 0.5f,
        };
        lv_evgpu_c_r_t_gl_draw_triangles_tex(&p->u->gl, xyuv, 3, p->grad_tex, 0, 0, 255, true);
    }
    else {
        float verts[6] = { x0, y0, x1, y1, x2, y2 };
        lv_evgpu_c_r_t_gl_draw_triangles_solid(&p->u->gl, verts, 3, p->solid_color, 255);
    }
}

static void paint_tri6(const crt_vec_stroke_paint_t * p,
                       float ax, float ay, float bx, float by,
                       float cx, float cy, float dx, float dy, float ex, float ey, float fx, float fy)
{
    paint_tri3(p, ax, ay, bx, by, cx, cy);
    paint_tri3(p, dx, dy, ex, ey, fx, fy);
}

static void emit_seg(const crt_vec_stroke_paint_t * p,
                     float ax, float ay, float bx, float by,
                     float nx, float ny)
{
    paint_tri6(p,
               ax + nx, ay + ny, ax - nx, ay - ny, bx + nx, by + ny,
               ax - nx, ay - ny, bx - nx, by - ny, bx + nx, by + ny);
}

static void emit_disc(const crt_vec_stroke_paint_t * p, float cx, float cy, float r)
{
    const int segs = 12;
    float prev_x = cx + r;
    float prev_y = cy;
    for(int i = 1; i <= segs; i++) {
        float a = (float)i * (2.f * (float)M_PI / (float)segs);
        float x = cx + cosf(a) * r;
        float y = cy + sinf(a) * r;
        paint_tri3(p, cx, cy, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
}

static void emit_cap(const crt_vec_stroke_paint_t * p,
                     float x, float y, float tx, float ty, float hw,
                     lv_vector_stroke_cap_t cap, bool at_start)
{
    if(cap == LV_VECTOR_STROKE_CAP_BUTT) return;

    float nx = -ty * hw;
    float ny = tx * hw;
    if(cap == LV_VECTOR_STROKE_CAP_SQUARE) {
        float sx = (at_start ? -tx : tx) * hw;
        float sy = (at_start ? -ty : ty) * hw;
        float px = x + sx;
        float py = y + sy;
        paint_tri6(p,
                   x + nx, y + ny, x - nx, y - ny, px + nx, py + ny,
                   x - nx, y - ny, px - nx, py - ny, px + nx, py + ny);
    }
    else {
        emit_disc(p, x, y, hw);
    }
}

static void emit_join(const crt_vec_stroke_paint_t * p,
                      float x, float y,
                      float in_tx, float in_ty, float out_tx, float out_ty,
                      float hw, lv_vector_stroke_join_t join, uint16_t miter_limit)
{
    float in_nx = -in_ty * hw;
    float in_ny = in_tx * hw;
    float out_nx = -out_ty * hw;
    float out_ny = out_tx * hw;

    float cross = in_tx * out_ty - in_ty * out_tx;
    if(LV_ABS(cross) < 1e-5f) return;

    float ax, ay, bx, by;
    if(cross > 0.f) {
        ax = x + in_nx;
        ay = y + in_ny;
        bx = x + out_nx;
        by = y + out_ny;
    }
    else {
        ax = x - in_nx;
        ay = y - in_ny;
        bx = x - out_nx;
        by = y - out_ny;
    }

    if(join == LV_VECTOR_STROKE_JOIN_ROUND) {
        emit_disc(p, x, y, hw);
        return;
    }

    if(join == LV_VECTOR_STROKE_JOIN_MITER) {
        float det = in_tx * (-out_ty) - in_ty * (-out_tx);
        if(LV_ABS(det) > 1e-5f) {
            float tt = ((bx - ax) * (-out_ty) - (by - ay) * (-out_tx)) / det;
            float mx = ax + in_tx * tt;
            float my = ay + in_ty * tt;
            float mdx = mx - x;
            float mdy = my - y;
            float mlen = sqrtf(mdx * mdx + mdy * mdy);
            float limit = (miter_limit > 0 ? (float)miter_limit : 4.f) * hw;
            if(mlen <= limit) {
                paint_tri3(p, x, y, ax, ay, mx, my);
                paint_tri3(p, x, y, mx, my, bx, by);
                return;
            }
        }
    }

    paint_tri3(p, x, y, ax, ay, bx, by);
}

static void normalize2(float * x, float * y)
{
    float len = sqrtf((*x) * (*x) + (*y) * (*y));
    if(len < 1e-6f) {
        *x = 1.f;
        *y = 0.f;
        return;
    }
    *x /= len;
    *y /= len;
}

static uint32_t color_scale_alpha(uint32_t color, float scale)
{
    uint8_t a = (uint8_t)(((color >> 24) & 0xFF) * scale);
    return (color & 0x00FFFFFFu) | ((uint32_t)a << 24);
}

static void stroke_polyline_core(const crt_vec_stroke_paint_t * paint, const crt_vec_pt_t * pts, uint32_t n,
                                 bool closed, float width,
                                 lv_vector_stroke_cap_t cap, lv_vector_stroke_join_t join,
                                 uint16_t miter_limit)
{
    if(n < 2 || width <= 0.f) return;

    if(!closed && n > 2) {
        float dx = pts[0].x - pts[n - 1].x;
        float dy = pts[0].y - pts[n - 1].y;
        if(dx * dx + dy * dy < 1e-6f) n--;
    }
    if(n < 2) return;

    float hw = width * 0.5f;
    uint32_t seg_count = closed ? n : (n - 1);

    float tx[CRT_VEC_MAX_PTS];
    float ty[CRT_VEC_MAX_PTS];
    for(uint32_t i = 0; i < seg_count; i++) {
        const crt_vec_pt_t * a = &pts[i];
        const crt_vec_pt_t * b = &pts[(i + 1) % n];
        float dx = b->x - a->x;
        float dy = b->y - a->y;
        normalize2(&dx, &dy);
        tx[i] = dx;
        ty[i] = dy;
    }

    for(uint32_t i = 0; i < seg_count; i++) {
        const crt_vec_pt_t * a = &pts[i];
        const crt_vec_pt_t * b = &pts[(i + 1) % n];
        float nx = -ty[i] * hw;
        float ny = tx[i] * hw;
        emit_seg(paint, a->x, a->y, b->x, b->y, nx, ny);
    }

    if(closed) {
        for(uint32_t i = 0; i < n; i++) {
            uint32_t ip = (i + n - 1) % n;
            emit_join(paint, pts[i].x, pts[i].y,
                      tx[ip], ty[ip], tx[i], ty[i],
                      hw, join, miter_limit);
        }
    }
    else {
        for(uint32_t i = 1; i + 1 < n; i++) {
            emit_join(paint, pts[i].x, pts[i].y,
                      tx[i - 1], ty[i - 1], tx[i], ty[i],
                      hw, join, miter_limit);
        }
        emit_cap(paint, pts[0].x, pts[0].y, tx[0], ty[0], hw, cap, true);
        emit_cap(paint, pts[n - 1].x, pts[n - 1].y, tx[seg_count - 1], ty[seg_count - 1], hw, cap, false);
    }
}

static void stroke_polyline(const crt_vec_stroke_paint_t * paint, const crt_vec_pt_t * pts, uint32_t n,
                            bool closed, const lv_vector_stroke_dsc_t * stroke)
{
    if(paint->use_grad) {
        /* Grad fringe would need a dimmer ramp; keep core stroke only. */
        stroke_polyline_core(paint, pts, n, closed, stroke->width,
                             stroke->cap, stroke->join, stroke->miter_limit);
        return;
    }

    crt_vec_stroke_paint_t fringe = *paint;
    fringe.solid_color = color_scale_alpha(paint->solid_color, 0.35f);
    stroke_polyline_core(&fringe, pts, n, closed, stroke->width + 1.25f,
                         stroke->cap, stroke->join, stroke->miter_limit);
    stroke_polyline_core(paint, pts, n, closed, stroke->width,
                         stroke->cap, stroke->join, stroke->miter_limit);
}

static void stroke_dashed(const crt_vec_stroke_paint_t * paint, const crt_vec_pt_t * pts, uint32_t n,
                          bool closed, const lv_vector_stroke_dsc_t * stroke)
{
    const float * pattern = lv_array_front(&stroke->dash_pattern);
    const uint32_t pattern_count = lv_array_size(&stroke->dash_pattern);
    if(pattern == NULL || pattern_count == 0) {
        stroke_polyline(paint, pts, n, closed, stroke);
        return;
    }

    if(!closed && n > 2) {
        float dx = pts[0].x - pts[n - 1].x;
        float dy = pts[0].y - pts[n - 1].y;
        if(dx * dx + dy * dy < 1e-6f) n--;
    }
    if(n < 2) return;

    uint32_t pat_idx = 0;
    float pat_remaining = pattern[0];
    bool drawing = true;
    crt_vec_pt_t cur = pts[0];

    uint32_t seg_end = closed ? n : (n - 1);
    for(uint32_t i = 0; i < seg_end; i++) {
        crt_vec_pt_t end = pts[(i + 1) % n];
        float dx = end.x - cur.x;
        float dy = end.y - cur.y;
        float seg_len = sqrtf(dx * dx + dy * dy);
        if(seg_len < 1e-4f) {
            cur = end;
            continue;
        }
        float ux = dx / seg_len;
        float uy = dy / seg_len;
        float traveled = 0.f;

        while(traveled < seg_len - 1e-4f) {
            float step = pat_remaining;
            if(step > seg_len - traveled) step = seg_len - traveled;
            float x0 = cur.x + ux * traveled;
            float y0 = cur.y + uy * traveled;
            float x1 = cur.x + ux * (traveled + step);
            float y1 = cur.y + uy * (traveled + step);

            if(drawing && step > 1e-4f) {
                crt_vec_pt_t dash_pts[2] = { { x0, y0 }, { x1, y1 } };
                lv_vector_stroke_dsc_t tmp = *stroke;
                if(tmp.cap == LV_VECTOR_STROKE_CAP_BUTT) tmp.cap = LV_VECTOR_STROKE_CAP_ROUND;
                stroke_polyline(paint, dash_pts, 2, false, &tmp);
            }

            traveled += step;
            pat_remaining -= step;
            if(pat_remaining <= 1e-4f) {
                pat_idx = (pat_idx + 1) % pattern_count;
                pat_remaining = pattern[pat_idx];
                if(pat_remaining < 1e-4f) pat_remaining = 1e-3f;
                drawing = (pat_idx % 2) == 0;
            }
        }
        cur = end;
    }
}

static bool setup_stroke_grad_paint(crt_vec_stroke_paint_t * paint,
                                    lv_draw_evgpu_c_r_t_unit_t * u,
                                    const lv_vector_stroke_dsc_t * stroke,
                                    const lv_matrix_t * path_m,
                                    crt_vec_lin_uv_t * lin_ud,
                                    crt_vec_rad_uv_t * rad_ud,
                                    GLuint * out_tex)
{
    const lv_vector_gradient_t * g = &stroke->gradient;
    if(g->stops_count == 0) return false;

    GLuint tex = lv_evgpu_c_r_t_gl_create_grad_tex_stops(g->stops, g->stops_count, stroke->opa);
    if(tex == 0) return false;

    paint->u = u;
    paint->use_grad = true;
    paint->solid_color = 0;
    paint->grad_tex = tex;
    *out_tex = tex;

    if(g->style == LV_VECTOR_GRADIENT_STYLE_RADIAL) {
        float wcx, wcy, wrx, wry;
        fill_to_world(path_m, &stroke->matrix, g->cx, g->cy, &wcx, &wcy);
        fill_to_world(path_m, &stroke->matrix, g->cx + g->cr, g->cy, &wrx, &wry);
        float rr = sqrtf((wrx - wcx) * (wrx - wcx) + (wry - wcy) * (wry - wcy));
        if(rr < 1e-3f) rr = 1.f;
        rad_ud->cx = wcx;
        rad_ud->cy = wcy;
        rad_ud->inv_r = 1.f / rr;
        rad_ud->spread = g->spread;
        paint->t_fn = rad_uv_fn;
        paint->t_ud = rad_ud;
    }
    else {
        float x0, y0, x1, y1;
        fill_to_world(path_m, &stroke->matrix, g->x1, g->y1, &x0, &y0);
        fill_to_world(path_m, &stroke->matrix, g->x2, g->y2, &x1, &y1);
        float dx = x1 - x0;
        float dy = y1 - y0;
        float len2 = dx * dx + dy * dy;
        if(len2 < 1e-6f) {
            glDeleteTextures(1, &tex);
            *out_tex = 0;
            return false;
        }
        lin_ud->x0 = x0;
        lin_ud->y0 = y0;
        lin_ud->dx = dx;
        lin_ud->dy = dy;
        lin_ud->inv_len2 = 1.f / len2;
        lin_ud->spread = g->spread;
        paint->t_fn = lin_uv_fn;
        paint->t_ud = lin_ud;
    }
    return true;
}

static void draw_stroke(lv_draw_evgpu_c_r_t_unit_t * u, const crt_vec_pt_t * pts, uint32_t n,
                        bool closed, const lv_vector_stroke_dsc_t * stroke, const lv_matrix_t * path_m)
{
    crt_vec_stroke_paint_t paint;
    crt_vec_lin_uv_t lin_ud;
    crt_vec_rad_uv_t rad_ud;
    GLuint grad_tex = 0;
    memset(&paint, 0, sizeof(paint));
    paint.u = u;

    if(stroke->style == LV_VECTOR_DRAW_STYLE_GRADIENT &&
       setup_stroke_grad_paint(&paint, u, stroke, path_m, &lin_ud, &rad_ud, &grad_tex)) {
        /* ok */
    }
    else {
        paint.use_grad = false;
        paint.solid_color = color32_to_gl(stroke->color, stroke->opa);
        paint.grad_tex = 0;
        paint.t_fn = NULL;
        paint.t_ud = NULL;
    }

    if(!lv_array_is_empty(&stroke->dash_pattern)) {
        stroke_dashed(&paint, pts, n, closed, stroke);
    }
    else {
        stroke_polyline(&paint, pts, n, closed, stroke);
    }

    if(grad_tex) glDeleteTextures(1, &grad_tex);
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
        switch(dsc->fill_dsc.style) {
            case LV_VECTOR_DRAW_STYLE_GRADIENT:
                draw_fill_gradient(u, pts, n, &dsc->fill_dsc, &dsc->matrix);
                break;
            case LV_VECTOR_DRAW_STYLE_PATTERN:
                draw_fill_pattern(u, pts, n, &dsc->fill_dsc, &dsc->matrix, path);
                break;
            case LV_VECTOR_DRAW_STYLE_SOLID:
            default: {
                    uint32_t color = color32_to_gl(dsc->fill_dsc.color, dsc->fill_dsc.opa);
                    draw_fill_solid(u, pts, n, color);
                }
                break;
        }
    }

    if(dsc->stroke_dsc.opa > LV_OPA_TRANSP && dsc->stroke_dsc.width > 0.f) {
        draw_stroke(u, pts, n, closed, &dsc->stroke_dsc, &dsc->matrix);
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
