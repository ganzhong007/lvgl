/**
 * @file lv_gpu_renderer_gles2_vector.c — GLES2 vector paths (stencil fill, gradient, dash stroke)
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER && LV_USE_VECTOR_GRAPHIC

#include "lv_gpu_renderer_gles2_vector.h"
#include "lv_draw_gpu_renderer.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../stdlib/lv_mem.h"
#include "../../misc/lv_math.h"
#include "../../misc/lv_area_private.h"
#include "../../include/lvgl/draw/lv_grad.h"
#include <math.h>
#include <string.h>

#define GPU_VEC_MAX_PTS   4096
#define GPU_VEC_MAX_TRIS  8192
#define GPU_VEC_FLAT_TOL  0.25f
#define GPU_VEC_MAX_STOPS 8
#define GPU_VEC_BATCH_VERTS (GPU_VEC_MAX_TRIS * 3)

static unsigned int prog_tri;
static int loc_tri_disp;
static int loc_tri_color;
static int loc_tri_grad_on;
static int loc_tri_grad_style;
static int loc_tri_color1;
static int loc_tri_grad_p0;
static int loc_tri_grad_p1;
static int loc_tri_grad_cx;
static int loc_tri_grad_cy;
static int loc_tri_grad_r0;
static int loc_tri_grad_r1;
static int loc_tri_grad_lut;

#if LV_USE_EGL
static const char * vs_vec =
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_pos;\n"
    "void main(){\n"
    "  v_pos=a_pos;\n"
    "  vec2 ndc;\n"
    "  ndc.x=a_pos.x/u_disp.x*2.0-1.0;\n"
    "  ndc.y=a_pos.y/u_disp.y*2.0-1.0;\n"
    "  gl_Position=vec4(ndc,0.0,1.0);\n"
    "}\n";

static const char * fs_vec =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform int u_grad_on;\n"
    "uniform int u_grad_style;\n"
    "uniform vec4 u_color1;\n"
    "uniform vec2 u_grad_p0;\n"
    "uniform vec2 u_grad_p1;\n"
    "uniform float u_grad_cx;\n"
    "uniform float u_grad_cy;\n"
    "uniform float u_grad_r0;\n"
    "uniform float u_grad_r1;\n"
    "uniform sampler2D u_grad_lut;\n"
    "varying vec2 v_pos;\n"
    "void main(){\n"
    "  vec4 base=u_color;\n"
    "  if(u_grad_on==1){\n"
    "    float t=0.0;\n"
    "    if(u_grad_style==1){\n"
    "      vec2 ab=u_grad_p1-u_grad_p0;\n"
    "      t=dot(v_pos-u_grad_p0,ab)/max(dot(ab,ab),1.0);\n"
    "    } else {\n"
    "      float d=length(v_pos-vec2(u_grad_cx,u_grad_cy));\n"
    "      t=(d-u_grad_r0)/max(u_grad_r1-u_grad_r0,0.001);\n"
    "    }\n"
    "    t=clamp(t,0.0,1.0);\n"
    "    base=texture2D(u_grad_lut, vec2(t,0.5));\n"
    "  }\n"
    "  if(base.a<0.004) discard;\n"
    "  gl_FragColor=base;\n"
    "}\n";
#else
static const char * vs_vec =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_pos;\n"
    "void main(){\n"
    "  v_pos=a_pos;\n"
    "  vec2 ndc;\n"
    "  ndc.x=a_pos.x/u_disp.x*2.0-1.0;\n"
    "  ndc.y=a_pos.y/u_disp.y*2.0-1.0;\n"
    "  gl_Position=vec4(ndc,0.0,1.0);\n"
    "}\n";

static const char * fs_vec =
    "#version 120\n"
    "uniform vec4 u_color;\n"
    "uniform int u_grad_on;\n"
    "uniform int u_grad_style;\n"
    "uniform vec4 u_color1;\n"
    "uniform vec2 u_grad_p0;\n"
    "uniform vec2 u_grad_p1;\n"
    "uniform float u_grad_cx;\n"
    "uniform float u_grad_cy;\n"
    "uniform float u_grad_r0;\n"
    "uniform float u_grad_r1;\n"
    "uniform sampler2D u_grad_lut;\n"
    "varying vec2 v_pos;\n"
    "void main(){\n"
    "  vec4 base=u_color;\n"
    "  if(u_grad_on==1){\n"
    "    float t=0.0;\n"
    "    if(u_grad_style==1){\n"
    "      vec2 ab=u_grad_p1-u_grad_p0;\n"
    "      t=dot(v_pos-u_grad_p0,ab)/max(dot(ab,ab),1.0);\n"
    "    } else {\n"
    "      float d=length(v_pos-vec2(u_grad_cx,u_grad_cy));\n"
    "      t=(d-u_grad_r0)/max(u_grad_r1-u_grad_r0,0.001);\n"
    "    }\n"
    "    t=clamp(t,0.0,1.0);\n"
    "    base=texture2D(u_grad_lut, vec2(t,0.5));\n"
    "  }\n"
    "  if(base.a<0.004) discard;\n"
    "  gl_FragColor=base;\n"
    "}\n";
#endif

typedef struct {
    float x;
    float y;
} gpu_vec_pt_t;

#define GPU_VEC_LUT_SIZE 256
typedef struct {
    bool active;
    int style;
    lv_color32_t c0;
    lv_color32_t c1;
    float p0x, p0y, p1x, p1y;
    float cx, cy, r0, r1;
    uint8_t lut[GPU_VEC_LUT_SIZE * 4]; /* B,G,R,A per texel, all stops resolved */
} gpu_vec_grad_t;

