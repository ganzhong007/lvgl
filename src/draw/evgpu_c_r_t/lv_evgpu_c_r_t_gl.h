#ifndef LV_EVGPU_C_R_T_GL_H
#define LV_EVGPU_C_R_T_GL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU_C_R_T

#ifndef LV_EVGPU_C_R_T_SKIP_SYSTEM_GLES
#include <GLES2/gl2.h>
#endif

#define EVGPU_C_R_T_UNIT_ID 13

#define EVGPU_C_R_T_ARENA_SIZE (256 * 1024)
#define EVGPU_C_R_T_SOLID_BATCH_MAX 4096
#define EVGPU_C_R_T_GRAD_CACHE_MAX 8

typedef struct {
    GLuint prog;
    GLuint tex;
    GLenum blend_src;
    GLenum blend_dst;
    bool scissor_enabled;
    GLint scissor_box[4];
} evgpu_c_r_t_gl_state_t;

typedef struct {
    uint8_t * buf;
    size_t cap;
    size_t off;
} evgpu_c_r_t_arena_t;

typedef struct {
    bool active;
    uint32_t color;
    uint8_t alpha;
    int quad_count;
} evgpu_c_r_t_solid_batch_t;

typedef struct {
    uint32_t key;
    GLuint tex;
    int w;
} evgpu_c_r_t_grad_cache_t;

typedef struct {
    GLuint solid_prog;
    GLint solid_u_color;
    GLint solid_u_alpha;
    GLint solid_u_proj;

    GLuint round_prog;
    GLint round_u_color;
    GLint round_u_proj;
    GLint round_u_rect;
    GLint round_u_radius;
    GLint round_u_border;

    GLuint radial_prog;
    GLint radial_u_proj;
    GLint radial_u_rect;
    GLint radial_u_radius;
    GLint radial_u_center;
    GLint radial_u_radii;
    GLint radial_u_stops_count;
    GLint radial_u_stop_color;
    GLint radial_u_stop_frac;

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

    GLuint grad_tex_prog;
    GLint grad_tex_u_proj;
    GLint grad_tex_u_tex;
    GLint grad_tex_u_dir;

    GLuint vbo;

    evgpu_c_r_t_gl_state_t state;
    evgpu_c_r_t_arena_t arena;
    evgpu_c_r_t_solid_batch_t batch;
    evgpu_c_r_t_grad_cache_t grad_cache[EVGPU_C_R_T_GRAD_CACHE_MAX];
    int grad_cache_count;
    float proj[16];
    int32_t view_w, view_h;
} lv_evgpu_c_r_t_gl_t;

void lv_evgpu_c_r_t_gl_init(lv_evgpu_c_r_t_gl_t * gl);
void lv_evgpu_c_r_t_gl_deinit(lv_evgpu_c_r_t_gl_t * gl);
void lv_evgpu_c_r_t_gl_flush(lv_evgpu_c_r_t_gl_t * gl);

void lv_evgpu_c_r_t_gl_set_projection(lv_evgpu_c_r_t_gl_t * gl, int32_t w, int32_t h);

void lv_evgpu_c_r_t_gl_draw_quad_solid(lv_evgpu_c_r_t_gl_t * gl,
                                       float x1, float y1, float x2, float y2,
                                       uint32_t color, uint8_t alpha);

/** Rounded fill (border_w=0) or rounded border ring (border_w>0). */
void lv_evgpu_c_r_t_gl_draw_round_rect(lv_evgpu_c_r_t_gl_t * gl,
                                       float x1, float y1, float x2, float y2,
                                       float radius, float border_w,
                                       uint32_t color, uint8_t alpha);

/** Multi-stop radial gradient (concentric), clipped to rounded/circle rect. */
#define EVGPU_C_R_T_RADIAL_MAX_STOPS 8
void lv_evgpu_c_r_t_gl_draw_radial_grad(lv_evgpu_c_r_t_gl_t * gl,
                                        float x1, float y1, float x2, float y2,
                                        float clip_radius,
                                        float cx, float cy, float r0, float r1,
                                        const float * stop_rgba, /* stops_count * 4 */
                                        const float * stop_fracs,
                                        int stops_count);

void lv_evgpu_c_r_t_gl_draw_quad_tex(lv_evgpu_c_r_t_gl_t * gl,
                                     float x1, float y1, float x2, float y2,
                                     float u1, float v1, float u2, float v2,
                                     GLuint texture, uint32_t recolor, uint8_t recolor_opa, uint8_t alpha);

void lv_evgpu_c_r_t_gl_draw_quad_grad(lv_evgpu_c_r_t_gl_t * gl,
                                      float x1, float y1, float x2, float y2,
                                      uint32_t color1, uint32_t color2, int dir);

void lv_evgpu_c_r_t_gl_draw_triangles_solid(lv_evgpu_c_r_t_gl_t * gl,
                                            const float * verts, int count,
                                            uint32_t color, uint8_t alpha);

void lv_evgpu_c_r_t_gl_draw_triangle_strip_solid(lv_evgpu_c_r_t_gl_t * gl,
                                                  const float * verts, int count,
                                                  uint32_t color, uint8_t alpha);

void lv_evgpu_c_r_t_gl_blur_quad(lv_evgpu_c_r_t_gl_t * gl,
                                  float x1, float y1, float x2, float y2,
                                  float u1, float v1, float u2, float v2,
                                  GLuint texture, float dir_x, float dir_y,
                                  float radius, float size_w, float size_h);

/** Set GL scissor from LVGL top-left coords (flips Y using gl->view_h). */
void lv_evgpu_c_r_t_gl_set_scissor(lv_evgpu_c_r_t_gl_t * gl, int32_t x, int32_t y, int32_t w, int32_t h);
void lv_evgpu_c_r_t_gl_disable_scissor(void);

void lv_evgpu_c_r_t_gl_clear(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

/** Pack as 0xAARRGGBB to match shader unpack (R>>16, G>>8, B>>0). */
static inline uint32_t lv_evgpu_c_r_t_color_to_gl(lv_color_t c) {
    return ((uint32_t)c.red << 16) | ((uint32_t)c.green << 8) | (uint32_t)c.blue | 0xFF000000u;
}

static inline uint32_t lv_evgpu_c_r_t_color_to_gl_alpha(lv_color_t c, lv_opa_t opa) {
    return ((uint32_t)opa << 24) | ((uint32_t)c.red << 16) | ((uint32_t)c.green << 8) | (uint32_t)c.blue;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
