/**
 * @file lv_g100_shader.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_g100_shader.h"

#if LV_USE_DRAW_G100

#include "lv_g100_context.h"
#include "lv_draw_g100_private.h"

#if LV_USE_OPENGLES && LV_USE_EGL
    #include "../../drivers/opengles/lv_opengles_private.h"
    #define LV_G100_SHADER_HAS_GL 1
#else
    #define LV_G100_SHADER_HAS_GL 0
#endif

/*********************
 *      DEFINES
 *********************/

#if LV_G100_SHADER_HAS_GL

static const char g100_vert_src[] =
    "precision mediump float;\n"
    "attribute vec2 a_pos;\n"
    "uniform mat3 u_matrix;\n"
    "uniform vec2 u_view_size;\n"
    "uniform vec4 u_rect;\n"
    "varying vec2 v_pos;\n"
    "void main(void) {\n"
    "  v_pos = u_rect.xy + a_pos * u_rect.zw;\n"
    "  vec3 p = u_matrix * vec3(a_pos, 1.0);\n"
    "  gl_Position = vec4(2.0 * p.x / u_view_size.x - 1.0,\n"
    "                     1.0 - 2.0 * p.y / u_view_size.y, 0.0, 1.0);\n"
    "}\n";

static const char g100_frag_src[] =
    "precision mediump float;\n"
    "varying vec2 v_pos;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "uniform vec4 u_color;\n"
    "void main(void) {\n"
    "  if(u_radius > 0.0) {\n"
    "    vec2 half_size = u_rect.zw * 0.5;\n"
    "    vec2 c = u_rect.xy + half_size;\n"
    "    vec2 q = abs(v_pos - c) - half_size + vec2(u_radius);\n"
    "    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - u_radius;\n"
    "    if(d > 0.0) discard;\n"
    "  }\n"
    "  vec4 c = u_color;\n"
    "  c.rgb *= c.a;\n"
    "  gl_FragColor = c;\n"
    "}\n";

static const char g100_grad_vert_src[] =
    "precision mediump float;\n"
    "attribute vec2 a_pos;\n"
    "uniform mat3 u_matrix;\n"
    "uniform vec2 u_view_size;\n"
    "uniform vec4 u_rect;\n"
    "varying vec2 v_pos;\n"
    "void main(void) {\n"
    "  v_pos = u_rect.xy + a_pos * u_rect.zw;\n"
    "  vec3 p = u_matrix * vec3(a_pos, 1.0);\n"
    "  gl_Position = vec4(2.0 * p.x / u_view_size.x - 1.0,\n"
    "                     1.0 - 2.0 * p.y / u_view_size.y, 0.0, 1.0);\n"
    "}\n";

