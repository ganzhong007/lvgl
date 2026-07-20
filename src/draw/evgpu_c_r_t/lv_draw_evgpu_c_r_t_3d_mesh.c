/**
 * @file lv_draw_evgpu_c_r_t_3d_mesh.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../drivers/opengles/lv_opengles_private.h"
#define LV_EVGPU_C_R_T_SKIP_SYSTEM_GLES 1
#include "lv_draw_evgpu_c_r_t_private.h"
#include "../lv_draw_3d_pass.h"

#if LV_USE_DRAW_EVGPU_C_R_T && LV_USE_3D_DRAW_TASKS
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../include/lvgl/draw/lv_draw_3d_mesh.h"
#include "../../include/lvgl/draw/lv_draw_3d_light.h"
#include "../../include/lvgl/draw/lv_draw_3d_camera.h"
#include "../lv_draw_3d_pass.h"

/*********************
 *      DEFINES
 *********************/

#define PHONG_MAX_LIGHTS 4

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

static const char * phong_vert =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_normal;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_model;\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_world_pos;\n"
    "void main() {\n"
    "  vec4 world = u_model * vec4(a_pos, 1.0);\n"
    "  v_world_pos = world.xyz;\n"
    "  v_normal = normalize(mat3(u_model) * a_normal);\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "}\n";

static const char * phong_frag =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform float u_shininess;\n"
    "uniform float u_ambient;\n"
    "uniform vec3 u_view_pos;\n"
    "uniform float u_light_count;\n"
    "uniform vec4 u_light_pos[4];\n"
    "uniform vec4 u_light_color[4];\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_world_pos;\n"
    "void main() {\n"
    "  vec3 norm = normalize(v_normal);\n"
    "  vec3 viewDir = normalize(u_view_pos - v_world_pos);\n"
    "  vec3 result = u_color.rgb * u_ambient;\n"
    "  int count = int(u_light_count + 0.5);\n"
    "  for(int i = 0; i < 4; i++) {\n"
    "    if(i >= count) break;\n"
    "    vec4 lp = u_light_pos[i];\n"
    "    vec3 lc = u_light_color[i].rgb;\n"
    "    vec3 ldir;\n"
    "    float atten = 1.0;\n"
    "    if(lp.w < 0.5) {\n"
    "      ldir = normalize(-lp.xyz);\n"
    "    } else {\n"
    "      vec3 toLight = lp.xyz - v_world_pos;\n"
    "      float dist = length(toLight);\n"
    "      ldir = toLight / max(dist, 0.0001);\n"
    "      float range = u_light_color[i].a;\n"
    "      if(range > 0.0) atten = clamp(1.0 - dist / range, 0.0, 1.0);\n"
    "    }\n"
    "    float diff = max(dot(norm, ldir), 0.0);\n"
    "    vec3 reflectDir = reflect(-ldir, norm);\n"
    "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_shininess);\n"
    "    result += (u_color.rgb * diff + vec3(spec)) * lc * atten;\n"
    "  }\n"
    "  gl_FragColor = vec4(result, u_color.a);\n"
    "}\n";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static bool mesh_shader_init(uint32_t * prog, int32_t * loc_mvp, int32_t * loc_color);
static bool phong_shader_init(uint32_t * prog, int32_t * locs, int32_t loc_count);
static void mesh_shader_deinit(uint32_t prog);
static void mat4_mul(float out[16], const float a[16], const float b[16]);
static void upload_lights(const lv_layer_t * pass_layer, int32_t loc_pos, int32_t loc_color,
                          int32_t loc_count, float default_dir[3]);

/**********************
 *  STATIC VARIABLES
 **********************/

static uint32_t s_mesh_prog;
static int32_t s_loc_mvp;
static int32_t s_loc_color;
static bool s_mesh_ready;

static uint32_t s_phong_prog;
static int32_t s_phong_locs[12];
static bool s_phong_ready;

static uint32_t s_mesh_vbo;
static uint32_t s_mesh_nbo;
static uint32_t s_mesh_ibo;