static unsigned int g_vec_grad_lut_tex = 0;
static unsigned int g_vec_vbo = 0;
static float g_vec_batch_verts[GPU_VEC_BATCH_VERTS * 2];

static unsigned int compile_shader(unsigned int type, const char * src)
{
    unsigned int s = glCreateShader(type);
    GL_CALL(glShaderSource(s, 1, &src, NULL));
    GL_CALL(glCompileShader(s));
    int ok = 0;
    GL_CALL(glGetShaderiv(s, GL_COMPILE_STATUS, &ok));
    if(!ok) {
        GL_CALL(glDeleteShader(s));
        return 0;
    }
    return s;
}

static unsigned int link_program(unsigned int v, unsigned int f)
{
    unsigned int p = glCreateProgram();
    GL_CALL(glAttachShader(p, v));
    GL_CALL(glAttachShader(p, f));
    GL_CALL(glBindAttribLocation(p, 0, "a_pos"));
    GL_CALL(glLinkProgram(p));
    GL_CALL(glDeleteShader(v));
    GL_CALL(glDeleteShader(f));
    int ok = 0;
    GL_CALL(glGetProgramiv(p, GL_LINK_STATUS, &ok));
    if(!ok) {
        GL_CALL(glDeleteProgram(p));
        return 0;
    }
    return p;
}

void lv_gpu_renderer_gles2_vector_init(void)
{
    if(prog_tri) return;
    if(!g_vec_vbo) GL_CALL(glGenBuffers(1, &g_vec_vbo));
    unsigned int v = compile_shader(GL_VERTEX_SHADER, vs_vec);
    unsigned int f = compile_shader(GL_FRAGMENT_SHADER, fs_vec);
    prog_tri = link_program(v, f);
    if(!prog_tri) return;
    loc_tri_disp = glGetUniformLocation(prog_tri, "u_disp");
    loc_tri_color = glGetUniformLocation(prog_tri, "u_color");
    loc_tri_grad_on = glGetUniformLocation(prog_tri, "u_grad_on");
    loc_tri_grad_style = glGetUniformLocation(prog_tri, "u_grad_style");
    loc_tri_color1 = glGetUniformLocation(prog_tri, "u_color1");
    loc_tri_grad_p0 = glGetUniformLocation(prog_tri, "u_grad_p0");
    loc_tri_grad_p1 = glGetUniformLocation(prog_tri, "u_grad_p1");
    loc_tri_grad_cx = glGetUniformLocation(prog_tri, "u_grad_cx");
    loc_tri_grad_cy = glGetUniformLocation(prog_tri, "u_grad_cy");
    loc_tri_grad_r0 = glGetUniformLocation(prog_tri, "u_grad_r0");
    loc_tri_grad_r1 = glGetUniformLocation(prog_tri, "u_grad_r1");
    loc_tri_grad_lut = glGetUniformLocation(prog_tri, "u_grad_lut");
}

void lv_gpu_renderer_gles2_vector_deinit(void)
{
    if(prog_tri) GL_CALL(glDeleteProgram(prog_tri));
    prog_tri = 0;
    if(g_vec_grad_lut_tex) {
        GL_CALL(glDeleteTextures(1, &g_vec_grad_lut_tex));
        g_vec_grad_lut_tex = 0;
    }
    if(g_vec_vbo) {
        GL_CALL(glDeleteBuffers(1, &g_vec_vbo));
        g_vec_vbo = 0;
    }
}

static void xform_pt(const lv_matrix_t * m, float x, float y, float * ox, float * oy)
{
    *ox = m->m[0][0] * x + m->m[0][1] * y + m->m[0][2];
    *oy = m->m[1][0] * x + m->m[1][1] * y + m->m[1][2];
}

