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
    "attribute vec2 a_pos;\n"
    "uniform mat3 u_matrix;\n"
    "void main(void) {\n"
    "  vec3 p = u_matrix * vec3(a_pos, 1.0);\n"
    "  gl_Position = vec4(p.xy, 0.0, 1.0);\n"
    "}\n";

static const char g100_frag_src[] =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "void main(void) {\n"
    "  gl_FragColor = u_color;\n"
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
