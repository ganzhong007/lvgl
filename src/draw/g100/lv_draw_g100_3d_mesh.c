/**
 * @file lv_draw_g100_3d_mesh.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_g100_private.h"

#if LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS

#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../include/lvgl/draw/lv_draw_3d_mesh.h"
#include "lv_g100_3d_pass.h"
#include "lv_g100_utils.h"

/*********************
 *      DEFINES
 *********************/

static const char * mesh_vert =
    "attribute vec3 a_pos;\n"
    "uniform mat4 u_mvp;\n"
    "void main() {\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "}\n";

static const char * mesh_frag =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "  gl_FragColor = u_color;\n"
    "}\n";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static bool mesh_shader_init(uint32_t * prog, int32_t * loc_mvp, int32_t * loc_color);
static void mesh_shader_deinit(uint32_t prog);
static void mat4_mul(float out[16], const float a[16], const float b[16]);

/**********************
 *  STATIC VARIABLES
 **********************/

static uint32_t s_mesh_prog;
static int32_t s_loc_mvp;
static int32_t s_loc_color;
static bool s_mesh_ready;
static uint32_t s_mesh_vbo;
static uint32_t s_mesh_ibo;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_g100_3d_mesh_init(void)
{
    if(s_mesh_ready) return;
    s_mesh_ready = mesh_shader_init(&s_mesh_prog, &s_loc_mvp, &s_loc_color);
    if(s_mesh_ready) {
        GL_CALL(glGenBuffers(1, &s_mesh_vbo));
        GL_CALL(glGenBuffers(1, &s_mesh_ibo));
        LV_LOG_INFO("G100 3D mesh ready (MESH pass shader)");
    }
}

void lv_draw_g100_3d_mesh_deinit(void)
{
    if(s_mesh_vbo) {
        GL_CALL(glDeleteBuffers(1, &s_mesh_vbo));
        s_mesh_vbo = 0;
    }
    if(s_mesh_ibo) {
        GL_CALL(glDeleteBuffers(1, &s_mesh_ibo));
        s_mesh_ibo = 0;
    }
    if(s_mesh_prog) {
        mesh_shader_deinit(s_mesh_prog);
        s_mesh_prog = 0;
    }
    s_mesh_ready = false;
}

void lv_draw_g100_3d_mesh(lv_draw_task_t * t, const lv_draw_3d_mesh_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    if(!s_mesh_ready) lv_draw_g100_3d_mesh_init();
    if(!s_mesh_ready || dsc->vertices == NULL || dsc->indices == NULL || dsc->index_count == 0) {
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

    const float * view_proj = lv_draw_3d_pass_get_view_proj(pass_layer);
    if(view_proj == NULL) {
        lv_g100_3d_pass_unbind_fbo();
        LV_PROFILER_DRAW_END;
        return;
    }

    float mvp[16];
    mat4_mul(mvp, view_proj, dsc->model_matrix);

    if(dsc->flags & LV_3D_MESH_FLAG_DEPTH_TEST) {
        GL_CALL(glEnable(GL_DEPTH_TEST));
        GL_CALL(glDepthMask(GL_TRUE));
        GL_CALL(glDepthFunc(GL_LESS));
    }
    else GL_CALL(glDisable(GL_DEPTH_TEST));

    if(dsc->flags & LV_3D_MESH_FLAG_CULL_FACE) {
        GL_CALL(glEnable(GL_CULL_FACE));
        GL_CALL(glCullFace(GL_BACK));
    }
    else {
        GL_CALL(glDisable(GL_CULL_FACE));
    }

    GL_CALL(glUseProgram(s_mesh_prog));
    GL_CALL(glUniformMatrix4fv(s_loc_mvp, 1, GL_FALSE, mvp));
    GL_CALL(glUniform4f(s_loc_color,
                        dsc->color.red / 255.f,
                        dsc->color.green / 255.f,
                        dsc->color.blue / 255.f,
                        dsc->color.alpha / 255.f));

    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_mesh_vbo));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(dsc->vertex_count * 3 * sizeof(float)),
                         dsc->vertices, GL_STREAM_DRAW));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_mesh_ibo));
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(dsc->index_count * sizeof(uint16_t)),
                         dsc->indices, GL_STREAM_DRAW));

    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * (GLsizei)sizeof(float), NULL));
    GL_CALL(glDrawElements(GL_TRIANGLES, (GLsizei)dsc->index_count, GL_UNSIGNED_SHORT, NULL));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    GL_CALL(glUseProgram(0));

    lv_g100_3d_pass_unbind_fbo();

    LV_PROFILER_DRAW_END;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool mesh_shader_init(uint32_t * prog, int32_t * loc_mvp, int32_t * loc_color)
{
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    if(!v || !f) return false;

    glShaderSource(v, 1, &mesh_vert, NULL);
    glShaderSource(f, 1, &mesh_frag, NULL);
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

static void mesh_shader_deinit(uint32_t prog)
{
    GL_CALL(glDeleteProgram(prog));
}

static void mat4_mul(float out[16], const float a[16], const float b[16])
{
    float r[16];
    for(int col = 0; col < 4; col++) {
        for(int row = 0; row < 4; row++) {
            r[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0]
                               + a[1 * 4 + row] * b[col * 4 + 1]
                               + a[2 * 4 + row] * b[col * 4 + 2]
                               + a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    lv_memcpy(out, r, sizeof(r));
}

#endif /* LV_USE_DRAW_G100 && LV_USE_3D_DRAW_TASKS */
