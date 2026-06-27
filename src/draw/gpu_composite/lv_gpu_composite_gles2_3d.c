/**
 * @file lv_gpu_composite_gles2_3d.c — GLES2 wireframe box renderer [GL2]
 */

#include "lv_gpu_composite_gles2_3d.h"

#if LV_USE_DRAW_GPU_COMPOSITE && LV_USE_3D

#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../misc/lv_color.h"
#include <stdlib.h>
#include <string.h>

static unsigned int prog;
static int loc_mvp;
static int loc_color;

#if LV_USE_EGL
static const char * vs_src =
    "attribute vec3 a_pos;\n"
    "uniform mat4 u_mvp;\n"
    "void main() { gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char * fs_src =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "void main() { gl_FragColor = u_color; }\n";
#else
static const char * vs_src =
    "#version 120\n"
    "attribute vec3 a_pos;\n"
    "uniform mat4 u_mvp;\n"
    "void main() { gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char * fs_src =
    "#version 120\n"
    "uniform vec4 u_color;\n"
    "void main() { gl_FragColor = u_color; }\n";
#endif

static bool shader_ok(unsigned int sh)
{
    int ok = 0;
    GL_CALL(glGetShaderiv(sh, GL_COMPILE_STATUS, &ok));
    if(!ok) {
        char log[512];
        log[0] = '\0';
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        LV_LOG_ERROR("LVGL shader compile failed: %s", log);
    }
    return ok != 0;
}

static unsigned int compile_shader(unsigned int type, const char * src)
{
    unsigned int sh = glCreateShader(type);
    GL_CALL(glShaderSource(sh, 1, &src, NULL));
    GL_CALL(glCompileShader(sh));
    if(!shader_ok(sh)) {
        GL_CALL(glDeleteShader(sh));
        return 0;
    }
    return sh;
}

static unsigned int link_program(const char * vs, const char * fs)
{
    unsigned int v = compile_shader(GL_VERTEX_SHADER, vs);
    unsigned int f = compile_shader(GL_FRAGMENT_SHADER, fs);
    if(!v || !f) {
        if(v) GL_CALL(glDeleteShader(v));
        if(f) GL_CALL(glDeleteShader(f));
        return 0;
    }
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
        char log[512];
        log[0] = '\0';
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        LV_LOG_ERROR("LVGL shader link failed: %s", log);
        GL_CALL(glDeleteProgram(p));
        return 0;
    }
    return p;
}

void lv_gpu_composite_gles2_3d_init(void)
{
    if(prog) return;
    prog = link_program(vs_src, fs_src);
    if(!prog) return;
    loc_mvp = glGetUniformLocation(prog, "u_mvp");
    loc_color = glGetUniformLocation(prog, "u_color");
}

void lv_gpu_composite_gles2_3d_deinit(void)
{
    if(prog) {
        GL_CALL(glDeleteProgram(prog));
        prog = 0;
    }
}

static void box_wire_verts(float w, float h, float d, float * out24)
{
    float hx = w * 0.5f, hy = h * 0.5f, hz = d * 0.5f;
    const float v[8][3] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz},
    };
    static const int edges[24] = {
        0,1, 1,2, 2,3, 3,0, 4,5, 5,6, 6,7, 7,4, 0,4, 1,5, 2,6, 3,7
    };
    for(int i = 0; i < 24; i++) {
        out24[i * 3 + 0] = v[edges[i]][0];
        out24[i * 3 + 1] = v[edges[i]][1];
        out24[i * 3 + 2] = v[edges[i]][2];
    }
}

static void box_solid_verts(float w, float h, float d, float * out108)
{
    float hx = w * 0.5f, hy = h * 0.5f, hz = d * 0.5f;
    const float v[8][3] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz},
    };
    static const int faces[36] = {
        0,1,2, 0,2,3, 5,4,7, 5,7,6, 4,0,3, 4,3,7,
        1,5,6, 1,6,2, 3,2,6, 3,6,7, 4,5,1, 4,1,0
    };
    for(int i = 0; i < 36; i++) {
        out108[i * 3 + 0] = v[faces[i]][0];
        out108[i * 3 + 1] = v[faces[i]][1];
        out108[i * 3 + 2] = v[faces[i]][2];
    }
}

