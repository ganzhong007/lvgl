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
    "void main() {\n"
    "  gl_FragColor = u_color;\n"
    "}";

/* SDF rounded rect: fill (u_border<=0) or border ring (u_border>0). */
static const char * round_vs =
    "uniform mat4 u_proj;\n"
    "attribute vec2 a_pos;\n"
    "varying vec2 v_pos;\n"
    "void main() {\n"
    "  v_pos = a_pos;\n"
    "  gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);\n"
    "}";

static const char * round_fs =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "uniform float u_border;\n"
    "varying vec2 v_pos;\n"
    "float sdRoundBox(vec2 p, vec2 b, float r) {\n"
    "  vec2 q = abs(p) - b + vec2(r);\n"
    "  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;\n"
    "}\n"
    "void main() {\n"
    "  vec2 halfSize = u_rect.zw * 0.5;\n"
    "  vec2 center = u_rect.xy + halfSize;\n"
    "  float r = min(u_radius, min(halfSize.x, halfSize.y));\n"
    "  float d = sdRoundBox(v_pos - center, halfSize, r);\n"
    "  float aa = 0.75;\n"
    "  float a;\n"
    "  if(u_border > 0.5) {\n"
    "    float bw = min(u_border, min(halfSize.x, halfSize.y));\n"
    "    vec2 innerHalf = max(halfSize - vec2(bw), vec2(0.0));\n"
    "    float ri = max(r - bw, 0.0);\n"
    "    float di = sdRoundBox(v_pos - center, innerHalf, ri);\n"
    "    float outer = 1.0 - smoothstep(-aa, aa, d);\n"
    "    float inner = smoothstep(-aa, aa, di);\n"
    "    a = outer * inner;\n"
    "  } else {\n"
    "    a = 1.0 - smoothstep(-aa, aa, d);\n"
    "  }\n"
    "  if(a < 0.004) discard;\n"
    "  gl_FragColor = vec4(u_color.rgb, u_color.a * a);\n"
    "}";

/* Multi-stop concentric radial + optional rounded clip. */
static const char * radial_vs =
    "uniform mat4 u_proj;\n"
    "attribute vec2 a_pos;\n"
    "varying vec2 v_pos;\n"
    "void main() {\n"
    "  v_pos = a_pos;\n"
    "  gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);\n"
    "}";

static const char * radial_fs =
    "precision mediump float;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "uniform vec2 u_center;\n"
    "uniform vec2 u_radii;\n"
    "uniform int u_stops_count;\n"
    "uniform vec4 u_stop_color[8];\n"
    "uniform float u_stop_frac[8];\n"
    "varying vec2 v_pos;\n"
    "float sdRoundBox(vec2 p, vec2 b, float r) {\n"
    "  vec2 q = abs(p) - b + vec2(r);\n"
    "  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;\n"
    "}\n"
    "vec4 sample_grad(float t) {\n"
    "  t = clamp(t, 0.0, 1.0);\n"
    "  if(u_stops_count <= 1) return u_stop_color[0];\n"
    "  for(int i = 0; i < 7; i++) {\n"
    "    if(i + 1 >= u_stops_count) break;\n"
    "    float f0 = u_stop_frac[i];\n"
    "    float f1 = u_stop_frac[i + 1];\n"
    "    if(t <= f1 || i + 2 >= u_stops_count) {\n"
    "      float u = clamp((t - f0) / max(f1 - f0, 0.0001), 0.0, 1.0);\n"
    "      return mix(u_stop_color[i], u_stop_color[i + 1], u);\n"
    "    }\n"
    "  }\n"
    "  return u_stop_color[u_stops_count - 1];\n"
    "}\n"
    "void main() {\n"
    "  if(u_radius > 0.0) {\n"
    "    vec2 halfSize = u_rect.zw * 0.5;\n"
    "    vec2 c = u_rect.xy + halfSize;\n"
    "    float r = min(u_radius, min(halfSize.x, halfSize.y));\n"
    "    float d = sdRoundBox(v_pos - c, halfSize, r);\n"
    "    if(d > 0.75) discard;\n"
    "  }\n"
    "  float t = (length(v_pos - u_center) - u_radii.x) / max(u_radii.y - u_radii.x, 0.0001);\n"
    "  vec4 c = sample_grad(t);\n"
    "  if(u_radius > 0.0) {\n"
    "    vec2 halfSize = u_rect.zw * 0.5;\n"
    "    vec2 ctr = u_rect.xy + halfSize;\n"
    "    float r = min(u_radius, min(halfSize.x, halfSize.y));\n"
    "    float d = sdRoundBox(v_pos - ctr, halfSize, r);\n"
    "    c.a *= 1.0 - smoothstep(-0.75, 0.75, d);\n"
    "  }\n"
    "  if(c.a < 0.004) discard;\n"
    "  gl_FragColor = c;\n"
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
    "  gl_FragColor = vec4(recolored.rgb, recolored.a * u_alpha);\n"
    "}";