static const char g100_grad_frag_src[] =
    "precision mediump float;\n"
    "varying vec2 v_pos;\n"
    "uniform int u_dir;\n"
    "uniform int u_extend;\n"
    "uniform int u_stops_count;\n"
    "uniform vec4 u_stop_color[8];\n"
    "uniform float u_stop_frac[8];\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "uniform vec4 u_linear;\n"
    "uniform vec4 u_radial0;\n"
    "uniform vec4 u_radial1;\n"
    "uniform vec2 u_conical_center;\n"
    "uniform vec2 u_conical_angles;\n"
    "float extend_t(float t) {\n"
    "  if(u_extend == 0) return clamp(t, 0.0, 1.0);\n"
    "  if(u_extend == 1) return fract(t);\n"
    "  return 1.0 - abs(mod(t, 2.0) - 1.0);\n"
    "}\n"
    "float linear_t(vec2 p, vec4 l) {\n"
    "  vec2 v = l.zw - l.xy;\n"
    "  float len2 = dot(v, v);\n"
    "  if(len2 < 0.0001) return 0.0;\n"
    "  return dot(p - l.xy, v) / len2;\n"
    "}\n"
    "float radial_t(vec2 p) {\n"
    "  vec2 f = u_radial0.xy;\n"
    "  vec2 e = u_radial1.xy;\n"
    "  float r0 = u_radial0.z;\n"
    "  float r1 = u_radial1.z;\n"
    "  vec2 fd = e - f;\n"
    "  if(dot(fd, fd) < 0.0001) {\n"
    "    return (length(p - f) - r0) / max(r1 - r0, 0.0001);\n"
    "  }\n"
    "  float dr = r1 - r0;\n"
    "  float len_fd = length(fd);\n"
    "  float inv = 1.0 / max(len_fd * len_fd - dr * dr, 0.0001);\n"
    "  vec2 q = p - f;\n"
    "  float b = dot(q, fd) / len_fd;\n"
    "  float c = dot(q, q) - r0 * r0;\n"
    "  float disc = b * b - c * inv * len_fd * len_fd;\n"
    "  if(disc < 0.0) return 0.0;\n"
    "  float s = sqrt(disc);\n"
    "  float w = (b - s) * len_fd * inv;\n"
    "  return w;\n"
    "}\n"
    "float conical_t(vec2 p) {\n"
    "  float a = atan(p.y - u_conical_center.y, p.x - u_conical_center.x);\n"
    "  return (a - u_conical_angles.x) / u_conical_angles.y;\n"
    "}\n"
    "float compute_t(vec2 p) {\n"
    "  if(u_dir == 1) return (p.y - u_linear.y) / max(u_linear.w - u_linear.y, 0.0001);\n"
    "  if(u_dir == 2) return (p.x - u_linear.x) / max(u_linear.z - u_linear.x, 0.0001);\n"
    "  if(u_dir == 3) return linear_t(p, u_linear);\n"
    "  if(u_dir == 4) return radial_t(p);\n"
    "  if(u_dir == 5) return conical_t(p);\n"
    "  return 0.0;\n"
    "}\n"
    "vec4 sample_grad(float t) {\n"
    "  t = extend_t(t);\n"
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
    "void main(void) {\n"
    "  if(u_radius > 0.0) {\n"
    "    vec2 half_size = u_rect.zw * 0.5;\n"
    "    vec2 c = u_rect.xy + half_size;\n"
    "    vec2 q = abs(v_pos - c) - half_size + vec2(u_radius);\n"
    "    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - u_radius;\n"
    "    if(d > 0.0) discard;\n"
    "  }\n"
    "  vec4 c = sample_grad(compute_t(v_pos));\n"
    "  c.rgb *= c.a;\n"
    "  gl_FragColor = c;\n"
    "}\n";

static const char g100_tex_vert_src[] =
    "precision mediump float;\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "uniform mat3 u_matrix;\n"
    "uniform vec2 u_view_size;\n"
    "varying vec2 v_uv;\n"
    "void main(void) {\n"
    "  v_uv = a_uv;\n"
    "  vec3 p = u_matrix * vec3(a_pos, 1.0);\n"
    "  gl_Position = vec4(2.0 * p.x / u_view_size.x - 1.0,\n"
    "                     1.0 - 2.0 * p.y / u_view_size.y, 0.0, 1.0);\n"
    "}\n";

static const char g100_tex_frag_src[] =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_opa;\n"
    "uniform vec4 u_recolor;\n"
    "uniform float u_recolor_opa;\n"
    "void main(void) {\n"
    "  vec4 c = texture2D(u_texture, v_uv);\n"
    "  c.a *= u_opa;\n"
    "  if(u_recolor_opa > 0.001) {\n"
    "    c.rgb = mix(c.rgb, u_recolor.rgb, u_recolor_opa);\n"
    "  }\n"
    "  c.rgb *= c.a;\n"
    "  gl_FragColor = c;\n"
    "}\n";

#endif /*LV_G100_SHADER_HAS_GL*/

/**********************
 *  STATIC PROTOTYPES
 **********************/

