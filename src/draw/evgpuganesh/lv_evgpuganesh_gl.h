#ifndef LV_EVGPUGANESH_GL_H
#define LV_EVGPUGANESH_GL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPUGANESH

#include <GLES2/gl2.h>

#define EVGPUGANESH_UNIT_ID 12

typedef struct {
    GLuint solid_prog;
    GLint solid_u_color;
    GLint solid_u_alpha;
    GLint solid_u_proj;

    GLuint tex_prog;
    GLint tex_u_texture;
    GLint tex_u_recolor;
    GLint tex_u_recolor_opa;
    GLint tex_u_alpha;
    GLint tex_u_proj;

    GLuint blur_prog;
    GLint blur_u_texture;
    GLint blur_u_dir;
    GLint blur_u_radius;
    GLint blur_u_size;
    GLint blur_u_proj;

    GLuint grad_prog;
    GLint grad_u_color1;
    GLint grad_u_color2;
    GLint grad_u_dir;
    GLint grad_u_proj;

    GLuint vbo;
    GLuint vao;
} lv_evgpuganesh_gl_t;

void lv_evgpuganesh_gl_init(lv_evgpuganesh_gl_t * gl);
void lv_evgpuganesh_gl_deinit(lv_evgpuganesh_gl_t * gl);

void lv_evgpuganesh_gl_set_projection(lv_evgpuganesh_gl_t * gl, int32_t w, int32_t h);

void lv_evgpuganesh_gl_draw_quad_solid(lv_evgpuganesh_gl_t * gl,
                                       float x1, float y1, float x2, float y2,
                                       uint32_t color, uint8_t alpha);

void lv_evgpuganesh_gl_draw_quad_tex(lv_evgpuganesh_gl_t * gl,
                                     float x1, float y1, float x2, float y2,
                                     float u1, float v1, float u2, float v2,
                                     GLuint texture, uint32_t recolor, uint8_t recolor_opa, uint8_t alpha);

void lv_evgpuganesh_gl_draw_quad_grad(lv_evgpuganesh_gl_t * gl,
                                      float x1, float y1, float x2, float y2,
                                      uint32_t color1, uint32_t color2, int dir);

void lv_evgpuganesh_gl_draw_triangles_solid(lv_evgpuganesh_gl_t * gl,
                                            const float * verts, int count,
                                            uint32_t color, uint8_t alpha);

void lv_evgpuganesh_gl_draw_triangle_strip_solid(lv_evgpuganesh_gl_t * gl,
                                                  const float * verts, int count,
                                                  uint32_t color, uint8_t alpha);

void lv_evgpuganesh_gl_set_scissor(int32_t x, int32_t y, int32_t w, int32_t h);
void lv_evgpuganesh_gl_disable_scissor(void);

void lv_evgpuganesh_gl_clear(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

static inline uint32_t lv_evgpuganesh_color_to_gl(lv_color_t c) {
    return (uint32_t)c.red << 0 | (uint32_t)c.green << 8 | (uint32_t)c.blue << 16 | 0xFF000000;
}

static inline uint32_t lv_evgpuganesh_color_to_gl_alpha(lv_color_t c, lv_opa_t opa) {
    uint32_t a = opa;
    return ((uint32_t)c.blue << 16) | ((uint32_t)c.green << 8) | (uint32_t)c.red | (a << 24);
}

#endif

#ifdef __cplusplus
}
#endif

#endif