/* Color + bitmap mask (A8/L8 as GL_ALPHA). Outside mask UV → alpha 0. */
static const char * tex_mask_vs =
    "uniform mat4 u_proj;\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_tex;\n"
    "attribute vec2 a_mask;\n"
    "varying vec2 v_tex;\n"
    "varying vec2 v_mask;\n"
    "void main() {\n"
    "  v_tex = a_tex;\n"
    "  v_mask = a_mask;\n"
    "  gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);\n"
    "}";

static const char * tex_mask_fs =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "uniform sampler2D u_mask;\n"
    "uniform vec4 u_recolor;\n"
    "uniform float u_recolor_opa;\n"
    "uniform float u_alpha;\n"
    "varying vec2 v_tex;\n"
    "varying vec2 v_mask;\n"
    "void main() {\n"
    "  float m = 0.0;\n"
    "  if(v_mask.x >= 0.0 && v_mask.x <= 1.0 && v_mask.y >= 0.0 && v_mask.y <= 1.0) {\n"
    "    m = texture2D(u_mask, v_mask).a;\n"
    "  }\n"
    "  vec4 texel = texture2D(u_texture, v_tex);\n"
    "  vec4 recolored = mix(texel, vec4(u_recolor.rgb, texel.a), u_recolor_opa);\n"
    "  float a = recolored.a * m * u_alpha;\n"
    "  if(a < 0.004) discard;\n"
    "  gl_FragColor = vec4(recolored.rgb, a);\n"
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
    "  vec4 c = mix(u_color1, u_color2, t);\n"
    "  gl_FragColor = vec4(c.rgb * c.a, c.a);\n"
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
    "  vec4 c = texture2D(u_grad_tex, vec2(v_tex.x, 0.5));\n"
    "  gl_FragColor = c;\n"
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
    glBindAttribLocation(prog, 0, "a_pos");
    glBindAttribLocation(prog, 1, "a_tex");
    glBindAttribLocation(prog, 2, "a_mask");
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
    a *= gl->batch.alpha / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    set_proj_uniform(gl, gl->solid_u_proj);

    glEnable(GL_BLEND);
    glBlendFunc(gl->state.blend_src, gl->state.blend_dst);

    int total_verts = gl->batch.quad_count * 6;
    int stride = 2 * (int)sizeof(float);
    /* Arena is exclusive to the solid batch: always pack from buf[0], and
     * rewind after draw. Previously flush always read buf[0] while later
     * batches appended — so green card colors were applied to leftover
     * fullscreen world verts (green wash). */
    void * ptr = gl->arena.buf;

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, ptr);
    glDrawArrays(GL_TRIANGLES, 0, total_verts);
    glDisableVertexAttribArray(0);

    gl->batch.active = false;
    gl->batch.quad_count = 0;
    gl->arena.off = 0;
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
        gl->arena.off = 0; /* solid batch always packs from the arena base */
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
    gl->solid_u_alpha = -1; /* alpha folded into u_color (premul on CPU) */
    gl->solid_u_proj = glGetUniformLocation(gl->solid_prog, "u_proj");

    vs = compile_shader(GL_VERTEX_SHADER, round_vs);
    fs = compile_shader(GL_FRAGMENT_SHADER, round_fs);
    gl->round_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl->round_u_color = glGetUniformLocation(gl->round_prog, "u_color");
    gl->round_u_proj = glGetUniformLocation(gl->round_prog, "u_proj");
    gl->round_u_rect = glGetUniformLocation(gl->round_prog, "u_rect");
    gl->round_u_radius = glGetUniformLocation(gl->round_prog, "u_radius");
    gl->round_u_border = glGetUniformLocation(gl->round_prog, "u_border");

    vs = compile_shader(GL_VERTEX_SHADER, radial_vs);
    fs = compile_shader(GL_FRAGMENT_SHADER, radial_fs);
    gl->radial_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl->radial_u_proj = glGetUniformLocation(gl->radial_prog, "u_proj");
    gl->radial_u_rect = glGetUniformLocation(gl->radial_prog, "u_rect");
    gl->radial_u_radius = glGetUniformLocation(gl->radial_prog, "u_radius");
    gl->radial_u_center = glGetUniformLocation(gl->radial_prog, "u_center");
    gl->radial_u_radii = glGetUniformLocation(gl->radial_prog, "u_radii");
    gl->radial_u_stops_count = glGetUniformLocation(gl->radial_prog, "u_stops_count");
    gl->radial_u_stop_color = glGetUniformLocation(gl->radial_prog, "u_stop_color");
    gl->radial_u_stop_frac = glGetUniformLocation(gl->radial_prog, "u_stop_frac");

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

    vs = compile_shader(GL_VERTEX_SHADER, tex_mask_vs);
    fs = compile_shader(GL_FRAGMENT_SHADER, tex_mask_fs);
    gl->tex_mask_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl->tex_mask_u_texture = glGetUniformLocation(gl->tex_mask_prog, "u_texture");
    gl->tex_mask_u_mask = glGetUniformLocation(gl->tex_mask_prog, "u_mask");
    gl->tex_mask_u_recolor = glGetUniformLocation(gl->tex_mask_prog, "u_recolor");
    gl->tex_mask_u_recolor_opa = glGetUniformLocation(gl->tex_mask_prog, "u_recolor_opa");
    gl->tex_mask_u_alpha = glGetUniformLocation(gl->tex_mask_prog, "u_alpha");
    gl->tex_mask_u_proj = glGetUniformLocation(gl->tex_mask_prog, "u_proj");

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

    gl->state.blend_src = GL_SRC_ALPHA;
    gl->state.blend_dst = GL_ONE_MINUS_SRC_ALPHA;
    gl->state.scissor_enabled = true;
    gl->state.scissor_box[0] = -1;

    gl->arena.cap = EVGPU_C_R_T_ARENA_SIZE;
    gl->arena.buf = lv_malloc_zeroed(gl->arena.cap);
    gl->arena.off = 0;

    glUseProgram(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);

    LV_LOG_INFO("evgpu_c_r_t GL initialized (arena=%zuKB, grad_cache=%d)",
                gl->arena.cap / 1024, EVGPU_C_R_T_GRAD_CACHE_MAX);
}

