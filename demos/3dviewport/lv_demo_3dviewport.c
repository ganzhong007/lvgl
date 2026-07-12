/**
 * @file lv_demo_3dviewport.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_demo_3dviewport.h"

#if LV_USE_DEMO_3DVIEWPORT

#if LV_USE_OPENGLES
#include "../../src/drivers/opengles/lv_opengles_private.h"
#include "../../src/drivers/opengles/lv_opengles_debug.h"
#endif

/*********************
 *      DEFINES
 *********************/

static const char * tri_vert =
    "attribute vec3 a_pos;\n"
    "uniform mat4 u_mvp;\n"
    "void main() { gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char * tri_frag =
    "precision mediump float;\n"
    "void main() { gl_FragColor = vec4(0.95, 0.45, 0.15, 1.0); }\n";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void demo_render_cb(lv_layer_t * pass_layer, const lv_3d_camera_t * cam,
                             const float * view_proj, void * user_data);

static bool demo_tri_init(void);

/**********************
 *  STATIC VARIABLES
 **********************/

static uint32_t s_tri_prog;
static int32_t s_tri_loc_mvp;
static uint32_t s_tri_vbo;
static bool s_tri_ready;

static const float s_tri_verts[] = {
    0.f, 0.4f, 0.f,
    -0.5f, -0.3f, 0.2f,
    0.5f, -0.3f, -0.2f,
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_demo_3dviewport(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), LV_PART_MAIN);

    lv_obj_t * panel = lv_obj_create(scr);
    lv_obj_set_size(panel, 120, 60);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, 16, 16);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x3949AB), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN);

    lv_obj_t * panel_label = lv_label_create(panel);
    lv_label_set_text(panel_label, "2D FILL");
    lv_obj_center(panel_label);

    lv_obj_t * vp = lv_3dviewport_create(scr);
    lv_obj_set_size(vp, 420, 280);
    lv_obj_center(vp);
    lv_3dviewport_set_clear_color(vp, lv_color_hex(0x121212), LV_OPA_COVER);
    lv_3dviewport_set_grid_visible(vp, true);
    lv_3dviewport_set_render_cb(vp, demo_render_cb, NULL);

    lv_obj_t * label = lv_label_create(scr);
    lv_label_set_text(label, "G8.1 grid + callback + orbit drag (D3-07/08)");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 12);

    return vp;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void demo_render_cb(lv_layer_t * pass_layer, const lv_3d_camera_t * cam,
                             const float * view_proj, void * user_data)
{
    LV_UNUSED(pass_layer);
    LV_UNUSED(cam);
    LV_UNUSED(user_data);

#if LV_USE_OPENGLES
    if(!demo_tri_init() || view_proj == NULL) return;

    GL_CALL(glUseProgram(s_tri_prog));
    GL_CALL(glUniformMatrix4fv(s_tri_loc_mvp, 1, GL_FALSE, view_proj));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_tri_vbo));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 3));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CALL(glUseProgram(0));
#else
    LV_UNUSED(view_proj);
#endif
}

static bool demo_tri_init(void)
{
#if LV_USE_OPENGLES
    if(s_tri_ready) return true;

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    if(!v || !f) return false;

    glShaderSource(v, 1, &tri_vert, NULL);
    glShaderSource(f, 1, &tri_frag, NULL);
    glCompileShader(v);
    glCompileShader(f);

    s_tri_prog = glCreateProgram();
    glAttachShader(s_tri_prog, v);
    glAttachShader(s_tri_prog, f);
    glBindAttribLocation(s_tri_prog, 0, "a_pos");
    glLinkProgram(s_tri_prog);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(s_tri_prog, GL_LINK_STATUS, &ok);
    if(!ok) return false;

    s_tri_loc_mvp = glGetUniformLocation(s_tri_prog, "u_mvp");
    GL_CALL(glGenBuffers(1, &s_tri_vbo));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, s_tri_vbo));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(s_tri_verts), s_tri_verts, GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    s_tri_ready = s_tri_loc_mvp >= 0;
    return s_tri_ready;
#else
    return false;
#endif
}

#endif /*LV_USE_DEMO_3DVIEWPORT*/