static bool pt_push(gpu_vec_pt_t * pts, uint32_t * n, float x, float y)
{
    if(*n >= GPU_VEC_MAX_PTS) return false;
    pts[*n].x = x;
    pts[*n].y = y;
    (*n)++;
    return true;
}

static void flatten_quad(gpu_vec_pt_t * out, uint32_t * n,
                         float x0, float y0, float x1, float y1, float x2, float y2, int depth)
{
    const float dx = x2 - x0;
    const float dy = y2 - y0;
    const float d = fabsf((x1 - x2) * dy - (y1 - y2) * dx);
    const float len = sqrtf(dx * dx + dy * dy);
    if(depth > 8 || d < GPU_VEC_FLAT_TOL * len) {
        pt_push(out, n, x2, y2);
        return;
    }
    const float mx = (x0 + 2.f * x1) * 0.25f + x2 * 0.25f;
    const float my = (y0 + 2.f * y1) * 0.25f + y2 * 0.25f;
    const float qx = (x0 + x1) * 0.5f;
    const float qy = (y0 + y1) * 0.5f;
    flatten_quad(out, n, x0, y0, qx, qy, mx, my, depth + 1);
    flatten_quad(out, n, mx, my, (x1 + x2) * 0.5f, (y1 + y2) * 0.5f, x2, y2, depth + 1);
}

static void flatten_cubic(gpu_vec_pt_t * out, uint32_t * n,
                           float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, int depth)
{
    const float dx = x3 - x0;
    const float dy = y3 - y0;
    float d = fabsf((x1 - x3) * dy - (y1 - y3) * dx);
    d = LV_MAX(d, fabsf((x2 - x3) * dy - (y2 - y3) * dx));
    const float len = sqrtf(dx * dx + dy * dy);
    if(depth > 8 || d < GPU_VEC_FLAT_TOL * len) {
        pt_push(out, n, x3, y3);
        return;
    }
    const float x01 = (x0 + x1) * 0.5f, y01 = (y0 + y1) * 0.5f;
    const float x12 = (x1 + x2) * 0.5f, y12 = (y1 + y2) * 0.5f;
    const float x23 = (x2 + x3) * 0.5f, y23 = (y2 + y3) * 0.5f;
    const float x012 = (x01 + x12) * 0.5f, y012 = (y01 + y12) * 0.5f;
    const float x123 = (x12 + x23) * 0.5f, y123 = (y12 + y23) * 0.5f;
    const float mx = (x012 + x123) * 0.5f, my = (y012 + y123) * 0.5f;
    flatten_cubic(out, n, x0, y0, x01, y01, x012, y012, mx, my, depth + 1);
    flatten_cubic(out, n, mx, my, x123, y123, x23, y23, x3, y3, depth + 1);
}

static bool flatten_contour(const lv_vector_path_t * path, uint32_t op_start, uint32_t op_end,
                             const lv_matrix_t * matrix, gpu_vec_pt_t * out, uint32_t * n)
{
    const lv_vector_path_op_t * ops = lv_array_front(&path->ops);
    uint32_t pidx = 0;
    for(uint32_t k = 0; k < op_start; k++) {
        switch(ops[k]) {
            case LV_VECTOR_PATH_OP_MOVE_TO: pidx += 1; break;
            case LV_VECTOR_PATH_OP_LINE_TO: pidx += 1; break;
            case LV_VECTOR_PATH_OP_QUAD_TO: pidx += 2; break;
            case LV_VECTOR_PATH_OP_CUBIC_TO: pidx += 3; break;
            default: break;
        }
    }

    float cx = 0.f, cy = 0.f, sx = 0.f, sy = 0.f;
    bool has_start = false;

    for(uint32_t i = op_start; i < op_end; i++) {
        switch(ops[i]) {
            case LV_VECTOR_PATH_OP_MOVE_TO: {
                    const lv_fpoint_t * pt = lv_array_at(&path->points, pidx++);
                    float tx, ty;
                    xform_pt(matrix, pt->x, pt->y, &tx, &ty);
                    cx = sx = tx;
                    cy = sy = ty;
                    has_start = true;
                    if(!pt_push(out, n, cx, cy)) return false;
                }
                break;
            case LV_VECTOR_PATH_OP_LINE_TO: {
                    const lv_fpoint_t * pt = lv_array_at(&path->points, pidx++);
                    xform_pt(matrix, pt->x, pt->y, &cx, &cy);
                    if(!pt_push(out, n, cx, cy)) return false;
                }
                break;
            case LV_VECTOR_PATH_OP_QUAD_TO: {
                    const lv_fpoint_t * p1 = lv_array_at(&path->points, pidx);
                    const lv_fpoint_t * p2 = lv_array_at(&path->points, pidx + 1);
                    float x1, y1, x2, y2;
                    xform_pt(matrix, p1->x, p1->y, &x1, &y1);
                    xform_pt(matrix, p2->x, p2->y, &x2, &y2);
                    flatten_quad(out, n, cx, cy, x1, y1, x2, y2, 0);
                    cx = x2;
                    cy = y2;
                    pidx += 2;
                }
                break;
            case LV_VECTOR_PATH_OP_CUBIC_TO: {
                    const lv_fpoint_t * p1 = lv_array_at(&path->points, pidx);
                    const lv_fpoint_t * p2 = lv_array_at(&path->points, pidx + 1);
                    const lv_fpoint_t * p3 = lv_array_at(&path->points, pidx + 2);
                    float x1, y1, x2, y2, x3, y3;
                    xform_pt(matrix, p1->x, p1->y, &x1, &y1);
                    xform_pt(matrix, p2->x, p2->y, &x2, &y2);
                    xform_pt(matrix, p3->x, p3->y, &x3, &y3);
                    flatten_cubic(out, n, cx, cy, x1, y1, x2, y2, x3, y3, 0);
                    cx = x3;
                    cy = y3;
                    pidx += 3;
                }
                break;
            case LV_VECTOR_PATH_OP_CLOSE:
                if(has_start && (cx != sx || cy != sy)) {
                    if(!pt_push(out, n, sx, sy)) return false;
                    cx = sx;
                    cy = sy;
                }
                break;
            default:
                break;
        }
    }
    LV_UNUSED(has_start);
    return *n >= 3;
}

