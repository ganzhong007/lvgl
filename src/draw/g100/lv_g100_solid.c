/**
 * @file lv_g100_solid.c
 */

#include "lv_g100_solid.h"

#if LV_USE_DRAW_G100

#include "lv_draw_g100_private.h"
#include "lv_g100_context.h"
#include "lv_g100_shader.h"
#include "lv_g100_utils.h"

#if LV_USE_OPENGLES && LV_USE_EGL
    #include "../../drivers/opengles/lv_opengles_private.h"
    #define LV_G100_SOLID_HAS_GL 1
#else
    #define LV_G100_SOLID_HAS_GL 0
#endif

typedef struct {
    uint32_t vbo;
    bool ready;
#if LV_G100_SOLID_HAS_GL
    int32_t loc_matrix;
    int32_t loc_view_size;
    int32_t loc_rect;
    int32_t loc_radius;
    int32_t loc_color;
#endif
} lv_g100_solid_state_t;

static lv_g100_solid_state_t g_solid_state;

#if LV_G100_SOLID_HAS_GL
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
#endif

void lv_g100_solid_init(lv_draw_g100_unit_t * unit)
{
    lv_memzero(&g_solid_state, sizeof(g_solid_state));

#if LV_G100_SOLID_HAS_GL
    if(!lv_g100_shader_is_ready(&unit->shader)) return;

    static const float quad[] = {
        0.f, 0.f,  1.f, 0.f,  0.f, 1.f,
        1.f, 0.f,  1.f, 1.f,  0.f, 1.f,
    };

    glGenBuffers(1, &g_solid_state.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_solid_state.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const uint32_t prog = unit->shader.solid_program;
    g_solid_state.loc_matrix = glGetUniformLocation(prog, "u_matrix");
    g_solid_state.loc_view_size = glGetUniformLocation(prog, "u_view_size");
    g_solid_state.loc_rect = glGetUniformLocation(prog, "u_rect");
    g_solid_state.loc_radius = glGetUniformLocation(prog, "u_radius");
    g_solid_state.loc_color = glGetUniformLocation(prog, "u_color");

    g_solid_state.ready = true;
    LV_LOG_INFO("G100 native solid fill ready (program=%u)", (unsigned)prog);
#else
    LV_UNUSED(unit);
#endif
}

void lv_g100_solid_deinit(lv_draw_g100_unit_t * unit)
{
    LV_UNUSED(unit);
#if LV_G100_SOLID_HAS_GL
    if(g_solid_state.vbo) glDeleteBuffers(1, &g_solid_state.vbo);
#endif
    lv_memzero(&g_solid_state, sizeof(g_solid_state));
}

bool lv_g100_solid_fill_rect(lv_draw_g100_unit_t * unit,
                             const lv_area_t * coords,
                             lv_color_t color,
                             lv_opa_t opa,
                             float radius,
                             const lv_area_t * clip_area,
                             const lv_matrix_t * matrix)
{
    LV_ASSERT_NULL(unit);
    LV_ASSERT_NULL(coords);
    LV_ASSERT_NULL(matrix);

#if LV_G100_SOLID_HAS_GL
    if(!g_solid_state.ready || !lv_g100_shader_is_ready(&unit->shader)) return false;

    lv_g100_nvg_flush_pending(unit);

    const int32_t vp_w = unit->ctx.viewport_w > 0 ? unit->ctx.viewport_w :
                         lv_area_get_width(&unit->current_layer->buf_area);
    const int32_t vp_h = unit->ctx.viewport_h > 0 ? unit->ctx.viewport_h :
                         lv_area_get_height(&unit->current_layer->buf_area);

    float rect[4] = {
        (float)coords->x1,
        (float)coords->y1,
        (float)lv_area_get_width(coords),
        (float)lv_area_get_height(coords),
    };

    float xform[6];
    lv_g100_matrix_convert(xform, matrix);
    float mat3[9];
    lv_g100_xform_to_mat3(mat3, xform);

    const float view_size[2] = { (float)vp_w, (float)vp_h };

    const lv_color32_t c32 = lv_color_to_32(color, opa);
    const float col[4] = {
        (float)c32.red / 255.f,
        (float)c32.green / 255.f,
        (float)c32.blue / 255.f,
        (float)c32.alpha / 255.f,
    };

    lv_g100_native_gl_prepare(unit, vp_w, vp_h);
    set_scissor(clip_area, vp_h);

    lv_g100_shader_bind_solid(&unit->ctx, &unit->shader);
    glUniformMatrix3fv(g_solid_state.loc_matrix, 1, GL_FALSE, mat3);
    glUniform2fv(g_solid_state.loc_view_size, 1, view_size);
    glUniform4fv(g_solid_state.loc_rect, 1, rect);
    glUniform1f(g_solid_state.loc_radius, radius);
    glUniform4fv(g_solid_state.loc_color, 1, col);

    glBindBuffer(GL_ARRAY_BUFFER, g_solid_state.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * (GLsizei)sizeof(float), NULL);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    lv_g100_shader_unbind(&unit->ctx);
    lv_g100_native_gl_finish();
    return true;
#else
    LV_UNUSED(color);
    LV_UNUSED(opa);
    LV_UNUSED(radius);
    LV_UNUSED(clip_area);
    return false;
#endif
}

#endif /*LV_USE_DRAW_G100*/