#if LV_G100_SHADER_HAS_GL
static GLuint compile_shader(GLenum type, const char * src);
static GLuint link_program(GLuint vert, GLuint frag);
static void delete_program(GLuint * program);
#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_g100_shader_init(lv_draw_g100_unit_t * unit, lv_g100_context_t * ctx, lv_g100_shader_t * shader)
{
    LV_ASSERT_NULL(unit);
    LV_ASSERT_NULL(ctx);
    LV_ASSERT_NULL(shader);

    lv_memzero(shader, sizeof(*shader));

#if LV_G100_SHADER_HAS_GL
    GLuint vert = compile_shader(GL_VERTEX_SHADER, g100_vert_src);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, g100_frag_src);
    if(vert == 0 || frag == 0) {
        if(vert) glDeleteShader(vert);
        if(frag) glDeleteShader(frag);
        LV_LOG_WARN("G100 shader: compile failed");
        return;
    }

    shader->solid_program = link_program(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    if(shader->solid_program == 0) {
        LV_LOG_WARN("G100 shader: link failed");
        return;
    }

    shader->solid_loc_matrix = glGetUniformLocation(shader->solid_program, "u_matrix");
    shader->solid_loc_color = glGetUniformLocation(shader->solid_program, "u_color");

    GLuint gvert = compile_shader(GL_VERTEX_SHADER, g100_grad_vert_src);
    GLuint gfrag = compile_shader(GL_FRAGMENT_SHADER, g100_grad_frag_src);
    if(gvert && gfrag) {
        shader->grad_program = link_program(gvert, gfrag);
        glDeleteShader(gvert);
        glDeleteShader(gfrag);
        if(shader->grad_program) {
            shader->grad_ready = true;
            LV_LOG_INFO("G100 native grad shader ready (program=%u)", (unsigned)shader->grad_program);
        }
    }
    else {
        if(gvert) glDeleteShader(gvert);
        if(gfrag) glDeleteShader(gfrag);
        LV_LOG_WARN("G100 grad shader: compile failed");
    }

    GLuint tvert = compile_shader(GL_VERTEX_SHADER, g100_tex_vert_src);
    GLuint tfrag = compile_shader(GL_FRAGMENT_SHADER, g100_tex_frag_src);
    if(tvert && tfrag) {
        shader->tex_program = glCreateProgram();
        glAttachShader(shader->tex_program, tvert);
        glAttachShader(shader->tex_program, tfrag);
        glBindAttribLocation(shader->tex_program, 0, "a_pos");
        glBindAttribLocation(shader->tex_program, 1, "a_uv");
        glLinkProgram(shader->tex_program);
        glDeleteShader(tvert);
        glDeleteShader(tfrag);

        GLint ok = 0;
        glGetProgramiv(shader->tex_program, GL_LINK_STATUS, &ok);
        if(ok) {
            shader->tex_ready = true;
            LV_LOG_INFO("G100 native tex shader ready (program=%u)", (unsigned)shader->tex_program);
        }
        else {
            char log[256];
            GLsizei len = 0;
            glGetProgramInfoLog(shader->tex_program, (GLsizei)sizeof(log), &len, log);
            LV_LOG_WARN("G100 tex shader link: %s", log);
            delete_program(&shader->tex_program);
        }
    }
    else {
        if(tvert) glDeleteShader(tvert);
        if(tfrag) glDeleteShader(tfrag);
        LV_LOG_WARN("G100 tex shader: compile failed");
    }

    /* Self-test: bind once so apitrace / logs can confirm native path */
    glUseProgram(shader->solid_program);
    lv_g100_context_set_bound_program(ctx, shader->solid_program);
    glUseProgram(0);
    lv_g100_context_set_bound_program(ctx, 0);

    shader->ready = true;
    LV_LOG_INFO("G100 native shader ready (program=%u)", (unsigned)shader->solid_program);
#else
    LV_UNUSED(unit);
    LV_UNUSED(ctx);
    LV_LOG_INFO("G100 native shader skipped (no EGL GLES2)");
#endif
}