static float tri_area2(float ax, float ay, float bx, float by, float cx, float cy)
{
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static bool earcut(const gpu_vec_pt_t * poly, uint32_t n, uint16_t * idx, uint32_t * out_n)
{
    if(n < 3) return false;
    uint8_t * ear = lv_malloc(n);
    uint16_t * order = lv_malloc(sizeof(uint16_t) * n);
    if(!ear || !order) {
        lv_free(ear);
        lv_free(order);
        return false;
    }
    for(uint32_t i = 0; i < n; i++) {
        ear[i] = 1;
        order[i] = (uint16_t)i;
    }

    uint32_t remaining = n;
    uint32_t out = 0;
    uint32_t guard = 0;
    while(remaining > 3 && guard++ < n * n) {
        bool clipped = false;
        for(uint32_t i = 0; i < remaining; i++) {
            if(!ear[i]) continue;
            const uint16_t i0 = order[(i + remaining - 1) % remaining];
            const uint16_t i1 = order[i];
            const uint16_t i2 = order[(i + 1) % remaining];
            const gpu_vec_pt_t * a = &poly[i0];
            const gpu_vec_pt_t * b = &poly[i1];
            const gpu_vec_pt_t * c = &poly[i2];
            const float area = tri_area2(a->x, a->y, b->x, b->y, c->x, c->y);
            if(area <= 0.f) continue;
            bool hit = false;
            for(uint32_t j = 0; j < remaining; j++) {
                if(j == i || j == (i + remaining - 1) % remaining || j == (i + 1) % remaining) continue;
                const gpu_vec_pt_t * p = &poly[order[j]];
                const float w0 = tri_area2(b->x, b->y, c->x, c->y, p->x, p->y);
                const float w1 = tri_area2(c->x, c->y, a->x, a->y, p->x, p->y);
                const float w2 = tri_area2(a->x, a->y, b->x, b->y, p->x, p->y);
                if(w0 >= 0.f && w1 >= 0.f && w2 >= 0.f) {
                    hit = true;
                    break;
                }
            }
            if(hit) continue;
            if(out + 3 > GPU_VEC_MAX_TRIS * 3) break;
            idx[out++] = i0;
            idx[out++] = i1;
            idx[out++] = i2;
            ear[i] = 0;
            for(uint32_t k = i; k + 1 < remaining; k++) order[k] = order[k + 1];
            remaining--;
            clipped = true;
            break;
        }
        if(!clipped) break;
    }
    if(remaining == 3 && out + 3 <= GPU_VEC_MAX_TRIS * 3) {
        idx[out++] = order[0];
        idx[out++] = order[1];
        idx[out++] = order[2];
    }
    *out_n = out;
    lv_free(ear);
    lv_free(order);
    return out >= 3;
}

static uint32_t triangulate_fan(const gpu_vec_pt_t * poly, uint32_t n, uint16_t * idx)
{
    if(n < 3) return 0;
    uint32_t out = 0;
    for(uint32_t i = 1; i + 1 < n && out + 3 <= GPU_VEC_MAX_TRIS * 3; i++) {
        idx[out++] = 0;
        idx[out++] = (uint16_t)i;
        idx[out++] = (uint16_t)(i + 1);
    }
    return out;
}

static void poly_bounds(const gpu_vec_pt_t * poly, uint32_t n, float * x0, float * y0, float * x1, float * y1)
{
    *x0 = *y0 = 1e9f;
    *x1 = *y1 = -1e9f;
    for(uint32_t i = 0; i < n; i++) {
        if(poly[i].x < *x0) *x0 = poly[i].x;
        if(poly[i].y < *y0) *y0 = poly[i].y;
        if(poly[i].x > *x1) *x1 = poly[i].x;
        if(poly[i].y > *y1) *y1 = poly[i].y;
    }
}

static void bind_paint_uniforms(lv_color32_t color, const gpu_vec_grad_t * grad, int32_t dw, int32_t dh)
{
    GL_CALL(glUseProgram(prog_tri));
    GL_CALL(glUniform2f(loc_tri_disp, (float)dw, (float)dh));
    GL_CALL(glUniform4f(loc_tri_color, color.blue / 255.f, color.green / 255.f, color.red / 255.f,
                        color.alpha / 255.f));
    if(grad && grad->active) {
        GL_CALL(glUniform1i(loc_tri_grad_on, 1));
        GL_CALL(glUniform1i(loc_tri_grad_style, grad->style));
        const lv_color32_t c1 = grad->c1;
        GL_CALL(glUniform4f(loc_tri_color1, c1.blue / 255.f, c1.green / 255.f, c1.red / 255.f, c1.alpha / 255.f));
        GL_CALL(glUniform2f(loc_tri_grad_p0, grad->p0x, grad->p0y));
        GL_CALL(glUniform2f(loc_tri_grad_p1, grad->p1x, grad->p1y));
        GL_CALL(glUniform1f(loc_tri_grad_cx, grad->cx));
        GL_CALL(glUniform1f(loc_tri_grad_cy, grad->cy));
        GL_CALL(glUniform1f(loc_tri_grad_r0, grad->r0));
        GL_CALL(glUniform1f(loc_tri_grad_r1, grad->r1));

        GL_CALL(glActiveTexture(GL_TEXTURE0));
        if(!g_vec_grad_lut_tex) {
            GL_CALL(glGenTextures(1, &g_vec_grad_lut_tex));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, g_vec_grad_lut_tex));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        }
        else {
            GL_CALL(glBindTexture(GL_TEXTURE_2D, g_vec_grad_lut_tex));
        }
        GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GPU_VEC_LUT_SIZE, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, grad->lut));
        GL_CALL(glUniform1i(loc_tri_grad_lut, 0));
    }
    else {
        GL_CALL(glUniform1i(loc_tri_grad_on, 0));
    }
}

