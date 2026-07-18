#include "lv_evgpu_c_r_t_gl.h"

#if LV_USE_DRAW_EVGPU_C_R_T

#include "../../misc/lv_port_layer_trace.h"
#include <string.h>

static const char * solid_vs =
    "uniform mat4 u_proj;\n"
    "attribute vec2 a_pos;\n"
    "void main() {\n"
    "  gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);\n"
    "}";

static const char * solid_fs =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform float u_alpha;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(u_color.rgb, u_color.a * u_alpha);\n"
    "}";

static const char * tex_vs =
    "uniform mat4 u_proj;\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_tex;\n"
    "varying vec2 v_tex;\n"
    "void main() {\n"
    "  v_tex = a_tex;\n"
    "  gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);\n"
    "}";

static const char * tex_fs =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec4 u_recolor;\n"
    "uniform float u_recolor_opa;\n"
    "uniform float u_alpha;\n"
    "varying vec2 v_tex;\n"
    "void main() {\n"
    "  vec4 texel = texture2D(u_texture, v_tex);\n"
    "  vec4 recolored = mix(texel, vec4(u_recolor.rgb, texel.a), u_recolor_opa);\n"
    "  gl_FragColor = recolored * u_alpha;\n"
    "}";

static const char * blur_vs =
    "uniform mat4 u_proj;\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_tex;\n"
    "varying vec2 v_tex;\n"
    "void main() {\n"
    "  v_tex = a_tex;\n"
    "  gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);\n"
    "}";

static const char * blur_fs =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_dir;\n"
    "uniform float u_radius;\n"
    "uniform vec2 u_size;\n"
    "varying vec2 v_tex;\n"
    "void main() {\n"
    "  vec4 color = vec4(0.0);\n"
    "  float step = 1.0;\n"
    "  float total = 0.0;\n"
    "  for(float i = -u_radius; i <= u_radius; i += step) {\n"
    "    float weight = exp(-(i * i) / (2.0 * u_radius * u_radius * 0.25));\n"
    "    vec2 offset = u_dir * i / u_size;\n"
    "    color += texture2D(u_texture, v_tex + offset) * weight;\n"
    "    total += weight;\n"
    "  }\n"
    "  gl_FragColor = color / total;\n"
    "}";

static const char * grad_vs =
    "uniform mat4 u_proj;\n"
    "attribute vec2 a_pos;\n"
    "varying vec2 v_pos;\n"
    "void main() {\n"
    "  v_pos = a_pos;\n"
    "  gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);\n"
    "}";

static const char * grad_fs =
    "precision mediump float;\n"
    "uniform vec4 u_color1;\n"
    "uniform vec4 u_color2;\n"
    "uniform int u_dir;\n"
    "varying vec2 v_pos;\n"
    "void main() {\n"
    "  float t;\n"
    "  if(u_dir == 0) t = v_pos.x;\n"
    "  else t = v_pos.y;\n"
    "  t = fract(t * 0.01);\n"
    "  gl_FragColor = mix(u_color1, u_color2, t);\n"
    "}";

static const char * grad_tex_vs =
    "uniform mat4 u_proj;\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_tex;\n"
    "varying vec2 v_tex;\n"
    "void main() {\n"
    "  v_tex = a_tex;\n"
    "  gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);\n"
    "}";

static const char * grad_tex_fs =
    "precision mediump float;\n"
    "uniform sampler2D u_grad_tex;\n"
    "uniform int u_dir;\n"
    "varying vec2 v_tex;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(u_grad_tex, vec2(v_tex.x, 0.5));\n"
    "}";

