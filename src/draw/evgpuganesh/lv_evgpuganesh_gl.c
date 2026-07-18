#include "lv_evgpuganesh_gl.h"

#if LV_USE_DRAW_EVGPUGANESH

#include "../../misc/lv_port_layer_trace.h"

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

void lv_evgpuganesh_gl_init(lv_evgpuganesh_gl_t * gl) {
    LV_ASSERT_NULL(gl);

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

    glGenBuffers(1, &gl->vbo);
#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)
    GLuint vao = 0;
    glGenVertexArraysOES(1, &vao);
    gl->vao = vao;
#else
    gl->vao = 0;
#endif

    glUseProgram(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
}

void lv_evgpuganesh_gl_deinit(lv_evgpuganesh_gl_t * gl) {
    if(gl->solid_prog) glDeleteProgram(gl->solid_prog);
    if(gl->tex_prog) glDeleteProgram(gl->tex_prog);
    if(gl->blur_prog) glDeleteProgram(gl->blur_prog);
    if(gl->grad_prog) glDeleteProgram(gl->grad_prog);
    if(gl->vbo) glDeleteBuffers(1, &gl->vbo);
#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)
    if(gl->vao) glDeleteVertexArraysOES(1, &gl->vao);
#endif
}

void lv_evgpuganesh_gl_set_projection(lv_evgpuganesh_gl_t * gl, int32_t w, int32_t h) {
    LV_UNUSED(gl);
    glViewport(0, 0, w, h);
}

static void set_solid_proj(lv_evgpuganesh_gl_t * gl, int32_t w, int32_t h) {
    glUseProgram(gl->solid_prog);
    float proj[16] = {
        2.0f/w, 0,      0, -1.0f,
        0,     -2.0f/h, 0,  1.0f,
        0,      0,      1,  0,
        0,      0,      0,  1
    };
    glUniformMatrix4fv(gl->solid_u_proj, 1, GL_FALSE, proj);
}

static void set_tex_proj(lv_evgpuganesh_gl_t * gl, int32_t w, int32_t h) {
    glUseProgram(gl->tex_prog);
    float proj[16] = {
        2.0f/w, 0,      0, -1.0f,
        0,     -2.0f/h, 0,  1.0f,
        0,      0,      1,  0,
        0,      0,      0,  1
    };
    glUniformMatrix4fv(gl->tex_u_proj, 1, GL_FALSE, proj);
}

static void set_blur_proj(lv_evgpuganesh_gl_t * gl, int32_t w, int32_t h) {
    glUseProgram(gl->blur_prog);
    float proj[16] = {
        2.0f/w, 0,      0, -1.0f,
        0,     -2.0f/h, 0,  1.0f,
        0,      0,      1,  0,
        0,      0,      0,  1
    };
    glUniformMatrix4fv(gl->blur_u_proj, 1, GL_FALSE, proj);
}

static void set_grad_proj(lv_evgpuganesh_gl_t * gl, int32_t w, int32_t h) {
    glUseProgram(gl->grad_prog);
    float proj[16] = {
        2.0f/w, 0,      0, -1.0f,
        0,     -2.0f/h, 0,  1.0f,
        0,      0,      1,  0,
        0,      0,      0,  1
    };
    glUniformMatrix4fv(gl->grad_u_proj, 1, GL_FALSE, proj);
}

void lv_evgpuganesh_gl_draw_quad_solid(lv_evgpuganesh_gl_t * gl,
                                       float x1, float y1, float x2, float y2,
                                       uint32_t color, uint8_t alpha) {
    float verts[] = {
        x1, y1,
        x2, y1,
        x1, y2,
        x2, y2
    };

    glUseProgram(gl->solid_prog);
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 0) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    glUniform1f(gl->solid_u_alpha, alpha / 255.0f);

    glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void lv_evgpuganesh_gl_draw_quad_tex(lv_evgpuganesh_gl_t * gl,
                                     float x1, float y1, float x2, float y2,
                                     float u1, float v1, float u2, float v2,
                                     GLuint texture, uint32_t recolor, uint8_t recolor_opa, uint8_t alpha) {
    float verts[] = {
        x1, y1, u1, v1,
        x2, y1, u2, v1,
        x1, y2, u1, v2,
        x2, y2, u2, v2
    };

    glUseProgram(gl->tex_prog);
    float rr = ((recolor >> 16) & 0xFF) / 255.0f;
    float rg = ((recolor >> 8) & 0xFF) / 255.0f;
    float rb = ((recolor >> 0) & 0xFF) / 255.0f;
    glUniform4f(gl->tex_u_recolor, rr, rg, rb, 1.0f);
    glUniform1f(gl->tex_u_recolor_opa, recolor_opa / 255.0f);
    glUniform1f(gl->tex_u_alpha, alpha / 255.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(gl->tex_u_texture, 0);

    glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void lv_evgpuganesh_gl_draw_quad_grad(lv_evgpuganesh_gl_t * gl,
                                      float x1, float y1, float x2, float y2,
                                      uint32_t color1, uint32_t color2, int dir) {
    float verts[] = { x1, y1, x2, y2 };

    glUseProgram(gl->grad_prog);
    float r1 = ((color1 >> 16) & 0xFF) / 255.0f;
    float g1 = ((color1 >> 8) & 0xFF) / 255.0f;
    float b1 = ((color1 >> 0) & 0xFF) / 255.0f;
    float a1 = ((color1 >> 24) & 0xFF) / 255.0f;
    float r2 = ((color2 >> 16) & 0xFF) / 255.0f;
    float g2 = ((color2 >> 8) & 0xFF) / 255.0f;
    float b2 = ((color2 >> 0) & 0xFF) / 255.0f;
    float a2 = ((color2 >> 24) & 0xFF) / 255.0f;
    glUniform4f(gl->grad_u_color1, r1, g1, b1, a1);
    glUniform4f(gl->grad_u_color2, r2, g2, b2, a2);
    glUniform1i(gl->grad_u_dir, dir);

    glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void lv_evgpuganesh_gl_draw_triangles_solid(lv_evgpuganesh_gl_t * gl,
                                            const float * verts, int count,
                                            uint32_t color, uint8_t alpha) {
    glUseProgram(gl->solid_prog);
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 0) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    glUniform1f(gl->solid_u_alpha, alpha / 255.0f);

    glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
    glBufferData(GL_ARRAY_BUFFER, count * 2 * sizeof(float), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void lv_evgpuganesh_gl_draw_triangle_strip_solid(lv_evgpuganesh_gl_t * gl,
                                                  const float * verts, int count,
                                                  uint32_t color, uint8_t alpha) {
    glUseProgram(gl->solid_prog);
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 0) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    glUniform4f(gl->solid_u_color, r, g, b, a);
    glUniform1f(gl->solid_u_alpha, alpha / 255.0f);

    glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
    glBufferData(GL_ARRAY_BUFFER, count * 2 * sizeof(float), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, count);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void lv_evgpuganesh_gl_set_scissor(int32_t x, int32_t y, int32_t w, int32_t h) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
}

void lv_evgpuganesh_gl_disable_scissor(void) {
    glDisable(GL_SCISSOR_TEST);
}

void lv_evgpuganesh_gl_clear(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

#endif