static void vec_grad_lut_build(const lv_vector_gradient_t * g, lv_opa_t opa, uint8_t * out)
{
    const int n = (int)g->stops_count;
    for(int i = 0; i < GPU_VEC_LUT_SIZE; i++) {
        lv_color_t col;
        lv_opa_t sopa;
        if(i <= g->stops[0].frac) {
            col = g->stops[0].color;
            sopa = g->stops[0].opa;
        }
        else if(i >= g->stops[n - 1].frac) {
            col = g->stops[n - 1].color;
            sopa = g->stops[n - 1].opa;
        }
        else {
            int k = 0;
            while(k < n - 1 && i > g->stops[k + 1].frac) k++;
            const lv_grad_stop_t * a = &g->stops[k];
            const lv_grad_stop_t * b = &g->stops[k + 1];
            int span = (int)b->frac - (int)a->frac;
            int f = span > 0 ? ((i - (int)a->frac) * 255) / span : 0;
            col = lv_color_mix(b->color, a->color, (uint8_t)f);
            sopa = (lv_opa_t)((int)a->opa + ((int)b->opa - (int)a->opa) * f / 255);
        }
        lv_color32_t c = lv_color_to_32(col, LV_OPA_MIX2(opa, sopa));
        out[i * 4 + 0] = c.blue;
        out[i * 4 + 1] = c.green;
        out[i * 4 + 2] = c.red;
        out[i * 4 + 3] = c.alpha;
    }
}

