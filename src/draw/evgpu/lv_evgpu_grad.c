/**
 * @file lv_evgpu_grad.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_evgpu_grad.h"

#if LV_USE_DRAW_EVGPU

#include "lv_draw_evgpu_private.h"
#include "lv_evgpu_context.h"
#include "lv_evgpu_shader.h"
#include "lv_evgpu_utils.h"

#if LV_USE_OPENGLES && LV_USE_EGL
    #include "../../drivers/opengles/lv_opengles_private.h"
    #define LV_EVGPU_GRAD_HAS_GL 1
#else
    #define LV_EVGPU_GRAD_HAS_GL 0
#endif

/*********************
 *      DEFINES
 *********************/

#define LV_EVGPU_GRAD_MAX_SHADER_STOPS 8

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    struct _lv_draw_evgpu_unit_t * unit;
    uint32_t vbo;
    bool ready;
#if LV_EVGPU_GRAD_HAS_GL
    int32_t loc_matrix;
    int32_t loc_view_size;
    int32_t loc_dir;
    int32_t loc_extend;
    int32_t loc_stops_count;
    int32_t loc_stop_color;
    int32_t loc_stop_frac;
    int32_t loc_rect;
    int32_t loc_radius;
    int32_t loc_linear;
    int32_t loc_radial0;
    int32_t loc_radial1;
    int32_t loc_conical_center;
    int32_t loc_conical_angles;
#endif
} lv_evgpu_grad_state_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_evgpu_grad_state_t g_grad_state;

/**********************
 *  STATIC PROTOTYPES
 **********************/

#if LV_EVGPU_GRAD_HAS_GL
static void pack_stops(const lv_grad_dsc_t * grad_dsc, float * colors, float * fracs, int32_t * count);
static void pack_geometry(const lv_grad_dsc_t * grad_dsc, const lv_area_t * coords, float radius,
                          int32_t * dir, int32_t * extend, float * linear, float * radial0, float * radial1,
                          float * conical_center, float * conical_angles, float * rect);