static GLuint compile_shader(GLenum type, const char * src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LV_LOG_ERROR("shader compile: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        LV_LOG_ERROR("program link: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void use_program(lv_evgpu_c_r_t_gl_t * gl, GLuint prog) {
    if(gl->state.prog != prog) {
        glUseProgram(prog);
        gl->state.prog = prog;
    }
}

static void bind_texture(lv_evgpu_c_r_t_gl_t * gl, GLuint tex) {
    if(gl->state.tex != tex) {
        glBindTexture(GL_TEXTURE_2D, tex);
        gl->state.tex = tex;
    }
}

static void disable_scissor_cached(lv_evgpu_c_r_t_gl_t * gl) {
    if(gl->state.scissor_enabled) {
        glDisable(GL_SCISSOR_TEST);
        gl->state.scissor_enabled = false;
    }
}

static void * arena_alloc(lv_evgpu_c_r_t_gl_t * gl, size_t sz) {
    if(gl->arena.off + sz > gl->arena.cap) {
        size_t new_cap = gl->arena.cap * 2;
        if(new_cap < gl->arena.off + sz) new_cap = gl->arena.off + sz;
        gl->arena.buf = lv_realloc(gl->arena.buf, new_cap);
        gl->arena.cap = new_cap;
    }
    void * ptr = gl->arena.buf + gl->arena.off;
    gl->arena.off += sz;
    return ptr;
}

static void arena_reset(lv_evgpu_c_r_t_gl_t * gl) {
    gl->arena.off = 0;
}

static void set_proj_uniform(lv_evgpu_c_r_t_gl_t * gl, GLint u_proj) {
    glUniformMatrix4fv(u_proj, 1, GL_FALSE, gl->proj);
}

static void batch_flush(lv_evgpu_c_r_t_gl_t * gl) {
    if(!gl->batch.active || gl->batch.quad_count == 0) return;

    use_program(gl, gl->solid_prog);

    float r = ((gl->batch.color >> 16) & 0xFF) / 255.0f;
    float g = ((gl->batch.color >> 8) & 0xFF) / 255.0f;
    float b = ((gl->batch.color >> 0) & 0xFF) / 255.0f;
    float a = ((gl->batch.color >> 24) & 0xFF) / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    glUniform1f(gl->solid_u_alpha, gl->batch.alpha / 255.0f);
    set_proj_uniform(gl, gl->solid_u_proj);

    int total_verts = gl->batch.quad_count * 6;
    int stride = 2 * (int)sizeof(float);
    void * ptr = gl->arena.buf;

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, ptr);
    glDrawArrays(GL_TRIANGLES, 0, total_verts);
    glDisableVertexAttribArray(0);

    gl->batch.active = false;
    gl->batch.quad_count = 0;
    gl->state.prog = 0;
}

static void batch_add_quad(lv_evgpu_c_r_t_gl_t * gl,
                           float x1, float y1, float x2, float y2,
                           uint32_t color, uint8_t alpha) {
    if(gl->batch.active &&
       (gl->batch.color != color || gl->batch.alpha != alpha)) {
        batch_flush(gl);
    }

    if(!gl->batch.active) {
        gl->batch.active = true;
        gl->batch.color = color;
        gl->batch.alpha = alpha;
        gl->batch.quad_count = 0;
    }

    float verts[12] = {
        x1, y1,
        x2, y1,
        x1, y2,
        x2, y1,
        x1, y2,
        x2, y2,
    };

    memcpy(arena_alloc(gl, sizeof(verts)), verts, sizeof(verts));
    gl->batch.quad_count++;
}

static uint32_t grad_cache_key(uint32_t c1, uint32_t c2, int dir) {
    return c1 ^ (c2 << 1) ^ ((uint32_t)dir << 24);
}

static GLuint grad_cache_lookup(lv_evgpu_c_r_t_gl_t * gl,
                                 uint32_t c1, uint32_t c2, int dir) {
    uint32_t key = grad_cache_key(c1, c2, dir);
    for(int i = 0; i < gl->grad_cache_count; i++) {
        if(gl->grad_cache[i].key == key)
            return gl->grad_cache[i].tex;
    }
    if(gl->grad_cache_count >= EVGPU_C_R_T_GRAD_CACHE_MAX) {
        glDeleteTextures(1, &gl->grad_cache[0].tex);
        gl->grad_cache_count--;
        memmove(&gl->grad_cache[0], &gl->grad_cache[1],
                sizeof(evgpu_c_r_t_grad_cache_t) * gl->grad_cache_count);
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    gl->state.tex = tex;

    int w = 256;
    uint32_t * pixels = lv_malloc_zeroed(w * 4);
    uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF, a1 = (c1 >> 24) & 0xFF;
    uint8_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF, a2 = (c2 >> 24) & 0xFF;
    for(int i = 0; i < w; i++) {
        float t = i / (float)(w - 1);
        uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
        uint8_t g_ = (uint8_t)(g1 + (g2 - g1) * t);
        uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
        uint8_t a_ = (uint8_t)(a1 + (a2 - a1) * t);
        pixels[i] = (uint32_t)a_ << 24 | (uint32_t)b << 16 | (uint32_t)g_ << 8 | r;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    lv_free(pixels);

    int idx = gl->grad_cache_count++;
    gl->grad_cache[idx].key = key;
    gl->grad_cache[idx].tex = tex;
    gl->grad_cache[idx].w = w;
    return tex;
}

void lv_evgpu_c_r_t_gl_init(lv_evgpu_c_r_t_gl_t * gl) {
    LV_ASSERT_NULL(gl);
    memset(gl, 0, sizeof(*gl));

    GLuint vs, fs;

    vs = compile_shader(GL_VERTEX_SHADER, solid_vs);
    fs = compile_shader(GL_FRAGMENT_SHADER, solid_fs);
    gl->solid_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl->solid_u_color = glGetUniformLocation(gl->solid_prog, "u_color");
    gl->solid_u_alpha = glGetUniformLocation(gl->solid_prog, "u_alpha");
    gl->solid_u_proj = glGetUniformLocation(gl->solid_prog, "u_proj");

    vs = compile_shader(GL_VERTEX_SHADER, tex_vs);
    fs = compile_shader(GL_FRAGMENT_SHADER, tex_fs);
    gl->tex_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl->tex_u_texture = glGetUniformLocation(gl->tex_prog, "u_texture");
    gl->tex_u_recolor = glGetUniformLocation(gl->tex_prog, "u_recolor");
    gl->tex_u_recolor_opa = glGetUniformLocation(gl->tex_prog, "u_recolor_opa");
    gl->tex_u_alpha = glGetUniformLocation(gl->tex_prog, "u_alpha");
    gl->tex_u_proj = glGetUniformLocation(gl->tex_prog, "u_proj");

    vs = compile_shader(GL_VERTEX_SHADER, blur_vs);
    fs = compile_shader(GL_FRAGMENT_SHADER, blur_fs);
    gl->blur_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl->blur_u_texture = glGetUniformLocation(gl->blur_prog, "u_texture");
    gl->blur_u_dir = glGetUniformLocation(gl->blur_prog, "u_dir");
    gl->blur_u_radius = glGetUniformLocation(gl->blur_prog, "u_radius");
    gl->blur_u_size = glGetUniformLocation(gl->blur_prog, "u_size");
    gl->blur_u_proj = glGetUniformLocation(gl->blur_prog, "u_proj");

    vs = compile_shader(GL_VERTEX_SHADER, grad_vs);
    fs = compile_shader(GL_FRAGMENT_SHADER, grad_fs);
    gl->grad_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl->grad_u_color1 = glGetUniformLocation(gl->grad_prog, "u_color1");
    gl->grad_u_color2 = glGetUniformLocation(gl->grad_prog, "u_color2");
    gl->grad_u_dir = glGetUniformLocation(gl->grad_prog, "u_dir");
    gl->grad_u_proj = glGetUniformLocation(gl->grad_prog, "u_proj");

    vs = compile_shader(GL_VERTEX_SHADER, grad_tex_vs);
    fs = compile_shader(GL_FRAGMENT_SHADER, grad_tex_fs);
    gl->grad_tex_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl->grad_tex_u_proj = glGetUniformLocation(gl->grad_tex_prog, "u_proj");
    gl->grad_tex_u_tex = glGetUniformLocation(gl->grad_tex_prog, "u_grad_tex");
    gl->grad_tex_u_dir = glGetUniformLocation(gl->grad_tex_prog, "u_dir");

    glGenBuffers(1, &gl->vbo);

    gl->state.blend_src = GL_ONE;
    gl->state.blend_dst = GL_ONE_MINUS_SRC_ALPHA;
    gl->state.scissor_enabled = true;
    gl->state.scissor_box[0] = -1;

    gl->arena.cap = EVGPU_C_R_T_ARENA_SIZE;
    gl->arena.buf = lv_malloc_zeroed(gl->arena.cap);
    gl->arena.off = 0;

    glUseProgram(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);

    LV_LOG_INFO("evgpu_c_r_t GL initialized (arena=%zuKB, grad_cache=%d)",
                gl->arena.cap / 1024, EVGPU_C_R_T_GRAD_CACHE_MAX);
}

void lv_evgpu_c_r_t_gl_deinit(lv_evgpu_c_r_t_gl_t * gl) {
    if(gl->solid_prog) glDeleteProgram(gl->solid_prog);
    if(gl->tex_prog) glDeleteProgram(gl->tex_prog);
    if(gl->blur_prog) glDeleteProgram(gl->blur_prog);
    if(gl->grad_prog) glDeleteProgram(gl->grad_prog);
    if(gl->grad_tex_prog) glDeleteProgram(gl->grad_tex_prog);
    if(gl->vbo) glDeleteBuffers(1, &gl->vbo);
    for(int i = 0; i < gl->grad_cache_count; i++) {
        glDeleteTextures(1, &gl->grad_cache[i].tex);
    }
    lv_free(gl->arena.buf);
    memset(gl, 0, sizeof(*gl));
}

void lv_evgpu_c_r_t_gl_flush(lv_evgpu_c_r_t_gl_t * gl) {
    batch_flush(gl);

    gl->state.prog = 0;
    gl->state.tex = 0;

    arena_reset(gl);

    disable_scissor_cached(gl);

    glUseProgram(0);
    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_set_projection(lv_evgpu_c_r_t_gl_t * gl, int32_t w, int32_t h) {
    gl->view_w = w;
    gl->view_h = h;
    gl->proj[0] = 2.0f / w;  gl->proj[1] = 0;          gl->proj[2] = 0;  gl->proj[3] = -1.0f;
    gl->proj[4] = 0;         gl->proj[5] = -2.0f / h;   gl->proj[6] = 0;  gl->proj[7] = 1.0f;
    gl->proj[8] = 0;         gl->proj[9] = 0;            gl->proj[10] = 1; gl->proj[11] = 0;
    gl->proj[12] = 0;        gl->proj[13] = 0;           gl->proj[14] = 0; gl->proj[15] = 1;
}

void lv_evgpu_c_r_t_gl_draw_quad_solid(lv_evgpu_c_r_t_gl_t * gl,
                                       float x1, float y1, float x2, float y2,
                                       uint32_t color, uint8_t alpha) {
    batch_add_quad(gl, x1, y1, x2, y2, color, alpha);
}

void lv_evgpu_c_r_t_gl_draw_quad_tex(lv_evgpu_c_r_t_gl_t * gl,
                                     float x1, float y1, float x2, float y2,
                                     float u1, float v1, float u2, float v2,
                                     GLuint texture, uint32_t recolor, uint8_t recolor_opa, uint8_t alpha) {
    batch_flush(gl);

    float verts[] = {
        x1, y1, u1, v1,
        x2, y1, u2, v1,
        x1, y2, u1, v2,
        x2, y2, u2, v2
    };

    use_program(gl, gl->tex_prog);
    float rr = ((recolor >> 16) & 0xFF) / 255.0f;
    float rg = ((recolor >> 8) & 0xFF) / 255.0f;
    float rb = ((recolor >> 0) & 0xFF) / 255.0f;
    glUniform4f(gl->tex_u_recolor, rr, rg, rb, 1.0f);
    glUniform1f(gl->tex_u_recolor_opa, recolor_opa / 255.0f);
    glUniform1f(gl->tex_u_alpha, alpha / 255.0f);
    set_proj_uniform(gl, gl->tex_u_proj);

    bind_texture(gl, texture);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(gl->tex_u_texture, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_blur_quad(lv_evgpu_c_r_t_gl_t * gl,
                                  float x1, float y1, float x2, float y2,
                                  float u1, float v1, float u2, float v2,
                                  GLuint texture, float dir_x, float dir_y,
                                  float radius, float size_w, float size_h) {
    batch_flush(gl);

    float verts[] = {
        x1, y1, u1, v1,
        x2, y1, u2, v1,
        x1, y2, u1, v2,
        x2, y2, u2, v2
    };

    use_program(gl, gl->blur_prog);
    glUniform2f(gl->blur_u_dir, dir_x, dir_y);
    glUniform1f(gl->blur_u_radius, radius);
    glUniform2f(gl->blur_u_size, size_w, size_h);
    set_proj_uniform(gl, gl->blur_u_proj);

    bind_texture(gl, texture);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(gl->blur_u_texture, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_draw_quad_grad(lv_evgpu_c_r_t_gl_t * gl,
                                      float x1, float y1, float x2, float y2,
                                      uint32_t color1, uint32_t color2, int dir) {
    batch_flush(gl);

    GLuint grad_tex = grad_cache_lookup(gl, color1, color2, dir);

    float verts[] = {
        x1, y1, 0.0f, 0.0f,
        x2, y1, 1.0f, 0.0f,
        x1, y2, 0.0f, 1.0f,
        x2, y2, 1.0f, 1.0f
    };

    use_program(gl, gl->grad_tex_prog);
    glUniform1i(gl->grad_tex_u_dir, dir);
    set_proj_uniform(gl, gl->grad_tex_u_proj);

    bind_texture(gl, grad_tex);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(gl->grad_tex_u_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_draw_triangles_solid(lv_evgpu_c_r_t_gl_t * gl,
                                            const float * verts, int count,
                                            uint32_t color, uint8_t alpha) {
    batch_flush(gl);

    use_program(gl, gl->solid_prog);
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 0) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    glUniform1f(gl->solid_u_alpha, alpha / 255.0f);
    set_proj_uniform(gl, gl->solid_u_proj);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_draw_triangle_strip_solid(lv_evgpu_c_r_t_gl_t * gl,
                                                  const float * verts, int count,
                                                  uint32_t color, uint8_t alpha) {
    batch_flush(gl);

    use_program(gl, gl->solid_prog);
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 0) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    glUniform1f(gl->solid_u_alpha, alpha / 255.0f);
    set_proj_uniform(gl, gl->solid_u_proj);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, count);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_set_scissor(int32_t x, int32_t y, int32_t w, int32_t h) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
}

void lv_evgpu_c_r_t_gl_disable_scissor(void) {
    glDisable(GL_SCISSOR_TEST);
}

void lv_evgpu_c_r_t_gl_clear(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

#endif
