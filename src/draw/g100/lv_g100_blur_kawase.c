/**
 * @file lv_g100_blur_kawase.c
 */

#include "lv_g100_blur_kawase.h"

#if LV_USE_DRAW_G100

#include "lv_draw_g100_private.h"
#include "../../libs/nanovg/nanovg_gl_utils.h"

#if LV_USE_OPENGLES && LV_USE_EGL
    #include "../../drivers/opengles/lv_opengles_private.h"
    #define LV_G100_KAWASE_HAS_GL 1
#else
    #define LV_G100_KAWASE_HAS_GL 0
#endif

#define G100_KAWASE_MAX_PASSES 6

typedef struct {
    NVGcontext * vg;
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
    NVGLUframebuffer * ping;
    NVGLUframebuffer * pong;
    int fb_w;
    int fb_h;
    GLuint capture_tex;
    int capture_w;
    int capture_h;
    bool ready;
} lv_g100_blur_kawase_state_t;

static lv_g100_blur_kawase_state_t g_kawase;

#if LV_G100_KAWASE_HAS_GL

static const char g100_kawase_vs_src[] =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main(void) {\n"
    "  v_uv = a_uv;\n"
    "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char g100_kawase_down_fs_src[] =
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

static const char g100_kawase_up_fs_src[] =
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

static const float g100_kawase_quad[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
};

static GLuint compile_shader(GLenum type, const char * src)
{
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

static GLuint link_program(GLuint vs, GLuint fs)
{
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

static void ensure_fbs(NVGcontext * ctx, int w, int h)
{
    if(g_kawase.ping && g_kawase.pong && g_kawase.fb_w >= w && g_kawase.fb_h >= h) return;

    if(g_kawase.ping) {
        nvgluDeleteFramebuffer(g_kawase.ping);
        g_kawase.ping = NULL;
    }
    if(g_kawase.pong) {
        nvgluDeleteFramebuffer(g_kawase.pong);
        g_kawase.pong = NULL;
    }

    g_kawase.ping = nvgluCreateFramebuffer(ctx, w, h, 0, NVG_TEXTURE_RGBA);
    g_kawase.pong = nvgluCreateFramebuffer(ctx, w, h, 0, NVG_TEXTURE_RGBA);
    g_kawase.fb_w = g_kawase.ping ? w : 0;
    g_kawase.fb_h = g_kawase.pong ? h : 0;
}

static void ensure_capture(int w, int h)
{
    if(g_kawase.capture_tex && g_kawase.capture_w == w && g_kawase.capture_h == h) return;

    if(!g_kawase.capture_tex) glGenTextures(1, &g_kawase.capture_tex);
    glBindTexture(GL_TEXTURE_2D, g_kawase.capture_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_kawase.capture_w = w;
    g_kawase.capture_h = h;
}

static void bind_quad_attrs(void)
{
    glBindBuffer(GL_ARRAY_BUFFER, g_kawase.vbo);
    glEnableVertexAttribArray((GLuint)g_kawase.attr_pos);
    glVertexAttribPointer((GLuint)g_kawase.attr_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (const void *)0);
    glEnableVertexAttribArray((GLuint)g_kawase.attr_uv);
    glVertexAttribPointer((GLuint)g_kawase.attr_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                          (const void *)(sizeof(float) * 2));
}

static void draw_kawase_pass(GLuint prog, GLuint src_tex, int dst_w, int dst_h, float half_px_x, float half_px_y,
                             const NVGcolor * recolor, bool is_up)
{
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glUniform1i(glGetUniformLocation(prog, "u_tex"), 0);
    glUniform2f(is_up ? g_kawase.up_loc_half_px : g_kawase.down_loc_half_px, half_px_x, half_px_y);
    if(is_up && g_kawase.up_loc_recolor >= 0 && recolor) {
        glUniform4f(g_kawase.up_loc_recolor, recolor->ch.r / 255.0f, recolor->ch.g / 255.0f,
                      recolor->ch.b / 255.0f, recolor->ch.a > 0 ? 1.0f : 0.0f);
    }
    else if(is_up && g_kawase.up_loc_recolor >= 0) {
        glUniform4f(g_kawase.up_loc_recolor, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    glViewport(0, 0, dst_w, dst_h);
    bind_quad_attrs();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static int calc_passes(int radius)
{
    int passes = 0;
    int r = radius;
    while(r > 32 && passes < G100_KAWASE_MAX_PASSES) {
        passes++;
        r = (r + 1) / 2;
    }
    return passes > 0 ? passes : 1;
}

#endif /*LV_G100_KAWASE_HAS_GL*/

void lv_g100_blur_kawase_init(lv_draw_g100_unit_t * unit)
{
    lv_memzero(&g_kawase, sizeof(g_kawase));

#if LV_G100_KAWASE_HAS_GL
    if(!unit || !unit->vg) return;

    g_kawase.vg = unit->vg;

    GLuint vs = compile_shader(GL_VERTEX_SHADER, g100_kawase_vs_src);
    g_kawase.down_prog = link_program(vs, compile_shader(GL_FRAGMENT_SHADER, g100_kawase_down_fs_src));
    vs = compile_shader(GL_VERTEX_SHADER, g100_kawase_vs_src);
    g_kawase.up_prog = link_program(vs, compile_shader(GL_FRAGMENT_SHADER, g100_kawase_up_fs_src));

    if(!g_kawase.down_prog || !g_kawase.up_prog) {
        LV_LOG_WARN("G100 kawase blur: shader init failed");
        return;
    }

    g_kawase.down_loc_tex = glGetUniformLocation(g_kawase.down_prog, "u_tex");
    g_kawase.down_loc_half_px = glGetUniformLocation(g_kawase.down_prog, "u_half_px");
    g_kawase.up_loc_tex = glGetUniformLocation(g_kawase.up_prog, "u_tex");
    g_kawase.up_loc_half_px = glGetUniformLocation(g_kawase.up_prog, "u_half_px");
    g_kawase.up_loc_recolor = glGetUniformLocation(g_kawase.up_prog, "u_recolor");
    g_kawase.attr_pos = glGetAttribLocation(g_kawase.down_prog, "a_pos");
    g_kawase.attr_uv = glGetAttribLocation(g_kawase.down_prog, "a_uv");

    glGenBuffers(1, &g_kawase.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_kawase.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g100_kawase_quad), g100_kawase_quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g_kawase.ready = true;
    LV_LOG_INFO("G100 kawase blur ready");
#else
    LV_UNUSED(unit);
#endif
}

void lv_g100_blur_kawase_deinit(lv_draw_g100_unit_t * unit)
{
    LV_UNUSED(unit);

#if LV_G100_KAWASE_HAS_GL
    if(g_kawase.ping) nvgluDeleteFramebuffer(g_kawase.ping);
    if(g_kawase.pong) nvgluDeleteFramebuffer(g_kawase.pong);
    if(g_kawase.capture_tex) glDeleteTextures(1, &g_kawase.capture_tex);
    if(g_kawase.vbo) glDeleteBuffers(1, &g_kawase.vbo);
    if(g_kawase.down_prog) glDeleteProgram(g_kawase.down_prog);
    if(g_kawase.up_prog) glDeleteProgram(g_kawase.up_prog);
#endif

    lv_memzero(&g_kawase, sizeof(g_kawase));
}

bool lv_g100_blur_kawase_ready(void)
{
    return g_kawase.ready;
}

int lv_g100_blur_kawase_region(lv_draw_g100_unit_t * unit, NVGLUframebuffer * fb,
                               int x, int y, int w, int h, int radius, const NVGcolor * recolor)
{
#if !LV_G100_KAWASE_HAS_GL
    LV_UNUSED(unit);
    LV_UNUSED(fb);
    LV_UNUSED(x);
    LV_UNUSED(y);
    LV_UNUSED(w);
    LV_UNUSED(h);
    LV_UNUSED(radius);
    LV_UNUSED(recolor);
    return -1;
#else
    if(!g_kawase.ready || !unit || !unit->vg || w <= 0 || h <= 0 || radius <= 0) return -1;

    ensure_fbs(unit->vg, w, h);
    if(!g_kawase.ping || !g_kawase.pong) return -1;

    const bool is_root = (fb == NULL);
    const int passes = calc_passes(radius);

    GLint prev_program = 0, prev_fbo = 0, prev_vbo = 0, prev_tex = 0;
    GLint prev_active_tex = 0;
    GLint prev_viewport[4] = {0};
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

    ensure_capture(w, h);

    if(is_root) {
        glBindTexture(GL_TEXTURE_2D, g_kawase.capture_tex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, w, h);
    }
    else {
        nvgluBindFramebuffer(fb);
        glBindTexture(GL_TEXTURE_2D, g_kawase.capture_tex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, w, h);
    }

    nvgluBindFramebuffer(g_kawase.ping);
    draw_kawase_pass(g_kawase.up_prog, g_kawase.capture_tex, w, h,
                     0.5f / (float)w, 0.5f / (float)h, NULL, true);

    NVGLUframebuffer * cur = g_kawase.ping;
    NVGLUframebuffer * nxt = g_kawase.pong;
    int cur_w = w;
    int cur_h = h;

    for(int i = 0; i < passes; i++) {
        const int nw = LV_MAX(cur_w / 2, 1);
        const int nh = LV_MAX(cur_h / 2, 1);
        nvgluBindFramebuffer(nxt);
        draw_kawase_pass(g_kawase.down_prog, nvgluFramebufferGetTexture(cur), nw, nh,
                         0.5f / (float)cur_w, 0.5f / (float)cur_h, NULL, false);
        NVGLUframebuffer * tmp = cur;
        cur = nxt;
        nxt = tmp;
        cur_w = nw;
        cur_h = nh;
    }

    for(int i = passes - 1; i >= 0; i--) {
        const int nw = LV_MIN(cur_w * 2, w);
        const int nh = LV_MIN(cur_h * 2, h);
        const bool last = (i == 0);
        if(last) {
            if(is_root) nvgluBindFramebuffer(NULL);
            else nvgluBindFramebuffer(fb);
            glViewport(x, y, w, h);
        }
        else {
            nvgluBindFramebuffer(nxt);
        }
        draw_kawase_pass(g_kawase.up_prog, nvgluFramebufferGetTexture(cur), last ? w : nw, last ? h : nh,
                         0.5f / (float)cur_w, 0.5f / (float)cur_h, last ? recolor : NULL, true);
        if(!last) {
            NVGLUframebuffer * tmp = cur;
            cur = nxt;
            nxt = tmp;
            cur_w = nw;
            cur_h = nh;
        }
    }

    static bool applied_logged;
    if(!applied_logged) {
        applied_logged = true;
        LV_LOG_INFO("G100 kawase blur applied (radius=%d, passes=%d)", radius, passes);
    }

    glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    glActiveTexture(prev_active_tex);
    glBindTexture(GL_TEXTURE_2D, prev_tex);
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

    return 0;
#endif
}

#endif /*LV_USE_DRAW_G100*/
