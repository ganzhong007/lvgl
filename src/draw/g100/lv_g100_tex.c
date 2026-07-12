/**
 * @file lv_g100_tex.c
 */

#include "lv_g100_tex.h"

#if LV_USE_DRAW_G100

#include "lv_draw_g100_private.h"
#include "lv_g100_context.h"
#include "lv_g100_shader.h"
#include "lv_g100_utils.h"

#if LV_USE_OPENGLES && LV_USE_EGL
    #include "../../drivers/opengles/lv_opengles_private.h"
    #define LV_G100_TEX_HAS_GL 1
    extern GLuint nvglImageHandleGLES2(NVGcontext * ctx, int image);
#else
    #define LV_G100_TEX_HAS_GL 0
#endif

typedef struct {
    uint32_t vbo;
    bool ready;
#if LV_G100_TEX_HAS_GL
    int32_t loc_matrix;
    int32_t loc_view_size;
    int32_t loc_texture;
    int32_t loc_opa;
    int32_t loc_recolor;
    int32_t loc_recolor_opa;
#endif
} lv_g100_tex_state_t;

static lv_g100_tex_state_t g_tex_state;

#if LV_G100_TEX_HAS_GL
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

static void build_local_matrix(lv_matrix_t * out, const lv_matrix_t * matrix,
                               float x, float y, float w, float h)
{
    lv_matrix_t local;
    lv_matrix_identity(&local);
    local.m[0][0] = w;
    local.m[1][1] = h;
    local.m[0][2] = x;
    local.m[1][2] = y;
    *out = *matrix;
    lv_matrix_multiply(out, &local);
}
#endif

void lv_g100_tex_init(lv_draw_g100_unit_t * unit)
{
    lv_memzero(&g_tex_state, sizeof(g_tex_state));

#if LV_G100_TEX_HAS_GL
    if(!lv_g100_shader_tex_is_ready(&unit->shader)) return;

    static const float quad[] = {
        0.f, 0.f, 0.f, 0.f,
        1.f, 0.f, 1.f, 0.f,
        0.f, 1.f, 0.f, 1.f,
        1.f, 0.f, 1.f, 0.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, 1.f, 0.f, 1.f,
    };

    glGenBuffers(1, &g_tex_state.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_tex_state.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const uint32_t prog = unit->shader.tex_program;
    g_tex_state.loc_matrix = glGetUniformLocation(prog, "u_matrix");
    g_tex_state.loc_view_size = glGetUniformLocation(prog, "u_view_size");
    g_tex_state.loc_texture = glGetUniformLocation(prog, "u_texture");
    g_tex_state.loc_opa = glGetUniformLocation(prog, "u_opa");
    g_tex_state.loc_recolor = glGetUniformLocation(prog, "u_recolor");
    g_tex_state.loc_recolor_opa = glGetUniformLocation(prog, "u_recolor_opa");

    g_tex_state.ready = true;
    LV_LOG_INFO("G100 native texture draw ready (program=%u)", (unsigned)prog);
#else
    LV_UNUSED(unit);
#endif
}

void lv_g100_tex_deinit(lv_draw_g100_unit_t * unit)
{
    LV_UNUSED(unit);
#if LV_G100_TEX_HAS_GL
    if(g_tex_state.vbo) glDeleteBuffers(1, &g_tex_state.vbo);
#endif
    lv_memzero(&g_tex_state, sizeof(g_tex_state));
}

bool lv_g100_tex_draw_image(lv_draw_g100_unit_t * unit,
                            const lv_draw_image_dsc_t * dsc,
                            const lv_area_t * coords,
                            int32_t rect_w,
                            int32_t rect_h,
                            int image_handle,
                            const lv_area_t * clip_area,
                            const lv_matrix_t * matrix)
{
    LV_ASSERT_NULL(unit);
    LV_ASSERT_NULL(dsc);
    LV_ASSERT_NULL(coords);
    LV_ASSERT_NULL(matrix);

#if LV_G100_TEX_HAS_GL
    if(!g_tex_state.ready || !lv_g100_shader_tex_is_ready(&unit->shader)) return false;
    if(image_handle < 0) return false;

    lv_g100_nvg_flush_pending(unit);

    lv_matrix_t draw_matrix;
    build_local_matrix(&draw_matrix, matrix, 0.f, 0.f, (float)rect_w, (float)rect_h);

    float xform[6];
    lv_g100_matrix_convert(xform, &draw_matrix);

    const int32_t vp_w = unit->ctx.viewport_w > 0 ? unit->ctx.viewport_w :
                         lv_area_get_width(&unit->current_layer->buf_area);
    const int32_t vp_h = unit->ctx.viewport_h > 0 ? unit->ctx.viewport_h :
                         lv_area_get_height(&unit->current_layer->buf_area);
    const float view_size[2] = { (float)vp_w, (float)vp_h };

    const GLuint tex = nvglImageHandleGLES2(unit->vg, image_handle);
    if(tex == 0) return false;

    const float recolor[4] = {
        (float)dsc->recolor.red / 255.f,
        (float)dsc->recolor.green / 255.f,
        (float)dsc->recolor.blue / 255.f,
        (float)dsc->recolor_opa / 255.f,
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    set_scissor(clip_area, vp_h);

    lv_g100_shader_bind_tex(&unit->ctx, &unit->shader);
    glUniformMatrix3fv(g_tex_state.loc_matrix, 1, GL_FALSE, xform);
    glUniform2fv(g_tex_state.loc_view_size, 1, view_size);
    glUniform1i(g_tex_state.loc_texture, 0);
    glUniform1f(g_tex_state.loc_opa, (float)dsc->opa / (float)LV_OPA_COVER);
    glUniform4fv(g_tex_state.loc_recolor, 1, recolor);
    glUniform1f(g_tex_state.loc_recolor_opa, recolor[3]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBindBuffer(GL_ARRAY_BUFFER, g_tex_state.vbo);
    const GLsizei stride = (GLsizei)(4 * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (const void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    lv_g100_shader_unbind(&unit->ctx);
    glDisable(GL_SCISSOR_TEST);
    return true;
#else
    LV_UNUSED(rect_w);
    LV_UNUSED(rect_h);
    LV_UNUSED(image_handle);
    LV_UNUSED(clip_area);
    return false;
#endif
}

#endif /*LV_USE_DRAW_G100*/
