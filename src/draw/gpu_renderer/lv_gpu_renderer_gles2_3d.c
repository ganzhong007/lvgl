/**
 * @file lv_gpu_renderer_gles2_3d.c — GLES2 wireframe box renderer [GL2]
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER && LV_USE_3D

#include "lv_gpu_renderer_gles2_3d.h"

#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../include/lvgl/3d/lv_3d_plane_bake.h"
#include "../../misc/lv_color.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef LV_GPU_RENDERER_MSAA_SAMPLES
    #define LV_GPU_RENDERER_MSAA_SAMPLES 0
#endif

#if !LV_USE_EGL
static unsigned int g_msaa_fbo;
static unsigned int g_msaa_color_rb;
static int32_t g_msaa_w;
static int32_t g_msaa_h;
static int g_msaa_cached_samples;
#endif

static int msaa_samples_effective(void)
{
    static int cached = -1;
    if(cached >= 0) return cached;

    int s = LV_GPU_RENDERER_MSAA_SAMPLES;
    const char * env = getenv("LVGL_MSAA_SAMPLES");
    if(env && env[0]) s = atoi(env);
    if(s < 0) s = 0;
    if(s > 8) s = 8;
    if(s == 1) s = 0;
    cached = s;
    return cached;
}

#if !LV_USE_EGL
static void msaa_fbo_release(void)
{
    if(g_msaa_color_rb) {
        GL_CALL(glDeleteRenderbuffers(1, &g_msaa_color_rb));
        g_msaa_color_rb = 0;
    }
    if(g_msaa_fbo) {
        GL_CALL(glDeleteFramebuffers(1, &g_msaa_fbo));
        g_msaa_fbo = 0;
    }
    g_msaa_w = 0;
    g_msaa_h = 0;
    g_msaa_cached_samples = 0;
}

static bool msaa_fbo_ensure(int32_t w, int32_t h, int samples)
{
    if(samples < 2 || w < 1 || h < 1) return false;

    if(g_msaa_fbo && g_msaa_color_rb && g_msaa_w == w && g_msaa_h == h && g_msaa_cached_samples == samples) {
        return true;
    }

    msaa_fbo_release();

    GL_CALL(glGenFramebuffers(1, &g_msaa_fbo));
    GL_CALL(glGenRenderbuffers(1, &g_msaa_color_rb));
    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, g_msaa_color_rb));
    GL_CALL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, w, h));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, g_msaa_fbo));
    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, g_msaa_color_rb));

    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    if(!ok) {
        LV_LOG_WARN("LVGL MSAA FBO incomplete (samples=%d)", samples);
        msaa_fbo_release();
        return false;
    }

    g_msaa_w = w;
    g_msaa_h = h;
    g_msaa_cached_samples = samples;
    return true;
}

static void msaa_resolve_to_texture(unsigned int color_tex, int32_t dst_x, int32_t dst_y, int32_t w, int32_t h)
{
    unsigned int resolve_fbo = 0;
    GL_CALL(glGenFramebuffers(1, &resolve_fbo));
    GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_fbo));
    GL_CALL(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0));
    GL_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, g_msaa_fbo));
    GL_CALL(glBlitFramebuffer(0, 0, w, h, dst_x, dst_y, dst_x + w, dst_y + h, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &resolve_fbo));
}
#endif

static unsigned int prog;
static int loc_mvp;
static int loc_color;

static unsigned int prog_tex;
static int loc_tex_mvp;
static int loc_tex_sampler;
static int loc_tex_opa;

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

#if LV_USE_SNAPSHOT
#if LV_USE_EGL
static const char * vs_tex_src =
    "attribute vec3 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "uniform mat4 u_mvp;\n"
    "void main() { v_uv = a_uv; gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char * fs_tex_src =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_opa;\n"
    "void main() { vec4 c = texture2D(u_tex, v_uv); gl_FragColor = vec4(c.rgb, c.a * u_opa); }\n";
#else
static const char * vs_tex_src =
    "#version 120\n"
    "attribute vec3 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "uniform mat4 u_mvp;\n"
    "void main() { v_uv = a_uv; gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char * fs_tex_src =
    "#version 120\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_opa;\n"
    "void main() { vec4 c = texture2D(u_tex, v_uv); gl_FragColor = vec4(c.rgb, c.a * u_opa); }\n";
#endif
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

#if LV_USE_SNAPSHOT
static unsigned int link_program_tex(const char * vs, const char * fs)
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
    GL_CALL(glBindAttribLocation(p, 1, "a_uv"));
    GL_CALL(glLinkProgram(p));
    GL_CALL(glDeleteShader(v));
    GL_CALL(glDeleteShader(f));

    int ok = 0;
    GL_CALL(glGetProgramiv(p, GL_LINK_STATUS, &ok));
    if(!ok) {
        char log[512];
        log[0] = '\0';
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        LV_LOG_ERROR("LVGL tex shader link failed: %s", log);
        GL_CALL(glDeleteProgram(p));
        return 0;
    }
    return p;
}
#endif

void lv_gpu_renderer_gles2_3d_init(void)
{
    if(prog) return;
    prog = link_program(vs_src, fs_src);
    if(!prog) return;
    loc_mvp = glGetUniformLocation(prog, "u_mvp");
    loc_color = glGetUniformLocation(prog, "u_color");

#if LV_USE_SNAPSHOT
    prog_tex = link_program_tex(vs_tex_src, fs_tex_src);
    if(prog_tex) {
        loc_tex_mvp = glGetUniformLocation(prog_tex, "u_mvp");
        loc_tex_sampler = glGetUniformLocation(prog_tex, "u_tex");
        loc_tex_opa = glGetUniformLocation(prog_tex, "u_opa");
    }
#endif

    const int msaa = msaa_samples_effective();
    if(msaa > 1) {
        LV_LOG_USER("LVGL 3D MSAA enabled: %d samples (LVGL_MSAA_SAMPLES overrides)", msaa);
    }
}

void lv_gpu_renderer_gles2_3d_deinit(void)
{
    if(prog) {
        GL_CALL(glDeleteProgram(prog));
        prog = 0;
    }
#if LV_USE_SNAPSHOT
    if(prog_tex) {
        GL_CALL(glDeleteProgram(prog_tex));
        prog_tex = 0;
    }
#endif
#if !LV_USE_EGL
    msaa_fbo_release();
#endif
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

static void draw_plane_snapshot(const lv_3d_draw_item_t * it, const float view[16], const float proj[16])
{
#if LV_USE_SNAPSHOT
    if(!prog_tex) return;
    if(it->material.opa <= LV_OPA_MIN) return;

    unsigned int tex = lv_3d_plane_get_gl_texture(it->snapshot_id);
    if(tex == 0) return;

    float mvp[16], mv[16];
    lv_3d_mat4_mul(mv, view, it->transform.world);
    lv_3d_mat4_mul(mvp, proj, mv);

    float hx = it->w * 0.5f, hy = it->h * 0.5f, hz = it->d * 0.5f;
    const float pos[] = {
        -hx, -hy, hz,   hx, -hy, hz,   hx, hy, hz,
        -hx, -hy, hz,   hx, hy, hz,   -hx, hy, hz,
    };
    const float uv[] = {
        0.0f, 0.0f,   1.0f, 0.0f,   1.0f, 1.0f,
        0.0f, 0.0f,   1.0f, 1.0f,   0.0f, 1.0f,
    };

    GL_CALL(glUseProgram(prog_tex));
    GL_CALL(glUniformMatrix4fv(loc_tex_mvp, 1, GL_FALSE, mvp));
    GL_CALL(glUniform1f(loc_tex_opa, (float)it->material.opa / 255.0f));
    GL_CALL(glActiveTexture(GL_TEXTURE0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glUniform1i(loc_tex_sampler, 0));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glEnableVertexAttribArray(1));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, pos));
    GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(1));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
#else
    LV_UNUSED(it);
    LV_UNUSED(view);
    LV_UNUSED(proj);
#endif
}

static void gpu_comp_uniform_rgba(int loc, lv_color32_t c32)
{
    /* window_display_texture is presented with rb_swap; match SW overlay channel order */
    GL_CALL(glUniform4f(loc, c32.blue / 255.0f, c32.green / 255.0f, c32.red / 255.0f,
                        c32.alpha / 255.0f));
}

static void draw_box_triangles(const float verts[108], int vert_offset, int vert_count,
                               const float mvp[16], lv_color_t color, lv_opa_t opa)
{
    lv_color32_t c32 = lv_color_to_32(color, opa);
    GL_CALL(glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, mvp));
    gpu_comp_uniform_rgba(loc_color, c32);
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, &verts[vert_offset * 3]));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, vert_count));
    GL_CALL(glDisableVertexAttribArray(0));
}

static float face_avg_ndc_y(const float verts[108], int face_idx, const float mvp[16])
{
    float sum = 0.0f;
    int count = 0;
    for(int v = 0; v < 6; v++) {
        const int o = (face_idx * 6 + v) * 3;
        const float x = verts[o + 0];
        const float y = verts[o + 1];
        const float z = verts[o + 2];
        const float cx = mvp[0] * x + mvp[4] * y + mvp[8] * z + mvp[12];
        const float cy = mvp[1] * x + mvp[5] * y + mvp[9] * z + mvp[13];
        const float cw = mvp[3] * x + mvp[7] * y + mvp[11] * z + mvp[15];
        if(fabsf(cw) > 1e-6f) {
            sum += cy / cw;
            count++;
        }
    }
    return count > 0 ? (sum / (float)count) : 0.0f;
}

static void draw_shaded_box(const lv_3d_draw_item_t * it, const float mvp[16])
{
    float verts[108];
    box_solid_verts(it->w, it->h, it->d, verts);

    /* FBO y grows upward, but the window composite flips texture V; pick the cap that
     * lands on screen-top after that flip (smaller NDC y in the 3D pass). */
    int top_face = 5;
    float top_ndc_y = 1e30f;
    for(int f = 4; f <= 5; f++) {
        const float ay = face_avg_ndc_y(verts, f, mvp);
        if(ay < top_ndc_y) {
            top_ndc_y = ay;
            top_face = f;
        }
    }

    for(int f = 0; f < 4; f++) {
        draw_box_triangles(verts, f * 6, 6, mvp, it->material.color, it->material.opa);
    }
    for(int f = 4; f <= 5; f++) {
        const lv_color_t c = (f == top_face) ? it->material.top_color : it->material.color;
        draw_box_triangles(verts, f * 6, 6, mvp, c, it->material.opa);
    }
}

static bool draw_item_is_transparent(const lv_3d_draw_item_t * it)
{
    if(it->wireframe || it->material.kind == LV_3D_MAT_WIREFRAME) return true;
    if(it->material.kind == LV_3D_MAT_ALPHA) return true;
    if(it->material.kind == LV_3D_MAT_PLANE_SNAPSHOT && it->snapshot_id != LV_3D_SNAPSHOT_ID_NONE) return true;
    return false;
}

static float draw_item_view_depth(const lv_3d_draw_item_t * it, const float view[16])
{
    float mv[16];
    lv_3d_mat4_mul(mv, view, it->transform.world);
    return mv[2] * 0.0f + mv[6] * 0.0f + mv[10] * 0.0f + mv[14] * 1.0f;
}

static void draw_item(const lv_3d_draw_item_t * it, const float view[16], const float proj[16])
{
    if(it->material.opa <= LV_OPA_MIN) return;

    if(it->material.kind == LV_3D_MAT_PLANE_SNAPSHOT && it->snapshot_id != LV_3D_SNAPSHOT_ID_NONE) {
        draw_plane_snapshot(it, view, proj);
        return;
    }

    float mvp[16], mv[16];
    lv_3d_mat4_mul(mv, view, it->transform.world);
    lv_3d_mat4_mul(mvp, proj, mv);
    GL_CALL(glUseProgram(prog));

    if(it->material.kind == LV_3D_MAT_SHADED_BOX) {
        draw_shaded_box(it, mvp);
        return;
    }

    lv_color32_t c32 = lv_color_to_32(it->material.color, it->material.opa);
    GL_CALL(glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, mvp));
    gpu_comp_uniform_rgba(loc_color, c32);

    float verts[108];
    if(it->wireframe || it->material.kind == LV_3D_MAT_WIREFRAME) {
        box_solid_verts(it->w, it->h, it->d, verts);
        GL_CALL(glEnableVertexAttribArray(0));
        GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, verts));
        lv_color32_t dim = c32;
        dim.alpha = (uint8_t)(255.0f * 0.08f);
        gpu_comp_uniform_rgba(loc_color, dim);
        GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 36));
        box_wire_verts(it->w, it->h, it->d, verts);
        gpu_comp_uniform_rgba(loc_color, c32);
        GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, verts));
        GL_CALL(glLineWidth(2.0f));
#if !LV_USE_EGL
        GL_CALL(glEnable(GL_LINE_SMOOTH));
        GL_CALL(glHint(GL_LINE_SMOOTH_HINT, GL_NICEST));
#endif
        GL_CALL(glDrawArrays(GL_LINES, 0, 24));
#if !LV_USE_EGL
        GL_CALL(glDisable(GL_LINE_SMOOTH));
#endif
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

static void render_viewport_draw(unsigned int fbo, int32_t vp_x, int32_t vp_y, int32_t w, int32_t h,
                                 const float view[16], const float proj[16],
                                 const lv_3d_draw_item_t * items, uint32_t item_count,
                                 bool ar_passthrough)
{
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glViewport(vp_x, vp_y, w, h));
    GL_CALL(glEnable(GL_SCISSOR_TEST));
    GL_CALL(glScissor(vp_x, vp_y, w, h));

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
#if !LV_USE_EGL
    if(msaa_samples_effective() > 1) {
        GL_CALL(glEnable(GL_MULTISAMPLE));
    }
#endif

    uint32_t opaque_order[LV_3D_MAX_DRAW_ITEMS];
    uint32_t opaque_count = 0;
    for(uint32_t i = 0; i < item_count; i++) {
        if(draw_item_is_transparent(&items[i])) continue;
        opaque_order[opaque_count++] = i;
    }
    for(uint32_t a = 1; a < opaque_count; a++) {
        uint32_t key = opaque_order[a];
        float key_z = draw_item_view_depth(&items[key], view);
        uint32_t b = a;
        while(b > 0) {
            uint32_t prev = opaque_order[b - 1];
            if(draw_item_view_depth(&items[prev], view) <= key_z) break;
            opaque_order[b] = prev;
            b--;
        }
        opaque_order[b] = key;
    }

    GL_CALL(glUseProgram(prog));
    for(uint32_t o = 0; o < opaque_count; o++) {
        draw_item(&items[opaque_order[o]], view, proj);
    }
    for(uint32_t i = 0; i < item_count; i++) {
        if(!draw_item_is_transparent(&items[i])) continue;
        draw_item(&items[i], view, proj);
    }
    GL_CALL(glDisable(GL_SCISSOR_TEST));
}

static void render_viewport_probe_alpha(unsigned int fbo, int32_t x, int32_t y, int32_t w, int32_t h,
                                        uint8_t * max_alpha_out)
{
    if(!max_alpha_out) return;
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
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

void lv_gpu_renderer_gles2_render_viewport(unsigned int color_tex, unsigned int depth_rb,
                                            int32_t x, int32_t y, int32_t w, int32_t h,
                                            const float view[16], const float proj[16],
                                            const lv_3d_draw_item_t * items, uint32_t item_count,
                                            bool ar_passthrough, uint8_t * max_alpha_out)
{
    LV_UNUSED(depth_rb);
    if(max_alpha_out) *max_alpha_out = 0;
    if(!prog) lv_gpu_renderer_gles2_3d_init();
    if(!prog) return;

    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    const int msaa = msaa_samples_effective();
#if !LV_USE_EGL
    if(msaa > 1 && msaa_fbo_ensure(w, h, msaa)) {
        render_viewport_draw(g_msaa_fbo, 0, 0, w, h, view, proj, items, item_count, ar_passthrough);
        msaa_resolve_to_texture(color_tex, x, y, w, h);

        unsigned int probe_fbo = 0;
        GL_CALL(glGenFramebuffers(1, &probe_fbo));
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, probe_fbo));
        GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0));
        render_viewport_probe_alpha(probe_fbo, x, y, w, h, max_alpha_out);
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDeleteFramebuffers(1, &probe_fbo));
        return;
    }
#endif

    unsigned int fbo = 0;
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0));

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

    render_viewport_draw(fbo, x, y, w, h, view, proj, items, item_count, ar_passthrough);
    render_viewport_probe_alpha(fbo, x, y, w, h, max_alpha_out);

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &fbo));
}

#endif /*LV_USE_DRAW_GPU_RENDERER && LV_USE_3D*/
