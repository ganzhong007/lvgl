#include "lv_evgpu_c_r_t_kawase.h"
#include "lv_evgpu_c_r_t_fbo.h"
#include <GLES2/gl2.h>
#include <string.h>

#if LV_USE_DRAW_EVGPU_C_R_T

#define CRT_KAWASE_MAX_PASSES 6

static struct {
    bool ready;
    GLuint down_prog;
    GLuint up_prog;
    GLuint vbo;
    GLint down_loc_tex;
    GLint down_loc_half_px;
    GLint up_loc_tex;
    GLint up_loc_half_px;
    GLint up_loc_recolor;
    GLint attr_pos;
    GLint attr_uv;
    GLuint capture_tex;
    int capture_w;
    int capture_h;
} g_crt_kawase;

static const char crt_kawase_vs_src[] =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main(void) {\n"
    "  v_uv = a_uv;\n"
    "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char crt_kawase_down_fs_src[] =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_half_px;\n"
    "void main(void) {\n"
    "  vec2 d = u_half_px;\n"
    "  vec4 c = texture2D(u_tex, v_uv) * 4.0;\n"
    "  c += texture2D(u_tex, v_uv + vec2( d.x,  d.y));\n"
    "  c += texture2D(u_tex, v_uv + vec2( d.x, -d.y));\n"
    "  c += texture2D(u_tex, v_uv + vec2(-d.x,  d.y));\n"
    "  c += texture2D(u_tex, v_uv + vec2(-d.x, -d.y));\n"
    "  gl_FragColor = c / 8.0;\n"
    "}\n";

static const char crt_kawase_up_fs_src[] =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_half_px;\n"
    "uniform vec4 u_recolor;\n"
    "void main(void) {\n"
    "  vec2 d = u_half_px;\n"
    "  vec4 c = texture2D(u_tex, v_uv) * 4.0;\n"
    "  c += texture2D(u_tex, v_uv + vec2( d.x,  d.y));\n"
    "  c += texture2D(u_tex, v_uv + vec2( d.x, -d.y));\n"
    "  c += texture2D(u_tex, v_uv + vec2(-d.x,  d.y));\n"
    "  c += texture2D(u_tex, v_uv + vec2(-d.x, -d.y));\n"
    "  c /= 8.0;\n"
    "  if(u_recolor.a > 0.5) {\n"
    "    c = vec4(u_recolor.rgb * c.a, c.a);\n"
    "  }\n"
    "  gl_FragColor = c;\n"
    "}\n";

static GLuint crt_kawase_compile_shader(GLenum type, const char * src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint crt_kawase_link_program(GLuint vs, GLuint fs) {
    if(!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a_pos");
    glBindAttribLocation(prog, 1, "a_uv");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static const float crt_kawase_quad[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
};

static void ensure_capture_tex(int w, int h)
{
    if(g_crt_kawase.capture_tex && g_crt_kawase.capture_w == w && g_crt_kawase.capture_h == h) return;

    if(g_crt_kawase.capture_tex) glDeleteTextures(1, &g_crt_kawase.capture_tex);
    glGenTextures(1, &g_crt_kawase.capture_tex);
    glBindTexture(GL_TEXTURE_2D, g_crt_kawase.capture_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_crt_kawase.capture_w = w;
    g_crt_kawase.capture_h = h;
}

static void bind_quad_attrs(void)
{
    glBindBuffer(GL_ARRAY_BUFFER, g_crt_kawase.vbo);
    glEnableVertexAttribArray((GLuint)g_crt_kawase.attr_pos);
    glVertexAttribPointer((GLuint)g_crt_kawase.attr_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (const void *)0);
    glEnableVertexAttribArray((GLuint)g_crt_kawase.attr_uv);
    glVertexAttribPointer((GLuint)g_crt_kawase.attr_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                          (const void *)(sizeof(float) * 2));
}

static void draw_kawase_pass(GLuint prog, GLuint src_tex, int dst_w, int dst_h,
                              float half_px_x, float half_px_y,
                              float recolor_r, float recolor_g, float recolor_b, float recolor_a,
                              bool is_up)
{
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glUniform1i(glGetUniformLocation(prog, "u_tex"), 0);
    glUniform2f(is_up ? g_crt_kawase.up_loc_half_px : g_crt_kawase.down_loc_half_px, half_px_x, half_px_y);
    if(is_up) {
        glUniform4f(g_crt_kawase.up_loc_recolor, recolor_r, recolor_g, recolor_b, recolor_a);
    }
    glViewport(0, 0, dst_w, dst_h);
    bind_quad_attrs();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static int calc_passes(int radius)
{
    int passes = 0;
    int r = radius;
    while(r > 32 && passes < CRT_KAWASE_MAX_PASSES) {
        passes++;
        r = (r + 1) / 2;
    }
    return passes > 0 ? passes : 1;
}

void lv_evgpu_c_r_t_kawase_init(void)
{
    lv_memzero(&g_crt_kawase, sizeof(g_crt_kawase));

    GLuint vs = crt_kawase_compile_shader(GL_VERTEX_SHADER, crt_kawase_vs_src);
    g_crt_kawase.down_prog = crt_kawase_link_program(vs, crt_kawase_compile_shader(GL_FRAGMENT_SHADER, crt_kawase_down_fs_src));
    vs = crt_kawase_compile_shader(GL_VERTEX_SHADER, crt_kawase_vs_src);
    g_crt_kawase.up_prog = crt_kawase_link_program(vs, crt_kawase_compile_shader(GL_FRAGMENT_SHADER, crt_kawase_up_fs_src));

    if(!g_crt_kawase.down_prog || !g_crt_kawase.up_prog) {
        LV_LOG_ERROR("CRT_KAWASE: shader init failed");
        return;
    }

    g_crt_kawase.down_loc_tex = glGetUniformLocation(g_crt_kawase.down_prog, "u_tex");
    g_crt_kawase.down_loc_half_px = glGetUniformLocation(g_crt_kawase.down_prog, "u_half_px");
    g_crt_kawase.up_loc_tex = glGetUniformLocation(g_crt_kawase.up_prog, "u_tex");
    g_crt_kawase.up_loc_half_px = glGetUniformLocation(g_crt_kawase.up_prog, "u_half_px");
    g_crt_kawase.up_loc_recolor = glGetUniformLocation(g_crt_kawase.up_prog, "u_recolor");
    g_crt_kawase.attr_pos = glGetAttribLocation(g_crt_kawase.down_prog, "a_pos");
    g_crt_kawase.attr_uv = glGetAttribLocation(g_crt_kawase.down_prog, "a_uv");

    glGenBuffers(1, &g_crt_kawase.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_crt_kawase.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(crt_kawase_quad), crt_kawase_quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g_crt_kawase.ready = true;
    LV_LOG_INFO("CRT_KAWASE blur ready");
}

void lv_evgpu_c_r_t_kawase_deinit(void)
{
    if(g_crt_kawase.capture_tex) glDeleteTextures(1, &g_crt_kawase.capture_tex);
    if(g_crt_kawase.vbo) glDeleteBuffers(1, &g_crt_kawase.vbo);
    if(g_crt_kawase.down_prog) glDeleteProgram(g_crt_kawase.down_prog);
    if(g_crt_kawase.up_prog) glDeleteProgram(g_crt_kawase.up_prog);
    lv_memzero(&g_crt_kawase, sizeof(g_crt_kawase));
}

bool lv_evgpu_c_r_t_kawase_ready(void)
{
    return g_crt_kawase.ready;
}

int lv_evgpu_c_r_t_kawase_blur_region(int dst_fb, int x, int y, int w, int h,
                                       int radius, uint8_t cr, uint8_t cg, uint8_t cb, uint8_t alpha)
{
    if(!g_crt_kawase.ready || w <= 0 || h <= 0 || radius <= 0) return -1;

    lv_evgpu_c_r_t_fbo_t * ping = lv_evgpu_c_r_t_fbo_create(w, h);
    lv_evgpu_c_r_t_fbo_t * pong = lv_evgpu_c_r_t_fbo_create(w, h);
    if(!ping || !pong) {
        if(ping) lv_evgpu_c_r_t_fbo_destroy(ping);
        if(pong) lv_evgpu_c_r_t_fbo_destroy(pong);
        return -1;
    }

    const int passes = calc_passes(radius);

    GLint prev_program, prev_fbo, prev_vbo, prev_tex, prev_active_tex;
    GLint prev_viewport[4];
    GLboolean prev_blend, prev_scissor, prev_depth, prev_stencil, prev_cull;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_tex);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    glGetIntegerv(GL_VIEWPORT, prev_viewport);
    prev_blend = glIsEnabled(GL_BLEND);
    prev_scissor = glIsEnabled(GL_SCISSOR_TEST);
    prev_depth = glIsEnabled(GL_DEPTH_TEST);
    prev_stencil = glIsEnabled(GL_STENCIL_TEST);
    prev_cull = glIsEnabled(GL_CULL_FACE);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    ensure_capture_tex(w, h);

    glBindTexture(GL_TEXTURE_2D, g_crt_kawase.capture_tex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, ping->fbo);
    draw_kawase_pass(g_crt_kawase.up_prog, g_crt_kawase.capture_tex, w, h,
                     0.5f / (float)w, 0.5f / (float)h,
                     0.0f, 0.0f, 0.0f, 0.0f, true);

    lv_evgpu_c_r_t_fbo_t * cur = ping;
    lv_evgpu_c_r_t_fbo_t * nxt = pong;
    int cur_w = w;
    int cur_h = h;

    for(int i = 0; i < passes; i++) {
        const int nw = LV_MAX(cur_w / 2, 1);
        const int nh = LV_MAX(cur_h / 2, 1);
        glBindFramebuffer(GL_FRAMEBUFFER, nxt->fbo);
        draw_kawase_pass(g_crt_kawase.down_prog, cur->tex, nw, nh,
                         0.5f / (float)cur_w, 0.5f / (float)cur_h,
                         0.0f, 0.0f, 0.0f, 0.0f, false);
        lv_evgpu_c_r_t_fbo_t * tmp = cur;
        cur = nxt;
        nxt = tmp;
        cur_w = nw;
        cur_h = nh;
    }

    const float recolor_r = cr / 255.0f;
    const float recolor_g = cg / 255.0f;
    const float recolor_b = cb / 255.0f;
    const float recolor_a = alpha > 0 ? 1.0f : 0.0f;

    for(int i = passes - 1; i >= 0; i--) {
        const int nw = LV_MIN(cur_w * 2, w);
        const int nh = LV_MIN(cur_h * 2, h);
        const bool last = (i == 0);
        if(last) {
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)dst_fb);
            glViewport(x, y, w, h);
        }
        else {
            glBindFramebuffer(GL_FRAMEBUFFER, nxt->fbo);
        }
        draw_kawase_pass(g_crt_kawase.up_prog, cur->tex, last ? w : nw, last ? h : nh,
                         0.5f / (float)cur_w, 0.5f / (float)cur_h,
                         recolor_r, recolor_g, recolor_b, recolor_a, true);
        if(!last) {
            lv_evgpu_c_r_t_fbo_t * tmp = cur;
            cur = nxt;
            nxt = tmp;
            cur_w = nw;
            cur_h = nh;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prev_vbo);
    glActiveTexture(prev_active_tex);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    glUseProgram((GLuint)prev_program);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    if(prev_blend) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    if(prev_scissor) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
    if(prev_depth) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    if(prev_stencil) glEnable(GL_STENCIL_TEST);
    else glDisable(GL_STENCIL_TEST);
    if(prev_cull) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    lv_evgpu_c_r_t_fbo_destroy(ping);
    lv_evgpu_c_r_t_fbo_destroy(pong);

    return 0;
}

#endif