enum {
    PHONG_LOC_MVP = 0,
    PHONG_LOC_MODEL,
    PHONG_LOC_COLOR,
    PHONG_LOC_SHININESS,
    PHONG_LOC_AMBIENT,
    PHONG_LOC_VIEW_POS,
    PHONG_LOC_LIGHT_COUNT,
    PHONG_LOC_LIGHT_POS,
    PHONG_LOC_LIGHT_COLOR,
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_evgpu_c_r_t_3d_mesh_init(void)
{
    if(s_mesh_ready) return;
    s_mesh_ready = mesh_shader_init(&s_mesh_prog, &s_loc_mvp, &s_loc_color);
    if(s_mesh_ready) {
        GL_CALL(glGenBuffers(1, &s_mesh_vbo));
        GL_CALL(glGenBuffers(1, &s_mesh_nbo));
        GL_CALL(glGenBuffers(1, &s_mesh_ibo));
        LV_LOG_INFO("EVGPU_C_R_T 3D mesh ready (MESH pass shader)");
    }

    if(!s_phong_ready) {
        s_phong_ready = phong_shader_init(&s_phong_prog, s_phong_locs, 12);
        if(s_phong_ready) {
            LV_LOG_INFO("EVGPU_C_R_T 3D phong ready (MESH phong lights)");
        }
    }
}

void lv_draw_evgpu_c_r_t_3d_mesh_deinit(void)
{
    if(s_mesh_vbo) {
        GL_CALL(glDeleteBuffers(1, &s_mesh_vbo));
        s_mesh_vbo = 0;
    }
    if(s_mesh_nbo) {
        GL_CALL(glDeleteBuffers(1, &s_mesh_nbo));
        s_mesh_nbo = 0;
    }
    if(s_mesh_ibo) {
        GL_CALL(glDeleteBuffers(1, &s_mesh_ibo));
        s_mesh_ibo = 0;
    }
    if(s_mesh_prog) {
        mesh_shader_deinit(s_mesh_prog);
        s_mesh_prog = 0;
    }
    if(s_phong_prog) {
        mesh_shader_deinit(s_phong_prog);
        s_phong_prog = 0;
    }
    s_mesh_ready = false;
    s_phong_ready = false;
}

void lv_draw_evgpu_c_r_t_3d_mesh(lv_draw_task_t * t, const lv_draw_3d_mesh_dsc_t * dsc)
{
    LV_PROFILER_DRAW_BEGIN;

    if(!s_mesh_ready) lv_draw_evgpu_c_r_t_3d_mesh_init();
    if(!s_mesh_ready || dsc->vertices == NULL || dsc->indices == NULL || dsc->index_count == 0) {
        LV_PROFILER_DRAW_END;
        return;
    }

    bool use_phong = (dsc->flags & LV_3D_MESH_FLAG_PHONG) && dsc->normals != NULL && s_phong_ready;

    lv_draw_evgpu_c_r_t_unit_t * u = (lv_draw_evgpu_c_r_t_unit_t *)t->draw_unit;
    lv_layer_t * pass_layer = t->target_layer;
    int32_t w, h;
    lv_evgpu_3d_pass_fbo_t * fbo;

    lv_evgpu_c_r_t_end_frame(u);
    lv_opengles_reinit_state();

    if(!lv_evgpu_3d_pass_bind_fbo(pass_layer, &w, &h, &fbo)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    const float * view_proj = lv_draw_3d_pass_get_view_proj(pass_layer);
    if(view_proj == NULL) {
        lv_evgpu_3d_pass_unbind_fbo();
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

    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_mesh_vbo));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(dsc->vertex_count * 3 * sizeof(float)),
                         dsc->vertices, GL_STREAM_DRAW));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_mesh_ibo));
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(dsc->index_count * sizeof(uint16_t)),
                         dsc->indices, GL_STREAM_DRAW));

    if(use_phong) {
        GL_CALL(glUseProgram(s_phong_prog));
        GL_CALL(glUniformMatrix4fv(s_phong_locs[PHONG_LOC_MVP], 1, GL_FALSE, mvp));
        GL_CALL(glUniformMatrix4fv(s_phong_locs[PHONG_LOC_MODEL], 1, GL_FALSE, dsc->model_matrix));
        GL_CALL(glUniform4f(s_phong_locs[PHONG_LOC_COLOR],
                            dsc->color.red / 255.f,
                            dsc->color.green / 255.f,
                            dsc->color.blue / 255.f,
                            dsc->color.alpha / 255.f));
        GL_CALL(glUniform1f(s_phong_locs[PHONG_LOC_SHININESS], dsc->shininess));
        GL_CALL(glUniform1f(s_phong_locs[PHONG_LOC_AMBIENT], dsc->ambient));

        const lv_3d_camera_t * cam = lv_draw_3d_pass_get_camera(pass_layer);
        lv_3dpoint_t eye = { 0.f, 0.f, 0.f };
        if(cam != NULL) lv_3d_camera_get_eye(cam, &eye);
        GL_CALL(glUniform3f(s_phong_locs[PHONG_LOC_VIEW_POS], eye.x, eye.y, eye.z));

        float default_dir[3] = { 0.4f, -1.f, 0.3f };
        upload_lights(pass_layer, s_phong_locs[PHONG_LOC_LIGHT_POS], s_phong_locs[PHONG_LOC_LIGHT_COLOR],
                      s_phong_locs[PHONG_LOC_LIGHT_COUNT], default_dir);

        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_mesh_nbo));
        GL_CALL(glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(dsc->vertex_count * 3 * sizeof(float)),
                             dsc->normals, GL_STREAM_DRAW));

        GL_CALL(glEnableVertexAttribArray(0));
        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_mesh_vbo));
        GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * (GLsizei)sizeof(float), NULL));
        GL_CALL(glEnableVertexAttribArray(1));
        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_mesh_nbo));
        GL_CALL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * (GLsizei)sizeof(float), NULL));
    }
    else {
        GL_CALL(glUseProgram(s_mesh_prog));
        GL_CALL(glUniformMatrix4fv(s_loc_mvp, 1, GL_FALSE, mvp));
        GL_CALL(glUniform4f(s_loc_color,
                            dsc->color.red / 255.f,
                            dsc->color.green / 255.f,
                            dsc->color.blue / 255.f,
                            dsc->color.alpha / 255.f));
        GL_CALL(glEnableVertexAttribArray(0));
        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_mesh_vbo));
        GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * (GLsizei)sizeof(float), NULL));
    }

    GL_CALL(glDrawElements(GL_TRIANGLES, (GLsizei)dsc->index_count, GL_UNSIGNED_SHORT, NULL));
    GL_CALL(glDisableVertexAttribArray(0));
    if(use_phong) GL_CALL(glDisableVertexAttribArray(1));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    GL_CALL(glUseProgram(0));

    lv_evgpu_3d_pass_unbind_fbo();

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