static void grad_from_vector(const lv_vector_gradient_t * g, lv_opa_t opa, gpu_vec_grad_t * out)
{
    lv_memzero(out, sizeof(*out));
    if(!g || g->stops_count < 2) return;
    out->active = true;
    out->style = (g->style == LV_VECTOR_GRADIENT_STYLE_RADIAL) ? 2 : 1;
    out->c0 = lv_color_to_32(g->stops[0].color, LV_OPA_MIX2(opa, g->stops[0].opa));
    const uint32_t last = g->stops_count - 1;
    out->c1 = lv_color_to_32(g->stops[last].color, LV_OPA_MIX2(opa, g->stops[last].opa));
    vec_grad_lut_build(g, opa, out->lut);
    out->p0x = g->x1;
    out->p0y = g->y1;
    out->p1x = g->x2;
    out->p1y = g->y2;
    out->cx = g->cx;
    out->cy = g->cy;
    out->r0 = 0.f;
    out->r1 = g->cr;
}

static void grad_from_solid(lv_color32_t color, gpu_vec_grad_t * out)
{
    lv_memzero(out, sizeof(*out));
    LV_UNUSED(color);
}

static void vec_vbo_draw(const float * verts, uint32_t vert_count)
{
    if(vert_count < 3 || !g_vec_vbo) return;
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, g_vec_vbo));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, vert_count * 2 * sizeof(float), verts, GL_STREAM_DRAW));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vert_count));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

static void draw_tris_raw(const gpu_vec_pt_t * poly, const uint16_t * idx, uint32_t idx_n)
{
    if(idx_n < 3) return;
    if(idx_n > GPU_VEC_BATCH_VERTS) idx_n = GPU_VEC_BATCH_VERTS;
    for(uint32_t i = 0; i < idx_n; i++) {
        g_vec_batch_verts[i * 2]     = poly[idx[i]].x;
        g_vec_batch_verts[i * 2 + 1] = poly[idx[i]].y;
    }
    vec_vbo_draw(g_vec_batch_verts, idx_n);
}

static void draw_tris(const gpu_vec_pt_t * poly, const uint16_t * idx, uint32_t idx_n,
                      lv_color32_t color, const gpu_vec_grad_t * grad, int32_t dw, int32_t dh)
{
    if(!prog_tri || idx_n < 3) return;
    bind_paint_uniforms(color, grad, dw, dh);
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    draw_tris_raw(poly, idx, idx_n);
}

static void draw_tris_stencil(const gpu_vec_pt_t * poly, const uint16_t * idx, uint32_t idx_n, bool invert)
{
    GL_CALL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
    GL_CALL(glEnable(GL_STENCIL_TEST));
    GL_CALL(glStencilMask(0xFF));
    GL_CALL(glStencilFunc(GL_ALWAYS, 0, 0xFF));
    GL_CALL(glStencilOp(GL_KEEP, GL_KEEP, invert ? GL_INVERT : GL_INCR_WRAP));
    draw_tris_raw(poly, idx, idx_n);
    GL_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
}

static void draw_fill_bbox(float x0, float y0, float x1, float y1, lv_color32_t color,
                            const gpu_vec_grad_t * grad, int32_t dw, int32_t dh)
{
    const float v[12] = { x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1 };
    bind_paint_uniforms(color, grad, dw, dh);
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL_CALL(glEnable(GL_STENCIL_TEST));
    GL_CALL(glStencilFunc(GL_NOTEQUAL, 0, 0xFF));
    GL_CALL(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glDisable(GL_STENCIL_TEST));
}

static bool dash_visible(float dist, float seg_len, const float * dash, uint32_t dash_n, float * adv)
{
    if(!dash || dash_n == 0) {
        *adv = seg_len;
        return true;
    }
    float pos = dist;
    float rem = seg_len;
    while(rem > 0.001f) {
        for(uint32_t i = 0; i < dash_n; i++) {
            const float seg = dash[i];
            if(seg < 0.001f) continue;
            const float use = LV_MIN(seg, rem);
            const bool draw = (i & 1u) == 0u;
            if(draw && pos >= dist && pos + use > dist) {
                *adv = use;
                return true;
            }
            if(!draw && pos + use > dist) {
                *adv = use;
                return false;
            }
            pos += use;
            rem -= use;
            if(pos > dist + seg_len) break;
        }
    }
    *adv = seg_len;
    return false;
}