void lv_evgpu_c_r_t_gl_deinit(lv_evgpu_c_r_t_gl_t * gl) {
    if(gl->solid_prog) glDeleteProgram(gl->solid_prog);
    if(gl->round_prog) glDeleteProgram(gl->round_prog);
    if(gl->radial_prog) glDeleteProgram(gl->radial_prog);
    if(gl->tex_prog) glDeleteProgram(gl->tex_prog);
    if(gl->tex_mask_prog) glDeleteProgram(gl->tex_mask_prog);
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

void lv_evgpu_c_r_t_gl_set_projection(lv_evgpu_c_r_t_gl_t * gl,
                                      int32_t ox, int32_t oy, int32_t w, int32_t h) {
    gl->view_w = w;
    gl->view_h = h;
    gl->origin_x = ox;
    gl->origin_y = oy;
    /* Column-major ortho (GLES2 requires transpose=GL_FALSE):
     *   x_ndc =  2(x-ox)/w - 1
     *   y_ndc = -2(y-oy)/h + 1   (LVGL top-left → GL NDC) */
    gl->proj[0]  =  2.0f / (float)w; gl->proj[1]  = 0;               gl->proj[2]  = 0; gl->proj[3]  = 0;
    gl->proj[4]  =  0;               gl->proj[5]  = -2.0f / (float)h; gl->proj[6]  = 0; gl->proj[7]  = 0;
    gl->proj[8]  =  0;               gl->proj[9]  = 0;               gl->proj[10] = 1; gl->proj[11] = 0;
    gl->proj[12] = -2.0f * (float)ox / (float)w - 1.0f;
    gl->proj[13] =  2.0f * (float)oy / (float)h + 1.0f;
    gl->proj[14] = 0;
    gl->proj[15] = 1;
}

void lv_evgpu_c_r_t_gl_draw_quad_solid(lv_evgpu_c_r_t_gl_t * gl,
                                       float x1, float y1, float x2, float y2,
                                       uint32_t color, uint8_t alpha) {
    batch_add_quad(gl, x1, y1, x2, y2, color, alpha);
}

void lv_evgpu_c_r_t_gl_draw_round_rect(lv_evgpu_c_r_t_gl_t * gl,
                                       float x1, float y1, float x2, float y2,
                                       float radius, float border_w,
                                       uint32_t color, uint8_t alpha) {
    batch_flush(gl);

    float w = x2 - x1;
    float h = y2 - y1;
    if(w <= 0.f || h <= 0.f) return;
    if(radius < 0.f) radius = 0.f;
    float max_r = (w < h ? w : h) * 0.5f;
    if(radius > max_r) radius = max_r;

    float verts[] = {
        x1, y1,
        x2, y1,
        x1, y2,
        x2, y2
    };

    use_program(gl, gl->round_prog);
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 0) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    a *= alpha / 255.0f;
    glUniform4f(gl->round_u_color, r, g, b, a);
    glUniform4f(gl->round_u_rect, x1, y1, w, h);
    glUniform1f(gl->round_u_radius, radius);
    glUniform1f(gl->round_u_border, border_w);
    set_proj_uniform(gl, gl->round_u_proj);

    glEnable(GL_BLEND);
    glBlendFunc(gl->state.blend_src, gl->state.blend_dst);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_draw_radial_grad(lv_evgpu_c_r_t_gl_t * gl,
                                        float x1, float y1, float x2, float y2,
                                        float clip_radius,
                                        float cx, float cy, float r0, float r1,
                                        const float * stop_rgba,
                                        const float * stop_fracs,
                                        int stops_count) {
    batch_flush(gl);

    float w = x2 - x1;
    float h = y2 - y1;
    if(w <= 0.f || h <= 0.f || stops_count < 1 || !stop_rgba || !stop_fracs) return;
    if(stops_count > EVGPU_C_R_T_RADIAL_MAX_STOPS) stops_count = EVGPU_C_R_T_RADIAL_MAX_STOPS;
    if(clip_radius < 0.f) clip_radius = 0.f;
    float max_r = (w < h ? w : h) * 0.5f;
    if(clip_radius > max_r) clip_radius = max_r;
    if(r1 < r0 + 0.001f) r1 = r0 + 0.001f;

    float verts[] = {
        x1, y1,
        x2, y1,
        x1, y2,
        x2, y2
    };

    use_program(gl, gl->radial_prog);
    glUniform4f(gl->radial_u_rect, x1, y1, w, h);
    glUniform1f(gl->radial_u_radius, clip_radius);
    glUniform2f(gl->radial_u_center, cx, cy);
    glUniform2f(gl->radial_u_radii, r0, r1);
    glUniform1i(gl->radial_u_stops_count, stops_count);
    glUniform4fv(gl->radial_u_stop_color, stops_count, stop_rgba);
    glUniform1fv(gl->radial_u_stop_frac, stops_count, stop_fracs);
    set_proj_uniform(gl, gl->radial_u_proj);

    glEnable(GL_BLEND);
    glBlendFunc(gl->state.blend_src, gl->state.blend_dst);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_draw_quad_tex_strip(lv_evgpu_c_r_t_gl_t * gl,
                                           const float * xyuv16,
                                           GLuint texture, uint32_t recolor,
                                           uint8_t recolor_opa, uint8_t alpha)
{
    if(xyuv16 == NULL || texture == 0) return;
    batch_flush(gl);

    use_program(gl, gl->tex_prog);
    float rr = ((recolor >> 16) & 0xFF) / 255.0f;
    float rg = ((recolor >> 8) & 0xFF) / 255.0f;
    float rb = ((recolor >> 0) & 0xFF) / 255.0f;
    glUniform4f(gl->tex_u_recolor, rr, rg, rb, 1.0f);
    glUniform1f(gl->tex_u_recolor_opa, recolor_opa / 255.0f);
    glUniform1f(gl->tex_u_alpha, alpha / 255.0f);
    set_proj_uniform(gl, gl->tex_u_proj);

    glEnable(GL_BLEND);
    glBlendFunc(gl->state.blend_src, gl->state.blend_dst);

    bind_texture(gl, texture);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(gl->tex_u_texture, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), xyuv16);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), xyuv16 + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_draw_quad_tex(lv_evgpu_c_r_t_gl_t * gl,
                                     float x1, float y1, float x2, float y2,
                                     float u1, float v1, float u2, float v2,
                                     GLuint texture, uint32_t recolor, uint8_t recolor_opa, uint8_t alpha) {
    float verts[] = {
        x1, y1, u1, v1,
        x2, y1, u2, v1,
        x1, y2, u1, v2,
        x2, y2, u2, v2
    };
    lv_evgpu_c_r_t_gl_draw_quad_tex_strip(gl, verts, texture, recolor, recolor_opa, alpha);
}

void lv_evgpu_c_r_t_gl_draw_quad_tex_mask_strip(lv_evgpu_c_r_t_gl_t * gl,
                                                const float * xyuvmuv24,
                                                GLuint texture, GLuint mask_tex,
                                                uint32_t recolor, uint8_t recolor_opa,
                                                uint8_t alpha)
{
    if(xyuvmuv24 == NULL || texture == 0) return;
    if(mask_tex == 0 || gl->tex_mask_prog == 0) {
        /* Drop mask channels → reuse color-only strip (x y u v from each vert). */
        float xyuv[16];
        for(int i = 0; i < 4; i++) {
            xyuv[i * 4 + 0] = xyuvmuv24[i * 6 + 0];
            xyuv[i * 4 + 1] = xyuvmuv24[i * 6 + 1];
            xyuv[i * 4 + 2] = xyuvmuv24[i * 6 + 2];
            xyuv[i * 4 + 3] = xyuvmuv24[i * 6 + 3];
        }
        lv_evgpu_c_r_t_gl_draw_quad_tex_strip(gl, xyuv, texture, recolor, recolor_opa, alpha);
        return;
    }

    batch_flush(gl);

    use_program(gl, gl->tex_mask_prog);
    float rr = ((recolor >> 16) & 0xFF) / 255.0f;
    float rg = ((recolor >> 8) & 0xFF) / 255.0f;
    float rb = ((recolor >> 0) & 0xFF) / 255.0f;
    glUniform4f(gl->tex_mask_u_recolor, rr, rg, rb, 1.0f);
    glUniform1f(gl->tex_mask_u_recolor_opa, recolor_opa / 255.0f);
    glUniform1f(gl->tex_mask_u_alpha, alpha / 255.0f);
    set_proj_uniform(gl, gl->tex_mask_u_proj);

    glEnable(GL_BLEND);
    glBlendFunc(gl->state.blend_src, gl->state.blend_dst);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(gl->tex_mask_u_texture, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mask_tex);
    glUniform1i(gl->tex_mask_u_mask, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), xyuvmuv24);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), xyuvmuv24 + 2);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), xyuvmuv24 + 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    gl->state.tex = texture;
    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_draw_quad_tex_mask(lv_evgpu_c_r_t_gl_t * gl,
                                          float x1, float y1, float x2, float y2,
                                          float u1, float v1, float u2, float v2,
                                          float mu1, float mv1, float mu2, float mv2,
                                          GLuint texture, GLuint mask_tex,
                                          uint32_t recolor, uint8_t recolor_opa, uint8_t alpha)
{
    float verts[] = {
        x1, y1, u1, v1, mu1, mv1,
        x2, y1, u2, v1, mu2, mv1,
        x1, y2, u1, v2, mu1, mv2,
        x2, y2, u2, v2, mu2, mv2
    };
    lv_evgpu_c_r_t_gl_draw_quad_tex_mask_strip(gl, verts, texture, mask_tex,
                                               recolor, recolor_opa, alpha);
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
    a *= alpha / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    set_proj_uniform(gl, gl->solid_u_proj);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

GLuint lv_evgpu_c_r_t_gl_create_grad_tex_stops(const lv_grad_stop_t * stops, uint16_t stops_count,
                                               lv_opa_t opa_mul)
{
    if(stops == NULL || stops_count == 0) return 0;

    const int w = 256;
    uint32_t * pixels = lv_malloc_zeroed((size_t)w * 4u);
    if(pixels == NULL) return 0;

    for(int i = 0; i < w; i++) {
        float t = (w == 1) ? 0.f : (float)i / (float)(w - 1);
        float frac = t * 255.f;

        uint16_t i0 = 0;
        uint16_t i1 = 0;
        if(frac <= stops[0].frac) {
            i0 = i1 = 0;
        }
        else if(frac >= stops[stops_count - 1].frac) {
            i0 = i1 = (uint16_t)(stops_count - 1);
        }
        else {
            for(uint16_t s = 0; s + 1 < stops_count; s++) {
                if(frac >= stops[s].frac && frac <= stops[s + 1].frac) {
                    i0 = s;
                    i1 = (uint16_t)(s + 1);
                    break;
                }
            }
        }

        float local_t = 0.f;
        if(i0 != i1 && stops[i1].frac != stops[i0].frac) {
            local_t = (frac - (float)stops[i0].frac) / (float)(stops[i1].frac - stops[i0].frac);
        }

        float r = stops[i0].color.red + (stops[i1].color.red - stops[i0].color.red) * local_t;
        float g = stops[i0].color.green + (stops[i1].color.green - stops[i0].color.green) * local_t;
        float b = stops[i0].color.blue + (stops[i1].color.blue - stops[i0].color.blue) * local_t;
        float a = stops[i0].opa + (stops[i1].opa - stops[i0].opa) * local_t;
        a = a * (float)opa_mul / 255.f;

        uint8_t ru = (uint8_t)(r < 0.f ? 0.f : (r > 255.f ? 255.f : r));
        uint8_t gu = (uint8_t)(g < 0.f ? 0.f : (g > 255.f ? 255.f : g));
        uint8_t bu = (uint8_t)(b < 0.f ? 0.f : (b > 255.f ? 255.f : b));
        uint8_t au = (uint8_t)(a < 0.f ? 0.f : (a > 255.f ? 255.f : a));
        /* GL_RGBA byte order */
        pixels[i] = ((uint32_t)au << 24) | ((uint32_t)bu << 16) | ((uint32_t)gu << 8) | (uint32_t)ru;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    lv_free(pixels);
    return tex;
}

void lv_evgpu_c_r_t_gl_draw_triangles_tex(lv_evgpu_c_r_t_gl_t * gl,
                                          const float * xyuv, int count,
                                          GLuint texture, uint32_t recolor,
                                          uint8_t recolor_opa, uint8_t alpha,
                                          bool use_grad_sampler)
{
    if(xyuv == NULL || count < 3 || texture == 0) return;
    batch_flush(gl);

    if(use_grad_sampler) {
        use_program(gl, gl->grad_tex_prog);
        set_proj_uniform(gl, gl->grad_tex_u_proj);
        glUniform1i(gl->grad_tex_u_dir, 0);
        bind_texture(gl, texture);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(gl->grad_tex_u_tex, 0);
    }
    else {
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
    }

    glEnable(GL_BLEND);
    glBlendFunc(gl->state.blend_src, gl->state.blend_dst);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), xyuv);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), xyuv + 2);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(1);
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
    a *= alpha / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    set_proj_uniform(gl, gl->solid_u_proj);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, count);
    glDisableVertexAttribArray(0);

    gl->state.prog = 0;
}

void lv_evgpu_c_r_t_gl_set_scissor(lv_evgpu_c_r_t_gl_t * gl, int32_t x, int32_t y, int32_t w, int32_t h) {
    glEnable(GL_SCISSOR_TEST);
    /* Absolute LVGL coords → FBO-local, then flip Y for GL scissor. */
    if(gl) {
        x -= gl->origin_x;
        y -= gl->origin_y;
    }
    int32_t gy = y;
    if(gl && gl->view_h > 0) {
        gy = gl->view_h - y - h;
    }
    if(x < 0) {
        w += x;
        x = 0;
    }
    if(gy < 0) {
        h += gy;
        gy = 0;
    }
    if(w < 0) w = 0;
    if(h < 0) h = 0;
    glScissor(x, gy, w, h);
}

void lv_evgpu_c_r_t_gl_disable_scissor(void) {
    glDisable(GL_SCISSOR_TEST);
}

void lv_evgpu_c_r_t_gl_clear(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

#endif