static void draw_item(const lv_3d_draw_item_t * it, const float view[16], const float proj[16])
{
    float mvp[16], mv[16];
    lv_3d_mat4_mul(mv, view, it->transform.world);
    lv_3d_mat4_mul(mvp, proj, mv);
    GL_CALL(glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, mvp));

    lv_color32_t c32 = lv_color_to_32(it->material.color, it->material.opa);
    GL_CALL(glUniform4f(loc_color, c32.red / 255.0f, c32.green / 255.0f, c32.blue / 255.0f,
                        c32.alpha / 255.0f));

    float verts[108];
    if(it->wireframe || it->material.kind == LV_3D_MAT_WIREFRAME) {
        box_solid_verts(it->w, it->h, it->d, verts);
        GL_CALL(glEnableVertexAttribArray(0));
        GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, verts));
        GL_CALL(glUniform4f(loc_color, c32.red / 255.0f, c32.green / 255.0f, c32.blue / 255.0f, 0.08f));
        GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 36));
        box_wire_verts(it->w, it->h, it->d, verts);
        GL_CALL(glUniform4f(loc_color, c32.red / 255.0f, c32.green / 255.0f, c32.blue / 255.0f,
                            c32.alpha / 255.0f));
        GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, verts));
        GL_CALL(glLineWidth(2.0f));
        GL_CALL(glDrawArrays(GL_LINES, 0, 24));
        GL_CALL(glDisableVertexAttribArray(0));
    }
    else {
        box_solid_verts(it->w, it->h, it->d, verts);
        GL_CALL(glEnableVertexAttribArray(0));
        GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, verts));
        GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 36));
        GL_CALL(glDisableVertexAttribArray(0));
    }
}

void lv_gpu_composite_gles2_render_viewport(unsigned int color_tex, unsigned int depth_rb,
                                            int32_t x, int32_t y, int32_t w, int32_t h,
                                            const float view[16], const float proj[16],
                                            const lv_3d_draw_item_t * items, uint32_t item_count,
                                            bool ar_passthrough, uint8_t * max_alpha_out)
{
    if(max_alpha_out) *max_alpha_out = 0;
    if(!prog) lv_gpu_composite_gles2_3d_init();
    if(!prog) return;

    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    unsigned int fbo = 0;
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0));
    if(depth_rb) {
        GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb));
    }

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LV_LOG_ERROR("LVGL FBO incomplete");
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDeleteFramebuffers(1, &fbo));
        return;
    }

#if !LV_USE_EGL
    {
        GLenum draw_buf = GL_COLOR_ATTACHMENT0;
        GL_CALL(glDrawBuffers(1, &draw_buf));
        GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0));
    }
#endif

    GL_CALL(glViewport(x, y, w, h));
    GL_CALL(glEnable(GL_SCISSOR_TEST));
    GL_CALL(glScissor(x, y, w, h));

    if(ar_passthrough) {
        GL_CALL(glClearColor(0, 0, 0, 0));
    }
    else {
        GL_CALL(glClearColor(0, 0, 0, 1));
    }
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
    GL_CALL(glDisable(GL_DEPTH_TEST));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL_CALL(glUseProgram(prog));

    for(uint32_t i = 0; i < item_count; i++) {
        draw_item(&items[i], view, proj);
    }

    if(max_alpha_out) {
        uint8_t probe[4];
        static const int ppts[9][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                       {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        for(int pi = 0; pi < 9; pi++) {
            int px = x + w / 2 + ppts[pi][0] * (w / 8);
            int py = y + h / 2 + ppts[pi][1] * (h / 8);
            GL_CALL(glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, probe));
            if(probe[3] > *max_alpha_out) *max_alpha_out = probe[3];
        }
    }

    GL_CALL(glDisable(GL_SCISSOR_TEST));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &fbo));
}

#endif /*LV_USE_DRAW_GPU_COMPOSITE && LV_USE_3D*/