void lv_g100_shader_deinit(lv_g100_shader_t * shader)
{
    if(!shader) return;

#if LV_G100_SHADER_HAS_GL
    delete_program(&shader->solid_program);
    delete_program(&shader->grad_program);
    delete_program(&shader->tex_program);
#endif

    lv_memzero(shader, sizeof(*shader));
}

bool lv_g100_shader_is_ready(const lv_g100_shader_t * shader)
{
    return shader && shader->ready && shader->solid_program != 0;
}

bool lv_g100_shader_bind_solid(lv_g100_context_t * ctx, lv_g100_shader_t * shader)
{
    if(!lv_g100_shader_is_ready(shader)) return false;

#if LV_G100_SHADER_HAS_GL
    glUseProgram(shader->solid_program);
    if(ctx) lv_g100_context_set_bound_program(ctx, shader->solid_program);
    return true;
#else
    LV_UNUSED(ctx);
    return false;
#endif
}

void lv_g100_shader_unbind(lv_g100_context_t * ctx)
{
#if LV_G100_SHADER_HAS_GL
    glUseProgram(0);
    if(ctx) lv_g100_context_set_bound_program(ctx, 0);
#else
    LV_UNUSED(ctx);
#endif
}

uint32_t lv_g100_shader_get_solid_program(const lv_g100_shader_t * shader)
{
    if(!shader) return 0;
    return shader->solid_program;
}

bool lv_g100_shader_grad_is_ready(const lv_g100_shader_t * shader)
{
    return shader && shader->grad_ready && shader->grad_program != 0;
}

bool lv_g100_shader_bind_grad(lv_g100_context_t * ctx, lv_g100_shader_t * shader)
{
    if(!lv_g100_shader_grad_is_ready(shader)) return false;

#if LV_G100_SHADER_HAS_GL
    glUseProgram(shader->grad_program);
    if(ctx) lv_g100_context_set_bound_program(ctx, shader->grad_program);
    return true;
#else
    LV_UNUSED(ctx);
    return false;
#endif
}

bool lv_g100_shader_tex_is_ready(const lv_g100_shader_t * shader)
{
    return shader && shader->tex_ready && shader->tex_program != 0;
}

bool lv_g100_shader_bind_tex(lv_g100_context_t * ctx, lv_g100_shader_t * shader)
{
    if(!lv_g100_shader_tex_is_ready(shader)) return false;

#if LV_G100_SHADER_HAS_GL
    glUseProgram(shader->tex_program);
    if(ctx) lv_g100_context_set_bound_program(ctx, shader->tex_program);
    return true;
#else
    LV_UNUSED(ctx);
    return false;
#endif
}

#if LV_G100_SHADER_HAS_GL

static GLuint compile_shader(GLenum type, const char * src)
{
    GLuint sh = glCreateShader(type);
    if(sh == 0) return 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        char log[256];
        GLsizei len = 0;
        glGetShaderInfoLog(sh, (GLsizei)sizeof(log), &len, log);
        LV_LOG_WARN("G100 shader compile: %s", log);
        glDeleteShader(sh);
        return 0;
    }

    return sh;
}

static GLuint link_program(GLuint vert, GLuint frag)
{
    GLuint prog = glCreateProgram();
    if(prog == 0) return 0;

    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glBindAttribLocation(prog, 0, "a_pos");
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok) {
        char log[256];
        GLsizei len = 0;
        glGetProgramInfoLog(prog, (GLsizei)sizeof(log), &len, log);
        LV_LOG_WARN("G100 shader link: %s", log);
        glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

static void delete_program(GLuint * program)
{
    if(!program || *program == 0) return;
    glDeleteProgram(*program);
    *program = 0;
}

#endif /*LV_G100_SHADER_HAS_GL*/

#endif /*LV_USE_DRAW_G100*/