static void draw_stroke_poly(const gpu_vec_pt_t * poly, uint32_t n, float width,
                              lv_color32_t color, const gpu_vec_grad_t * grad,
                              const float * dash, uint32_t dash_n,
                              int32_t dw, int32_t dh)
{
    if(n < 2 || width < 0.5f) return;
    const float hw = width * 0.5f;
    float dist = 0.f;
    uint32_t nv = 0;
    bind_paint_uniforms(color, grad, dw, dh);
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    for(uint32_t i = 0; i + 1 < n; i++) {
        const float x0 = poly[i].x, y0 = poly[i].y;
        const float x1 = poly[i + 1].x, y1 = poly[i + 1].y;
        const float dx = x1 - x0, dy = y1 - y0;
        const float len = sqrtf(dx * dx + dy * dy);
        if(len < 0.001f) continue;

        float seg_off = 0.f;
        while(seg_off < len) {
            float adv = 0.f;
            const bool vis = dash_visible(dist + seg_off, len - seg_off, dash, dash_n, &adv);
            if(adv < 0.001f) break;
            if(vis) {
                const float t0 = seg_off / len;
                const float t1 = (seg_off + adv) / len;
                const float sx0 = x0 + dx * t0, sy0 = y0 + dy * t0;
                const float sx1 = x0 + dx * t1, sy1 = y0 + dy * t1;
                const float sdx = sx1 - sx0, sdy = sy1 - sy0;
                const float slen = sqrtf(sdx * sdx + sdy * sdy);
                if(slen > 0.001f && nv + 12 <= GPU_VEC_BATCH_VERTS * 2) {
                    const float nx = -sdy / slen * hw, ny = sdx / slen * hw;
                    const float q[12] = {
                        sx0 + nx, sy0 + ny, sx1 + nx, sy1 + ny, sx1 - nx, sy1 - ny,
                        sx0 + nx, sy0 + ny, sx1 - nx, sy1 - ny, sx0 - nx, sy0 - ny,
                    };
                    for(int k = 0; k < 12; k++) g_vec_batch_verts[nv++] = q[k];
                }
            }
            seg_off += adv;
        }
        dist += len;
    }

    if(nv >= 6) vec_vbo_draw(g_vec_batch_verts, nv / 2);
}

static bool draw_path_contour(int32_t dw, int32_t dh, const lv_vector_path_t * path,
                               uint32_t op_start, uint32_t op_end, const lv_matrix_t * matrix,
                               const lv_vector_fill_dsc_t * fill_dsc,
                               const lv_vector_stroke_dsc_t * stroke_dsc,
                               bool even_odd, const lv_area_t * clip)
{
    gpu_vec_pt_t poly[GPU_VEC_MAX_PTS];
    uint32_t pn = 0;
    if(!flatten_contour(path, op_start, op_end, matrix, poly, &pn)) return false;

    uint16_t idx[GPU_VEC_MAX_TRIS * 3];
    uint32_t idx_n = 0;
    if(!earcut(poly, pn, idx, &idx_n)) {
        idx_n = triangulate_fan(poly, pn, idx);
    }
    if(idx_n < 3) return false;

    gpu_vec_grad_t fill_grad, stroke_grad;
  lv_color32_t fc = fill_dsc->color;
    fc.alpha = LV_OPA_MIX2(fc.alpha, fill_dsc->opa);
    gpu_vec_grad_t * fgrad = NULL;
    if(fill_dsc->style == LV_VECTOR_DRAW_STYLE_GRADIENT) {
        grad_from_vector(&fill_dsc->gradient, fill_dsc->opa, &fill_grad);
        fc = fill_grad.c0;
        fgrad = &fill_grad;
    }
    else {
        grad_from_solid(fc, &fill_grad);
    }

    lv_color32_t sc = stroke_dsc->color;
    sc.alpha = LV_OPA_MIX2(sc.alpha, stroke_dsc->opa);
    gpu_vec_grad_t * sgrad = NULL;
    if(stroke_dsc->style == LV_VECTOR_DRAW_STYLE_GRADIENT) {
        grad_from_vector(&stroke_dsc->gradient, stroke_dsc->opa, &stroke_grad);
        sc = stroke_grad.c0;
        sgrad = &stroke_grad;
    }
    else {
        grad_from_solid(sc, &stroke_grad);
    }

    const float * dash = NULL;
    uint32_t dash_n = 0;
    if(!lv_array_is_empty(&stroke_dsc->dash_pattern)) {
        dash = lv_array_front(&stroke_dsc->dash_pattern);
        dash_n = stroke_dsc->dash_pattern.size;
    }

    if(stroke_dsc->width > 0.5f && sc.alpha > LV_OPA_MIN) {
        draw_stroke_poly(poly, pn, stroke_dsc->width, sc, sgrad, dash, dash_n, dw, dh);
    }

    if(fill_dsc->opa > LV_OPA_MIN && (fc.alpha > LV_OPA_MIN || (fgrad && fgrad->active))) {
        const bool use_stencil = lv_gpu_renderer_tex_fbo_has_stencil();
        if(use_stencil) {
            GL_CALL(glClearStencil(0));
            GL_CALL(glClear(GL_STENCIL_BUFFER_BIT));
            draw_tris_stencil(poly, idx, idx_n, even_odd);
            float bx0, by0, bx1, by1;
            poly_bounds(poly, pn, &bx0, &by0, &bx1, &by1);
            draw_fill_bbox(bx0, by0, bx1, by1, fc, fgrad, dw, dh);
        }
        else {
            draw_tris(poly, idx, idx_n, fc, fgrad, dw, dh);
        }
    }

    LV_UNUSED(clip);
    return true;
}