static bool phong_shader_init(uint32_t * prog, int32_t * locs, int32_t loc_count)
{
    LV_UNUSED(loc_count);
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    if(!v || !f) return false;

    glShaderSource(v, 1, &phong_vert, NULL);
    glShaderSource(f, 1, &phong_frag, NULL);
    glCompileShader(v);
    glCompileShader(f);

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_normal");
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
    locs[PHONG_LOC_MVP] = glGetUniformLocation(p, "u_mvp");
    locs[PHONG_LOC_MODEL] = glGetUniformLocation(p, "u_model");
    locs[PHONG_LOC_COLOR] = glGetUniformLocation(p, "u_color");
    locs[PHONG_LOC_SHININESS] = glGetUniformLocation(p, "u_shininess");
    locs[PHONG_LOC_AMBIENT] = glGetUniformLocation(p, "u_ambient");
    locs[PHONG_LOC_VIEW_POS] = glGetUniformLocation(p, "u_view_pos");
    locs[PHONG_LOC_LIGHT_COUNT] = glGetUniformLocation(p, "u_light_count");
    locs[PHONG_LOC_LIGHT_POS] = glGetUniformLocation(p, "u_light_pos");
    locs[PHONG_LOC_LIGHT_COLOR] = glGetUniformLocation(p, "u_light_color");

    return locs[PHONG_LOC_MVP] >= 0 && locs[PHONG_LOC_MODEL] >= 0 && locs[PHONG_LOC_COLOR] >= 0
           && locs[PHONG_LOC_LIGHT_POS] >= 0 && locs[PHONG_LOC_LIGHT_COLOR] >= 0;
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

static void upload_lights(const lv_layer_t * pass_layer, int32_t loc_pos, int32_t loc_color,
                          int32_t loc_count, float default_dir[3])
{
    float pos[PHONG_MAX_LIGHTS * 4];
    float col[PHONG_MAX_LIGHTS * 4];
    uint32_t count = lv_draw_3d_pass_get_light_count(pass_layer);
    const lv_3d_light_dsc_t * lights = lv_draw_3d_pass_get_lights(pass_layer);

    if(count == 0) {
        pos[0] = default_dir[0];
        pos[1] = default_dir[1];
        pos[2] = default_dir[2];
        pos[3] = 0.f;
        col[0] = 1.f;
        col[1] = 1.f;
        col[2] = 1.f;
        col[3] = 0.f;
        count = 1;
    }
    else {
        for(uint32_t i = 0; i < count && i < PHONG_MAX_LIGHTS; i++) {
            const lv_3d_light_dsc_t * l = &lights[i];
            float * p = &pos[i * 4];
            float * c = &col[i * 4];
            if(l->type == LV_3D_LIGHT_TYPE_DIRECTIONAL) {
                p[0] = l->direction[0];
                p[1] = l->direction[1];
                p[2] = l->direction[2];
                p[3] = 0.f;
            }
            else {
                p[0] = l->position[0];
                p[1] = l->position[1];
                p[2] = l->position[2];
                p[3] = 1.f;
            }
            c[0] = (l->color.red / 255.f) * l->intensity;
            c[1] = (l->color.green / 255.f) * l->intensity;
            c[2] = (l->color.blue / 255.f) * l->intensity;
            c[3] = l->range;
        }
    }

    GL_CALL(glUniform1f(loc_count, (float)count));
    GL_CALL(glUniform4fv(loc_pos, (GLsizei)count, pos));
    GL_CALL(glUniform4fv(loc_color, (GLsizei)count, col));
}

#endif /* LV_USE_DRAW_EVGPU_C_R_T && LV_USE_3D_DRAW_TASKS */