static void set_scissor(const lv_area_t * clip, int32_t viewport_h);
#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_evgpu_grad_init(lv_draw_evgpu_unit_t * unit)
{
    lv_memzero(&g_grad_state, sizeof(g_grad_state));
    g_grad_state.unit = unit;

#if LV_EVGPU_GRAD_HAS_GL
    if(!lv_evgpu_shader_grad_is_ready(&unit->shader)) return;

    static const float quad[] = {
        0.f, 0.f,
        1.f, 0.f,
        0.f, 1.f,
        1.f, 0.f,
        1.f, 1.f,
        0.f, 1.f,
    };

    glGenBuffers(1, &g_grad_state.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_grad_state.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const uint32_t prog = unit->shader.grad_program;
    g_grad_state.loc_matrix = glGetUniformLocation(prog, "u_matrix");
    g_grad_state.loc_view_size = glGetUniformLocation(prog, "u_view_size");
    g_grad_state.loc_dir = glGetUniformLocation(prog, "u_dir");
    g_grad_state.loc_extend = glGetUniformLocation(prog, "u_extend");
    g_grad_state.loc_stops_count = glGetUniformLocation(prog, "u_stops_count");
    g_grad_state.loc_stop_color = glGetUniformLocation(prog, "u_stop_color");
    g_grad_state.loc_stop_frac = glGetUniformLocation(prog, "u_stop_frac");
    g_grad_state.loc_rect = glGetUniformLocation(prog, "u_rect");
    g_grad_state.loc_radius = glGetUniformLocation(prog, "u_radius");
    g_grad_state.loc_linear = glGetUniformLocation(prog, "u_linear");
    g_grad_state.loc_radial0 = glGetUniformLocation(prog, "u_radial0");
    g_grad_state.loc_radial1 = glGetUniformLocation(prog, "u_radial1");
    g_grad_state.loc_conical_center = glGetUniformLocation(prog, "u_conical_center");
    g_grad_state.loc_conical_angles = glGetUniformLocation(prog, "u_conical_angles");

    g_grad_state.ready = true;
    LV_LOG_INFO("EVGPU native gradient ready (program=%u)", (unsigned)prog);
#else
    LV_UNUSED(unit);
#endif
}

void lv_evgpu_grad_deinit(lv_draw_evgpu_unit_t * unit)
{
    LV_UNUSED(unit);
#if LV_EVGPU_GRAD_HAS_GL
    if(g_grad_state.vbo) {
        glDeleteBuffers(1, &g_grad_state.vbo);
    }
#endif
    lv_memzero(&g_grad_state, sizeof(g_grad_state));
}

bool lv_evgpu_grad_fill_rect(lv_draw_evgpu_unit_t * unit,
                            const lv_area_t * coords,
                            const lv_grad_dsc_t * grad_dsc,
                            float radius,
                            const lv_area_t * clip_area,
                            const lv_matrix_t * matrix)
{
    LV_ASSERT_NULL(unit);
    LV_ASSERT_NULL(coords);
    LV_ASSERT_NULL(grad_dsc);

    if(grad_dsc->dir == LV_GRAD_DIR_NONE) return false;
#if LV_EVGPU_GRAD_HAS_GL
    if(!g_grad_state.ready || !lv_evgpu_shader_grad_is_ready(&unit->shader)) return false;

    lv_evgpu_evgr_flush_pending(unit);

    float colors[LV_EVGPU_GRAD_MAX_SHADER_STOPS * 4];
    float fracs[LV_EVGPU_GRAD_MAX_SHADER_STOPS];
    int32_t stops_count = 0;
    pack_stops(grad_dsc, colors, fracs, &stops_count);
    if(stops_count < 2) return false;

    int32_t dir = 0;
    int32_t extend = (int32_t)grad_dsc->extend;
    float linear[4];
    float radial0[4];
    float radial1[4];
    float conical_center[2];
    float conical_angles[2];
    float rect[4];
    pack_geometry(grad_dsc, coords, radius, &dir, &extend, linear, radial0, radial1,
                  conical_center, conical_angles, rect);

    float grad_xform[6];
    lv_evgpu_matrix_convert(grad_xform, matrix);
    float grad_mat3[9];
    lv_evgpu_xform_to_mat3(grad_mat3, grad_xform);

    const int32_t vp_w = unit->ctx.viewport_w > 0 ? unit->ctx.viewport_w :
                         lv_area_get_width(&unit->current_layer->buf_area);
    const int32_t vp_h = unit->ctx.viewport_h > 0 ? unit->ctx.viewport_h :
                         lv_area_get_height(&unit->current_layer->buf_area);
    const float view_size[2] = { (float)vp_w, (float)vp_h };

    lv_evgpu_native_gl_prepare(unit, vp_w, vp_h);
    set_scissor(clip_area, vp_h);

    lv_evgpu_shader_bind_grad(&unit->ctx, &unit->shader);

    glUniformMatrix3fv(g_grad_state.loc_matrix, 1, GL_FALSE, grad_mat3);
    glUniform2fv(g_grad_state.loc_view_size, 1, view_size);
    glUniform1i(g_grad_state.loc_dir, dir);
    glUniform1i(g_grad_state.loc_extend, extend);
    glUniform1i(g_grad_state.loc_stops_count, stops_count);
    glUniform4fv(g_grad_state.loc_stop_color, stops_count, colors);
    glUniform1fv(g_grad_state.loc_stop_frac, stops_count, fracs);
    glUniform4fv(g_grad_state.loc_rect, 1, rect);
    glUniform1f(g_grad_state.loc_radius, radius);
    glUniform4fv(g_grad_state.loc_linear, 1, linear);
    glUniform4fv(g_grad_state.loc_radial0, 1, radial0);
    glUniform4fv(g_grad_state.loc_radial1, 1, radial1);
    glUniform2fv(g_grad_state.loc_conical_center, 1, conical_center);
    glUniform2fv(g_grad_state.loc_conical_angles, 1, conical_angles);

    glBindBuffer(GL_ARRAY_BUFFER, g_grad_state.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * (GLsizei)sizeof(float), NULL);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    lv_evgpu_shader_unbind(&unit->ctx);
    lv_evgpu_native_gl_finish();
    return true;
#else
    LV_UNUSED(clip_area);
    LV_UNUSED(matrix);
    LV_UNUSED(radius);
    return false;
#endif
}

#if LV_EVGPU_GRAD_HAS_GL

static void pack_stops(const lv_grad_dsc_t * grad_dsc, float * colors, float * fracs, int32_t * count)
{
    const int32_t n = LV_MIN((int32_t)grad_dsc->stops_count, LV_EVGPU_GRAD_MAX_SHADER_STOPS);
    *count = n;
    for(int32_t i = 0; i < n; i++) {
        const lv_grad_stop_t * s = &grad_dsc->stops[i];
        colors[i * 4 + 0] = (float)s->color.red / 255.f;
        colors[i * 4 + 1] = (float)s->color.green / 255.f;
        colors[i * 4 + 2] = (float)s->color.blue / 255.f;
        colors[i * 4 + 3] = (float)s->opa / 255.f;
        fracs[i] = (float)s->frac / 255.f;
    }
}

static void pack_geometry(const lv_grad_dsc_t * grad_dsc, const lv_area_t * coords, float radius,
                          int32_t * dir, int32_t * extend, float * linear, float * radial0, float * radial1,
                          float * conical_center, float * conical_angles, float * rect)
{
    LV_UNUSED(radius);
    const int32_t w = lv_area_get_width(coords);
    const int32_t h = lv_area_get_height(coords);

    linear[0] = linear[1] = linear[2] = linear[3] = 0.f;
    radial0[0] = radial0[1] = radial0[2] = radial0[3] = 0.f;
    radial1[0] = radial1[1] = radial1[2] = radial1[3] = 0.f;
    conical_center[0] = conical_center[1] = 0.f;
    conical_angles[0] = conical_angles[1] = 0.f;

    rect[0] = (float)coords->x1;
    rect[1] = (float)coords->y1;
    rect[2] = (float)w;
    rect[3] = (float)h;

    *dir = (int32_t)grad_dsc->dir;
    *extend = (int32_t)grad_dsc->extend;

    switch(grad_dsc->dir) {
        case LV_GRAD_DIR_VER:
            linear[0] = (float)coords->x1;
            linear[1] = (float)coords->y1;
            linear[2] = (float)coords->x1;
            linear[3] = (float)coords->y2 + 1.f;
            break;
        case LV_GRAD_DIR_HOR:
            linear[0] = (float)coords->x1;
            linear[1] = (float)coords->y1;
            linear[2] = (float)coords->x2 + 1.f;
            linear[3] = (float)coords->y1;
            break;
        case LV_GRAD_DIR_LINEAR: {
                linear[0] = (float)(lv_pct_to_px(grad_dsc->params.linear.start.x, w) + coords->x1);
                linear[1] = (float)(lv_pct_to_px(grad_dsc->params.linear.start.y, h) + coords->y1);
                linear[2] = (float)(lv_pct_to_px(grad_dsc->params.linear.end.x, w) + coords->x1);
                linear[3] = (float)(lv_pct_to_px(grad_dsc->params.linear.end.y, h) + coords->y1);
            }
            break;
        case LV_GRAD_DIR_RADIAL: {
                const float fx = (float)(lv_pct_to_px(grad_dsc->params.radial.focal.x, w) + coords->x1);
                const float fy = (float)(lv_pct_to_px(grad_dsc->params.radial.focal.y, h) + coords->y1);
                const float ex = (float)(lv_pct_to_px(grad_dsc->params.radial.end.x, w) + coords->x1);
                const float ey = (float)(lv_pct_to_px(grad_dsc->params.radial.end.y, h) + coords->y1);
                const float fex = (float)(lv_pct_to_px(grad_dsc->params.radial.focal_extent.x, w) + coords->x1);
                const float fey = (float)(lv_pct_to_px(grad_dsc->params.radial.focal_extent.y, h) + coords->y1);
                const float eex = (float)(lv_pct_to_px(grad_dsc->params.radial.end_extent.x, w) + coords->x1);
                const float eey = (float)(lv_pct_to_px(grad_dsc->params.radial.end_extent.y, h) + coords->y1);
                const float r0 = (float)lv_sqrt32((int32_t)lv_sqr((int32_t)(fex - fx)) + (int32_t)lv_sqr((int32_t)(fey - fy)));
                const float r1 = (float)lv_sqrt32((int32_t)lv_sqr((int32_t)(eex - ex)) + (int32_t)lv_sqr((int32_t)(eey - ey)));
                radial0[0] = fx;
                radial0[1] = fy;
                radial0[2] = LV_MAX(r0, 0.001f);
                radial0[3] = 0.f;
                radial1[0] = ex;
                radial1[1] = ey;
                radial1[2] = LV_MAX(r1, radial0[2] + 0.001f);
                radial1[3] = 0.f;
            }
            break;
        case LV_GRAD_DIR_CONICAL: {
                conical_center[0] = (float)(lv_pct_to_px(grad_dsc->params.conical.center.x, w) + coords->x1);
                conical_center[1] = (float)(lv_pct_to_px(grad_dsc->params.conical.center.y, h) + coords->y1);
                const float a0 = (float)grad_dsc->params.conical.start_angle / 10.f * 3.14159265f / 180.f;
                const float a1 = (float)grad_dsc->params.conical.end_angle / 10.f * 3.14159265f / 180.f;
                conical_angles[0] = a0;
                conical_angles[1] = LV_MAX(a1 - a0, 0.001f);
            }
            break;
        default:
            linear[0] = linear[1] = linear[2] = linear[3] = 0.f;
            radial0[0] = radial0[1] = radial0[2] = radial0[3] = 0.f;
            radial1[0] = radial1[1] = radial1[2] = radial1[3] = 0.f;
            conical_center[0] = conical_center[1] = 0.f;
            conical_angles[0] = conical_angles[1] = 0.f;
            break;
    }
}

static void set_scissor(const lv_area_t * clip, int32_t viewport_h)
{
    if(!clip) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    const int32_t x = clip->x1;
    const int32_t y = viewport_h - clip->y2 - 1;
    const int32_t w = lv_area_get_width(clip);
    const int32_t h = lv_area_get_height(clip);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, LV_MAX(y, 0), LV_MAX(w, 0), LV_MAX(h, 0));
}

#endif /*LV_EVGPU_GRAD_HAS_GL*/

#endif /*LV_USE_DRAW_EVGPU*/
