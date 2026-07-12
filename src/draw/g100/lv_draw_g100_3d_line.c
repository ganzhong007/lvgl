/**
 * @file lv_draw_g100_3d_line.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_g100_private.h"

#if LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS

#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../include/lvgl/draw/lv_draw_3d_line.h"
#include "lv_g100_3d_pass.h"
#include "lv_g100_utils.h"

/*********************
 *      DEFINES
 *********************/

static const char * line_vert =
    "attribute vec3 a_pos;\n"
    "uniform mat4 u_mvp;\n"
    "void main() {\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "}\n";

static const char * line_frag =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "  gl_FragColor = u_color;\n"
    "}\n";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static bool line_shader_init(uint32_t * prog, int32_t * loc_mvp, int32_t * loc_color);
static void line_shader_deinit(uint32_t prog);

/**********************
 *  STATIC VARIABLES
 **********************/

static uint32_t s_line_prog;
static int32_t s_loc_mvp;
static int32_t s_loc_color;
static bool s_line_ready;
static uint32_t s_line_vbo;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_g100_3d_line_init(void)
{
    if(s_line_ready) return;
    s_line_ready = line_shader_init(&s_line_prog, &s_loc_mvp, &s_loc_color);
    if(s_line_ready) {
        GL_CALL(glGenBuffers(1, &s_line_vbo));
        LV_LOG_INFO("G100 3D line ready (LINE3D pass shader)");
    }
}

void lv_draw_g100_3d_line_deinit(void)
{
    if(s_line_vbo) {
        GL_CALL(glDeleteBuffers(1, &s_line_vbo));
        s_line_vbo = 0;
    }
    if(s_line_prog) {
        line_shader_deinit(s_line_prog);
        s_line_prog = 0;
    }
    s_line_ready = false;
}

void lv_draw_g100_3d_line(lv_draw_task_t * t, const lv_draw_3d_line_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    if(!s_line_ready) lv_draw_g100_3d_line_init();
    if(!s_line_ready || dsc->points == NULL || dsc->point_cnt < 2) {
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_draw_g100_unit_t * u = (lv_draw_g100_unit_t *)t->draw_unit;
    lv_layer_t * pass_layer = t->target_layer;
    int32_t w, h;
    lv_g100_3d_pass_fbo_t * fbo;

    lv_g100_end_frame(u);
    lv_opengles_reinit_state();

    if(!lv_g100_3d_pass_bind_fbo(pass_layer, &w, &h, &fbo)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    const float * mvp = lv_draw_3d_pass_get_view_proj(pass_layer);
    if(mvp == NULL) {
        lv_g100_3d_pass_unbind_fbo();
        LV_PROFILER_DRAW_END;
        return;
    }

    if(dsc->depth_test) GL_CALL(glEnable(GL_DEPTH_TEST));
    else GL_CALL(glDisable(GL_DEPTH_TEST));
    GL_CALL(glLineWidth(dsc->width > 0.f ? dsc->width : 1.f));

    GL_CALL(glUseProgram(s_line_prog));
    GL_CALL(glUniformMatrix4fv(s_loc_mvp, 1, GL_FALSE, mvp));
    GL_CALL(glUniform4f(s_loc_color,
                        dsc->color.red / 255.f,
                        dsc->color.green / 255.f,
                        dsc->color.blue / 255.f,
                        dsc->color.alpha / 255.f));

    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_line_vbo));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(dsc->point_cnt * sizeof(lv_3dpoint_t)),
                         dsc->points, GL_STREAM_DRAW));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(lv_3dpoint_t), NULL));
    GL_CALL(glDrawArrays(GL_LINES, 0, (GLsizei)dsc->point_cnt));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CALL(glUseProgram(0));

    lv_g100_3d_pass_unbind_fbo();

    LV_PROFILER_DRAW_END;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool line_shader_init(uint32_t * prog, int32_t * loc_mvp, int32_t * loc_color)
{
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    if(!v || !f) return false;

    glShaderSource(v, 1, &line_vert, NULL);
    glShaderSource(f, 1, &line_frag, NULL);
    glCompileShader(v);
    glCompileShader(f);

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "a_pos");
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok) {
        glDeleteProgram(p);
        return false;
    }

    *prog = p;
    *loc_mvp = glGetUniformLocation(p, "u_mvp");
    *loc_color = glGetUniformLocation(p, "u_color");
    return *loc_mvp >= 0 && *loc_color >= 0;
}

static void line_shader_deinit(uint32_t prog)
{
    GL_CALL(glDeleteProgram(prog));
}

#endif /* LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS */
