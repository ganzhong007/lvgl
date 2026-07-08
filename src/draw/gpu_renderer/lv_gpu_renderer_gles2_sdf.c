/**
 * @file lv_gpu_renderer_gles2_sdf.c — GPU SDF from A8 via jump-flood (GLES2)
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "lv_gpu_renderer_gles2_sdf.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../stdlib/lv_mem.h"
#include <math.h>
#include <string.h>

#define SDF_MAX_DIM 512

static unsigned int prog_seed;
static int loc_seed_disp;
static int loc_seed_sampler;

static unsigned int prog_inv;
static int loc_inv_sampler;

static unsigned int prog_jfa;
static int loc_jfa_disp;
static int loc_jfa_step;
static int loc_jfa_sampler;

static unsigned int prog_merge;
static int loc_merge_disp;
static int loc_merge_in_sampler;
static int loc_merge_out_sampler;

static const char * vs_sdf =
    "attribute vec2 a_pos;\n"
    "varying vec2 v_uv;\n"
    "void main(){ v_uv=a_pos; gl_Position=vec4(a_pos*2.0-1.0,0.0,1.0); }\n";

static const char * fs_seed =
    "precision mediump float;\n"
    "uniform vec2 u_disp;\n"
    "uniform sampler2D u_src;\n"
    "varying vec2 v_uv;\n"
    "void main(){\n"
    "  float a=texture2D(u_src,v_uv).a;\n"
    "  vec2 p=v_uv*u_disp;\n"
    "  if(a>0.5) gl_FragColor=vec4(p,1.0,1.0);\n"
    "  else gl_FragColor=vec4(-1.0,-1.0,0.0,0.0);\n"
    "}\n";

static const char * fs_inv =
    "precision mediump float;\n"
    "uniform sampler2D u_src;\n"
    "varying vec2 v_uv;\n"
    "void main(){ float a=texture2D(u_src,v_uv).a; gl_FragColor=vec4(0.0,0.0,0.0,1.0-a); }\n";

static const char * fs_jfa =
    "precision mediump float;\n"
    "uniform vec2 u_disp;\n"
    "uniform float u_step;\n"
    "uniform sampler2D u_src;\n"
    "varying vec2 v_uv;\n"
    "void main(){\n"
    "  vec2 p=v_uv*u_disp;\n"
    "  vec2 best=vec2(-1.0);\n"
    "  float bestd=1e20;\n"
    "  for(int j=-1;j<=1;j++){\n"
    "    for(int i=-1;i<=1;i++){\n"
    "      vec2 uv=v_uv+vec2(float(i),float(j))*u_step/u_disp;\n"
    "      vec4 s=texture2D(u_src,uv);\n"
    "      if(s.a>0.5){\n"
    "        float d=dot(p-s.xy,p-s.xy);\n"
    "        if(d<bestd){ bestd=d; best=s.xy; }\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "  if(best.x<0.0) gl_FragColor=vec4(-1.0,-1.0,0.0,0.0);\n"
    "  else gl_FragColor=vec4(best,1.0,1.0);\n"
    "}\n";

static const char * fs_merge =
    "precision mediump float;\n"
    "uniform vec2 u_disp;\n"
    "uniform sampler2D u_in;\n"
    "uniform sampler2D u_out;\n"
    "varying vec2 v_uv;\n"
    "void main(){\n"
    "  vec2 p=v_uv*u_disp;\n"
    "  vec4 si=texture2D(u_in,v_uv);\n"
    "  vec4 so=texture2D(u_out,v_uv);\n"
    "  float di=si.a>0.5?length(p-si.xy):32.0;\n"
    "  float dout=so.a>0.5?length(p-so.xy):32.0;\n"
    "  float sd=dout-di;\n"
    "  if(sd<-32.0) sd=-32.0; if(sd>32.0) sd=32.0;\n"
    "  float v=(128.0+sd*4.0)/255.0;\n"
    "  gl_FragColor=vec4(v,v,v,v);\n"
    "}\n";

static unsigned int compile_shader(unsigned int type, const char * src)
{
    unsigned int s = glCreateShader(type);
    GL_CALL(glShaderSource(s, 1, &src, NULL));
    GL_CALL(glCompileShader(s));
    int ok = 0;
    GL_CALL(glGetShaderiv(s, GL_COMPILE_STATUS, &ok));
    if(!ok) {
        GL_CALL(glDeleteShader(s));
        return 0;
    }
    return s;
}

static unsigned int link_program(unsigned int v, unsigned int f)
{
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
        GL_CALL(glDeleteProgram(p));
        return 0;
    }
    return p;
}

static unsigned int make_program(const char * fs)
{
    unsigned int v = compile_shader(GL_VERTEX_SHADER, vs_sdf);
    unsigned int f = compile_shader(GL_FRAGMENT_SHADER, fs);
    if(!v || !f) return 0;
    return link_program(v, f);
}

static void draw_fullscreen(void)
{
    const float quad[] = { 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f };
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
}

static unsigned int make_tex_rgba(int32_t w, int32_t h)
{
    unsigned int tex = 0;
    GL_CALL(glGenTextures(1, &tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    return tex;
}

static unsigned int make_tex_alpha(int32_t w, int32_t h)
{
    unsigned int tex = 0;
    GL_CALL(glGenTextures(1, &tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    return tex;
}

static unsigned int make_fbo(unsigned int tex)
{
    unsigned int fbo = 0;
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0));
#if !LV_USE_EGL
    GLenum db = GL_COLOR_ATTACHMENT0;
    GL_CALL(glDrawBuffers(1, &db));
#endif
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    return ok ? fbo : 0;
}

static void bind_tex(unsigned int prog, int loc_disp, int loc_sampler, unsigned int tex, int32_t w, int32_t h)
{
    GL_CALL(glUseProgram(prog));
    if(loc_disp >= 0) GL_CALL(glUniform2f(loc_disp, (float)w, (float)h));
    GL_CALL(glActiveTexture(GL_TEXTURE0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    if(loc_sampler >= 0) GL_CALL(glUniform1i(loc_sampler, 0));
}

static void upload_a8_padded(unsigned int tex, const uint8_t * bitmap, int32_t bw, int32_t bh,
                             int32_t stride, int32_t pad, int32_t tw, int32_t th)
{
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tw, th, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL));
    for(int32_t y = 0; y < bh; y++) {
        GL_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, pad, pad + y, bw, 1, GL_ALPHA, GL_UNSIGNED_BYTE,
                                bitmap + (uint32_t)y * (uint32_t)stride));
    }
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

static unsigned int jfa_from_mask(unsigned int mask_a8, int32_t w, int32_t h,
                                  unsigned int ping[2], unsigned int fbo[2])
{
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]));
    GL_CALL(glViewport(0, 0, w, h));
    GL_CALL(glDisable(GL_BLEND));
    bind_tex(prog_seed, loc_seed_disp, loc_seed_sampler, mask_a8, w, h);
    draw_fullscreen();

    int src = 0;
    int step = 1;
    while(step < (w > h ? w : h)) step <<= 1;
    step >>= 1;

    while(step >= 1) {
        const int dst = 1 - src;
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo[dst]));
        GL_CALL(glViewport(0, 0, w, h));
        bind_tex(prog_jfa, loc_jfa_disp, loc_jfa_sampler, ping[src], w, h);
        GL_CALL(glUniform1f(loc_jfa_step, (float)step));
        draw_fullscreen();
        src = dst;
        step >>= 1;
    }
    return ping[src];
}

void lv_gpu_renderer_gles2_sdf_init(void)
{
    if(prog_seed) return;
    prog_seed = make_program(fs_seed);
    prog_inv = make_program(fs_inv);
    prog_jfa = make_program(fs_jfa);
    prog_merge = make_program(fs_merge);
    if(!prog_seed || !prog_inv || !prog_jfa || !prog_merge) return;

    loc_seed_disp = glGetUniformLocation(prog_seed, "u_disp");
    loc_seed_sampler = glGetUniformLocation(prog_seed, "u_src");
    loc_inv_sampler = glGetUniformLocation(prog_inv, "u_src");
    loc_jfa_disp = glGetUniformLocation(prog_jfa, "u_disp");
    loc_jfa_step = glGetUniformLocation(prog_jfa, "u_step");
    loc_jfa_sampler = glGetUniformLocation(prog_jfa, "u_src");
    loc_merge_disp = glGetUniformLocation(prog_merge, "u_disp");
    loc_merge_in_sampler = glGetUniformLocation(prog_merge, "u_in");
    loc_merge_out_sampler = glGetUniformLocation(prog_merge, "u_out");
}

void lv_gpu_renderer_gles2_sdf_deinit(void)
{
    if(prog_seed) GL_CALL(glDeleteProgram(prog_seed));
    if(prog_inv) GL_CALL(glDeleteProgram(prog_inv));
    if(prog_jfa) GL_CALL(glDeleteProgram(prog_jfa));
    if(prog_merge) GL_CALL(glDeleteProgram(prog_merge));
    prog_seed = prog_inv = prog_jfa = prog_merge = 0;
}

bool lv_gpu_renderer_gles2_sdf_upload_atlas(unsigned int atlas_tex, int32_t dst_x, int32_t dst_y,
                                            const uint8_t * bitmap, int32_t bw, int32_t bh, int32_t stride,
                                            int32_t sdf_w, int32_t sdf_h)
{
    if(!bitmap || !atlas_tex || bw < 1 || bh < 1 || sdf_w < 1 || sdf_h < 1) return false;
    if(sdf_w > SDF_MAX_DIM || sdf_h > SDF_MAX_DIM) return false;

    lv_gpu_renderer_gles2_sdf_init();
    if(!prog_seed) return false;

    const int32_t pad = (sdf_w - bw) / 2;
    const int32_t tw = sdf_w;
    const int32_t th = sdf_h;

    unsigned int src_a8 = make_tex_alpha(tw, th);
    upload_a8_padded(src_a8, bitmap, bw, bh, stride, pad, tw, th);

    unsigned int ping[2] = { make_tex_rgba(tw, th), make_tex_rgba(tw, th) };
    unsigned int fbo[2] = { make_fbo(ping[0]), make_fbo(ping[1]) };
    if(!fbo[0] || !fbo[1]) goto cleanup;

    const unsigned int inside_field = jfa_from_mask(src_a8, tw, th, ping, fbo);

    unsigned int inv_a8 = make_tex_alpha(tw, th);
    unsigned int inv_fbo = make_fbo(inv_a8);
    if(inv_fbo) {
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, inv_fbo));
        GL_CALL(glViewport(0, 0, tw, th));
        GL_CALL(glDisable(GL_BLEND));
        bind_tex(prog_inv, -1, loc_inv_sampler, src_a8, tw, th);
        draw_fullscreen();
    }

    unsigned int ping2[2] = { make_tex_rgba(tw, th), make_tex_rgba(tw, th) };
    unsigned int fbo2[2] = { make_fbo(ping2[0]), make_fbo(ping2[1]) };
    const unsigned int outside_field = (inv_fbo && fbo2[0] && fbo2[1])
                                         ? jfa_from_mask(inv_a8, tw, th, ping2, fbo2)
                                         : inside_field;

    unsigned int sdf_rgba = make_tex_rgba(tw, th);
    unsigned int sdf_fbo = make_fbo(sdf_rgba);
    if(sdf_fbo) {
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, sdf_fbo));
        GL_CALL(glViewport(0, 0, tw, th));
        GL_CALL(glDisable(GL_BLEND));
        GL_CALL(glUseProgram(prog_merge));
        GL_CALL(glUniform2f(loc_merge_disp, (float)tw, (float)th));
        GL_CALL(glActiveTexture(GL_TEXTURE0));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, inside_field));
        GL_CALL(glUniform1i(loc_merge_in_sampler, 0));
        GL_CALL(glActiveTexture(GL_TEXTURE1));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, outside_field));
        GL_CALL(glUniform1i(loc_merge_out_sampler, 1));
        draw_fullscreen();
    }

    bool ok = false;
    if(sdf_fbo) {
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, sdf_fbo));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, atlas_tex));
        GL_CALL(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, dst_x, dst_y, 0, 0, tw, th));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        ok = true;
    }

cleanup:
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    if(src_a8) GL_CALL(glDeleteTextures(1, &src_a8));
    if(inv_a8) GL_CALL(glDeleteTextures(1, &inv_a8));
    if(sdf_rgba) GL_CALL(glDeleteTextures(1, &sdf_rgba));
    if(ping[0]) GL_CALL(glDeleteTextures(1, &ping[0]));
    if(ping[1]) GL_CALL(glDeleteTextures(1, &ping[1]));
    if(ping2[0]) GL_CALL(glDeleteTextures(1, &ping2[0]));
    if(ping2[1]) GL_CALL(glDeleteTextures(1, &ping2[1]));
    if(fbo[0]) GL_CALL(glDeleteFramebuffers(1, &fbo[0]));
    if(fbo[1]) GL_CALL(glDeleteFramebuffers(1, &fbo[1]));
    if(fbo2[0]) GL_CALL(glDeleteFramebuffers(1, &fbo2[0]));
    if(fbo2[1]) GL_CALL(glDeleteFramebuffers(1, &fbo2[1]));
    if(sdf_fbo) GL_CALL(glDeleteFramebuffers(1, &sdf_fbo));
    if(inv_fbo) GL_CALL(glDeleteFramebuffers(1, &inv_fbo));
    return ok;
}

#endif /*LV_USE_DRAW_GPU_RENDERER*/