bool lv_gpu_renderer_gles2_vector_draw_path(int32_t dw, int32_t dh,
                                            const lv_vector_path_t * path,
                                            const lv_matrix_t * matrix,
                                            lv_color32_t fill_color, lv_opa_t fill_opa,
                                            lv_color32_t stroke_color, lv_opa_t stroke_opa,
                                            float stroke_width,
                                            const lv_area_t * clip)
{
    lv_vector_fill_dsc_t fill_dsc;
    lv_memzero(&fill_dsc, sizeof(fill_dsc));
    fill_dsc.style = LV_VECTOR_DRAW_STYLE_SOLID;
    fill_dsc.color = fill_color;
    fill_dsc.opa = fill_opa;
    fill_dsc.fill_rule = LV_VECTOR_FILL_NONZERO;

    lv_vector_stroke_dsc_t stroke_dsc;
    lv_memzero(&stroke_dsc, sizeof(stroke_dsc));
    stroke_dsc.style = LV_VECTOR_DRAW_STYLE_SOLID;
    stroke_dsc.color = stroke_color;
    stroke_dsc.opa = stroke_opa;
    stroke_dsc.width = stroke_width;

    if(!path || dw < 1 || dh < 1) return false;
    lv_gpu_renderer_gles2_vector_init();
    if(!prog_tri) return false;

    if(clip) {
        GL_CALL(glEnable(GL_SCISSOR_TEST));
        GL_CALL(glScissor(clip->x1, dh - clip->y2 - 1, lv_area_get_width(clip), lv_area_get_height(clip)));
    }

    const lv_vector_path_op_t * ops = lv_array_front(&path->ops);
    const uint32_t op_n = lv_array_size(&path->ops);
    uint32_t start = 0;
    bool any = false;
    bool contour_parity = false;

    for(uint32_t i = 0; i <= op_n; i++) {
        const bool boundary = (i == op_n) || (ops[i] == LV_VECTOR_PATH_OP_MOVE_TO && i > start);
        if(!boundary) continue;
        if(i > start && draw_path_contour(dw, dh, path, start, i, matrix, &fill_dsc, &stroke_dsc,
                                          contour_parity, clip)) {
            any = true;
        }
        contour_parity = !contour_parity;
        start = i;
    }

    if(clip) GL_CALL(glDisable(GL_SCISSOR_TEST));
    return any;
}

bool lv_gpu_renderer_gles2_vector_draw_dsc(int32_t dw, int32_t dh,
                                           const lv_draw_vector_dsc_t * dsc,
                                           const lv_area_t * clip)
{
    if(!dsc || !dsc->task_list) return false;
    lv_gpu_renderer_gles2_vector_init();

    lv_draw_vector_subtask_t * task = lv_ll_get_head(dsc->task_list);
    bool any = false;
    while(task) {
        const lv_vector_path_ctx_t * ctx = &task->ctx;
        lv_matrix_t m = ctx->matrix;
        if(task->path) {
            const bool even_odd = ctx->fill_dsc.fill_rule == LV_VECTOR_FILL_EVENODD;
            const lv_vector_path_op_t * ops = lv_array_front(&task->path->ops);
            const uint32_t op_n = lv_array_size(&task->path->ops);
            uint32_t start = 0;
            bool contour_parity = false;
            for(uint32_t i = 0; i <= op_n; i++) {
                const bool boundary = (i == op_n) || (ops[i] == LV_VECTOR_PATH_OP_MOVE_TO && i > start);
                if(!boundary) continue;
                if(i > start && draw_path_contour(dw, dh, task->path, start, i, &m,
                                                  &ctx->fill_dsc, &ctx->stroke_dsc,
                                                  even_odd && contour_parity, clip)) {
                    any = true;
                }
                contour_parity = !contour_parity;
                start = i;
            }
        }
        task = lv_ll_get_next(dsc->task_list, task);
    }
    return any;
}

#endif /*LV_USE_DRAW_GPU_RENDERER && LV_USE_VECTOR_GRAPHIC*/
