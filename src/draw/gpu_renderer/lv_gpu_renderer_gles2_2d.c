/**
 * @file lv_gpu_renderer_gles2_2d.c — GLES2 2D overlay batch [GL2]
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "lv_gpu_renderer_gles2_2d.h"
#include "lv_draw_gpu_renderer.h"

#include "../../draw/sw/lv_draw_sw.h"
#include "../lv_draw_private.h"
#include "../../core/lv_refr_private.h"
#include "../../display/lv_display_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../include/lvgl/draw/lv_draw_label.h"
#include "../../include/lvgl/draw/lv_draw_image.h"
#include "../../include/lvgl/draw/lv_draw_rect.h"
#include "../../misc/lv_color.h"
#include "../../misc/lv_area_private.h"
#include <math.h>
#include <string.h>

#define LV_GPU2D_QUEUE_MAX 512

typedef lv_gpu_renderer_gles2_cmd_t lv_gpu2d_cmd_t;

static lv_gpu2d_cmd_t g_queue[LV_GPU2D_QUEUE_MAX];
static uint32_t g_queue_count;
static uint32_t g_raster_nest;

static unsigned int prog_fill;
static int loc_fill_disp;
static int loc_fill_color;
static int loc_fill_rect;
static int loc_fill_radius;

static unsigned int prog_tex;
static int loc_tex_disp;
static int loc_tex_sampler;

static lv_draw_buf_t g_raster_buf;

static bool shader_ok(unsigned int sh)
{
    int ok = 0;
    GL_CALL(glGetShaderiv(sh, GL_COMPILE_STATUS, &ok));
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
        GL_CALL(glDeleteProgram(p));
        return 0;
    }
    return p;
}

#if LV_USE_EGL
static const char * vs_fill =
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_p;\n"
    "void main(){ v_p=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * fs_fill =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 h=u_rect.zw*0.5;\n"
    "  vec2 c=u_rect.xy+h;\n"
    "  vec2 q=abs(v_p-c)-h+u_radius;\n"
    "  float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-u_radius;\n"
    "  float a=u_color.a*(1.0-smoothstep(-1.0,1.0,d));\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(u_color.rgb,a);\n"
    "}\n";
static const char * vs_tex =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_uv;\n"
    "void main(){ v_uv=a_uv; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * fs_tex =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main(){ gl_FragColor=texture2D(u_tex,v_uv);}\n";
#else
static const char * vs_fill =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_p;\n"
    "void main(){ v_p=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * fs_fill =
    "#version 120\n"
    "uniform vec4 u_color;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 h=u_rect.zw*0.5;\n"
    "  vec2 c=u_rect.xy+h;\n"
    "  vec2 q=abs(v_p-c)-h+u_radius;\n"
    "  float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-u_radius;\n"
    "  float a=u_color.a*(1.0-smoothstep(-1.0,1.0,d));\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(u_color.rgb,a);\n"
    "}\n";
static const char * vs_tex =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_uv;\n"
    "void main(){ v_uv=a_uv; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * fs_tex =
    "#version 120\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main(){ gl_FragColor=texture2D(u_tex,v_uv);}\n";
#endif

void lv_gpu_renderer_gles2_2d_init(void)
{
    if(prog_fill) return;
    prog_fill = link_program(vs_fill, fs_fill);
    if(prog_fill) {
        loc_fill_disp = glGetUniformLocation(prog_fill, "u_disp");
        loc_fill_color = glGetUniformLocation(prog_fill, "u_color");
        loc_fill_rect = glGetUniformLocation(prog_fill, "u_rect");
        loc_fill_radius = glGetUniformLocation(prog_fill, "u_radius");
    }
    prog_tex = link_program(vs_tex, fs_tex);
    if(prog_tex) {
        loc_tex_disp = glGetUniformLocation(prog_tex, "u_disp");
        loc_tex_sampler = glGetUniformLocation(prog_tex, "u_tex");
    }
    lv_draw_buf_init(&g_raster_buf, 0, 0, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO, NULL, 0);
}

void lv_gpu_renderer_gles2_2d_deinit(void)
{
    if(prog_fill) GL_CALL(glDeleteProgram(prog_fill));
    if(prog_tex) GL_CALL(glDeleteProgram(prog_tex));
    prog_fill = prog_tex = 0;
    if(g_raster_buf.unaligned_data) lv_free(g_raster_buf.unaligned_data);
    lv_memzero(&g_raster_buf, sizeof(g_raster_buf));
    g_queue_count = 0;
}

void lv_gpu_renderer_gles2_2d_queue_reset(void)
{
    g_queue_count = 0;
}

uint32_t lv_gpu_renderer_gles2_2d_queue_count(void)
{
    return g_queue_count;
}

static bool gpu2d_cmd_uses_fill_shader(lv_gpu_renderer_gles2_cmd_type_t type)
{
    return type == LV_GPU_RENDERER_GLES2_CMD_FILL || type == LV_GPU_RENDERER_GLES2_CMD_BORDER;
}

uint32_t lv_gpu_renderer_gles2_2d_count_shader_batches(void)
{
    if(g_queue_count == 0) return 0;

    uint32_t batches = 0;
    bool last_fill = true;
    for(uint32_t i = 0; i < g_queue_count; i++) {
        bool fill = gpu2d_cmd_uses_fill_shader(g_queue[i].type);
        if(i == 0 || fill != last_fill) batches++;
        last_fill = fill;
    }
    return batches > 0 ? batches : 1;
}

bool lv_gpu_renderer_gles2_2d_is_raster_nest(void)
{
    return g_raster_nest > 0;
}

static bool queue_push(const lv_gpu2d_cmd_t * cmd)
{
    if(g_queue_count >= LV_GPU2D_QUEUE_MAX) return false;
    g_queue[g_queue_count++] = *cmd;
    return true;
}

bool lv_gpu_renderer_gles2_2d_queue_fill(const lv_area_t * area, const lv_area_t * clip,
                                          lv_color_t color, lv_opa_t opa, int32_t radius)
{
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_FILL;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.fill.color = color;
    cmd.u.fill.opa = opa;
    cmd.u.fill.radius = radius;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_border(const lv_area_t * area, const lv_area_t * clip,
                                            lv_color_t color, lv_opa_t opa, int32_t width,
                                            int32_t radius, lv_border_side_t side)
{
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_BORDER;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.border.color = color;
    cmd.u.border.opa = opa;
    cmd.u.border.width = width;
    cmd.u.border.radius = radius;
    cmd.u.border.side = side;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_label(const lv_area_t * area, const lv_area_t * clip,
                                             const lv_draw_label_dsc_t * dsc)
{
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_LABEL;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.label = *dsc;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_letter(const lv_area_t * area, const lv_area_t * clip,
                                              const lv_draw_letter_dsc_t * dsc)
{
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_LETTER;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.letter = *dsc;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_image(const lv_area_t * area, const lv_area_t * clip,
                                           const lv_draw_image_dsc_t * dsc)
{
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_IMAGE;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.image = *dsc;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_copy_last_cmd(lv_gpu_renderer_gles2_cmd_t * out)
{
    if(!out || g_queue_count == 0) return false;
    *out = g_queue[g_queue_count - 1];
    return true;
}

static void gpu_comp_uniform_rgba(int loc, lv_color32_t c32)
{
    GL_CALL(glUniform4f(loc, c32.blue / 255.0f, c32.green / 255.0f, c32.red / 255.0f,
                        c32.alpha / 255.0f));
}

static void apply_scissor(const lv_area_t * clip, int32_t dh)
{
    int32_t w = clip->x2 - clip->x1 + 1;
    int32_t h = clip->y2 - clip->y1 + 1;
    int32_t y = dh - clip->y2 - 1;
    if(w < 0) w = 0;
    if(h < 0) h = 0;
    GL_CALL(glEnable(GL_SCISSOR_TEST));
    GL_CALL(glScissor(clip->x1, y, w, h));
}

static void draw_fill_quad(const lv_area_t * area, int32_t dw, int32_t dh,
                           lv_color_t color, lv_opa_t opa, int32_t radius)
{
    if(!prog_fill) return;
    lv_color32_t c32 = lv_color_to_32(color, opa);
    float x1 = (float)area->x1;
    float y1 = (float)area->y1;
    float x2 = (float)area->x2 + 1.0f;
    float y2 = (float)area->y2 + 1.0f;
    float verts[] = {
        x1, y1,  x2, y1,  x2, y2,
        x1, y1,  x2, y2,  x1, y2,
    };
    float rw = x2 - x1;
    float rh = y2 - y1;
    float rad = (float)radius;
    if(rad > rw * 0.5f) rad = rw * 0.5f;
    if(rad > rh * 0.5f) rad = rh * 0.5f;

    GL_CALL(glUseProgram(prog_fill));
    GL_CALL(glUniform2f(loc_fill_disp, (float)dw, (float)dh));
    gpu_comp_uniform_rgba(loc_fill_color, c32);
    GL_CALL(glUniform4f(loc_fill_rect, x1, y1, rw, rh));
    GL_CALL(glUniform1f(loc_fill_radius, rad));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
}

static void draw_border_gpu(const lv_gpu2d_cmd_t * cmd, int32_t dw, int32_t dh)
{
    const lv_area_t * a = &cmd->area;
    int32_t w = cmd->u.border.width;
    if(w < 1) return;
    lv_border_side_t side = cmd->u.border.side;
    lv_area_t inner = *a;
    lv_area_t outer = *a;

    if(side & LV_BORDER_SIDE_TOP) {
        lv_area_t r = { outer.x1, outer.y1, outer.x2, outer.y1 + w - 1 };
        draw_fill_quad(&r, dw, dh, cmd->u.border.color, cmd->u.border.opa, cmd->u.border.radius);
    }
    if(side & LV_BORDER_SIDE_BOTTOM) {
        lv_area_t r = { outer.x1, outer.y2 - w + 1, outer.x2, outer.y2 };
        draw_fill_quad(&r, dw, dh, cmd->u.border.color, cmd->u.border.opa, cmd->u.border.radius);
    }
    if(side & LV_BORDER_SIDE_LEFT) {
        lv_area_t r = { outer.x1, inner.y1 + w, outer.x1 + w - 1, inner.y2 - w };
        draw_fill_quad(&r, dw, dh, cmd->u.border.color, cmd->u.border.opa, 0);
    }
    if(side & LV_BORDER_SIDE_RIGHT) {
        lv_area_t r = { outer.x2 - w + 1, inner.y1 + w, outer.x2, inner.y2 - w };
        draw_fill_quad(&r, dw, dh, cmd->u.border.color, cmd->u.border.opa, 0);
    }
}

static bool raster_sw_to_texture(const lv_area_t * area, const lv_area_t * clip,
                                 void (*draw_fn)(lv_layer_t *, void *, const lv_area_t *),
                                 void * dsc, unsigned int * tex_out)
{
    int32_t rw = lv_area_get_width(area);
    int32_t rh = lv_area_get_height(area);
    if(rw < 1 || rh < 1) return false;

    if(NULL == lv_draw_buf_reshape(&g_raster_buf, LV_COLOR_FORMAT_ARGB8888, rw, rh, LV_STRIDE_AUTO)) {
        uint32_t sz = LV_DRAW_BUF_SIZE(rw, rh, LV_COLOR_FORMAT_ARGB8888);
        uint8_t * mem = lv_realloc(g_raster_buf.unaligned_data, sz);
        if(!mem) return false;
        lv_draw_buf_init(&g_raster_buf, rw, rh, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO, mem, sz);
    }
    lv_memzero(g_raster_buf.data, g_raster_buf.data_size);

    lv_layer_t layer;
    lv_layer_init(&layer);
    layer.draw_buf = &g_raster_buf;
    layer.color_format = LV_COLOR_FORMAT_ARGB8888;
    layer.buf_area.x1 = 0;
    layer.buf_area.y1 = 0;
    layer.buf_area.x2 = rw - 1;
    layer.buf_area.y2 = rh - 1;
    lv_area_t rel_clip = *clip;
    if(!lv_area_intersect(&rel_clip, &rel_clip, area)) return false;
    rel_clip.x1 -= area->x1;
    rel_clip.y1 -= area->y1;
    rel_clip.x2 -= area->x1;
    rel_clip.y2 -= area->y1;
    layer._clip_area = rel_clip;
    layer.phy_clip_area = rel_clip;

    lv_display_t * disp = lv_refr_get_disp_refreshing();
    if(!disp) return false;

    g_raster_nest++;
    lv_area_t rel = { 0, 0, rw - 1, rh - 1 };
    draw_fn(&layer, dsc, &rel);

    layer.all_tasks_added = true;
    while(layer.draw_task_head) {
        lv_draw_dispatch_layer(disp, &layer);
        if(layer.draw_task_head) {
            lv_draw_dispatch_wait_for_request();
        }
    }
    g_raster_nest--;

    unsigned int tex = 0;
    GL_CALL(glGenTextures(1, &tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    lv_opengles_teximage_bgra8888(0, rw, rh, g_raster_buf.data, g_raster_buf.header.stride);
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    *tex_out = tex;
    return true;
}

static void draw_label_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    lv_draw_label(layer, dsc, rel);
}

static void draw_letter_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    lv_point_t pt = { rel->x1, rel->y1 };
    lv_draw_letter(layer, dsc, &pt);
}

static void draw_image_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    lv_draw_image(layer, dsc, rel);
}

static void draw_textured_quad(const lv_area_t * area, int32_t dw, int32_t dh, unsigned int tex)
{
    if(!prog_tex || tex == 0) return;
    float x1 = (float)area->x1;
    float y1 = (float)area->y1;
    float x2 = (float)area->x2 + 1.0f;
    float y2 = (float)area->y2 + 1.0f;
    float pos[] = {
        x1, y1,  x2, y1,  x2, y2,
        x1, y1,  x2, y2,  x1, y2,
    };
    float uv[] = {
        0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
    };
    GL_CALL(glUseProgram(prog_tex));
    GL_CALL(glUniform2f(loc_tex_disp, (float)dw, (float)dh));
    GL_CALL(glActiveTexture(GL_TEXTURE0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glUniform1i(loc_tex_sampler, 0));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glEnableVertexAttribArray(1));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos));
    GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(1));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

uint32_t lv_gpu_renderer_gles2_2d_render_cmd_list(const lv_gpu_renderer_gles2_cmd_t * cmds, uint32_t count,
                                                   unsigned int color_tex, int32_t dw, int32_t dh,
                                                   uint32_t * sw_raster_out)
{
    if(sw_raster_out) *sw_raster_out = 0;
    if(!cmds || count == 0 || color_tex == 0 || dw < 1 || dh < 1) return 0;
    if(!prog_fill && !prog_tex) lv_gpu_renderer_gles2_2d_init();

    if(!lv_gpu_renderer_tex_fbo_bind_complete(color_tex)) {
        lv_gpu_renderer_restore_default_framebuffer();
        return 0;
    }

    GL_CALL(glViewport(0, 0, dw, dh));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    uint32_t rendered = 0;
    uint32_t sw_raster = 0;

    for(uint32_t i = 0; i < count; i++) {
        const lv_gpu_renderer_gles2_cmd_t * cmd = &cmds[i];
        lv_gpu2d_cmd_t * mut_cmd = (lv_gpu2d_cmd_t *)(uintptr_t)cmd;
        lv_area_t draw_area;
        if(!lv_area_intersect(&draw_area, &cmd->area, &cmd->clip)) continue;
        apply_scissor(&cmd->clip, dh);

        switch(cmd->type) {
            case LV_GPU_RENDERER_GLES2_CMD_FILL:
                draw_fill_quad(&draw_area, dw, dh, cmd->u.fill.color, cmd->u.fill.opa, cmd->u.fill.radius);
                rendered++;
                break;
            case LV_GPU_RENDERER_GLES2_CMD_BORDER:
                draw_border_gpu(mut_cmd, dw, dh);
                rendered++;
                break;
            case LV_GPU_RENDERER_GLES2_CMD_LABEL: {
                    unsigned int tex = 0;
                    if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_label_sw_cb,
                                             &cmd->u.label, &tex)) {
                        draw_textured_quad(&draw_area, dw, dh, tex);
                        GL_CALL(glDeleteTextures(1, &tex));
                        sw_raster++;
                        rendered++;
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_LETTER: {
                    unsigned int tex = 0;
                    if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_letter_sw_cb,
                                             &cmd->u.letter, &tex)) {
                        draw_textured_quad(&draw_area, dw, dh, tex);
                        GL_CALL(glDeleteTextures(1, &tex));
                        sw_raster++;
                        rendered++;
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_IMAGE: {
                    unsigned int tex = 0;
                    if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_image_sw_cb,
                                             &cmd->u.image, &tex)) {
                        draw_textured_quad(&draw_area, dw, dh, tex);
                        GL_CALL(glDeleteTextures(1, &tex));
                        sw_raster++;
                        rendered++;
                    }
                }
                break;
            default:
                break;
        }
    }

    GL_CALL(glDisable(GL_SCISSOR_TEST));
    lv_gpu_renderer_restore_default_framebuffer();

    if(sw_raster_out) *sw_raster_out = sw_raster;
    return rendered;
}

uint32_t lv_gpu_renderer_gles2_2d_render_batch(unsigned int color_tex, int32_t dw, int32_t dh,
                                                uint32_t * sw_raster_out)
{
    return lv_gpu_renderer_gles2_2d_render_cmd_list(g_queue, g_queue_count, color_tex, dw, dh, sw_raster_out);
}

#if 0
uint32_t lv_gpu_renderer_gles2_2d_render_batch_legacy(unsigned int color_tex, int32_t dw, int32_t dh,
                                                uint32_t * sw_raster_out)
{
    if(sw_raster_out) *sw_raster_out = 0;
    if(g_queue_count == 0 || color_tex == 0 || dw < 1 || dh < 1) return 0;
    if(!prog_fill && !prog_tex) lv_gpu_renderer_gles2_2d_init();

    unsigned int fbo = 0;
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0));
#if !LV_USE_EGL
    {
        GLenum db = GL_COLOR_ATTACHMENT0;
        GL_CALL(glDrawBuffers(1, &db));
        GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0));
    }
#endif
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDeleteFramebuffers(1, &fbo));
        return 0;
    }

    GL_CALL(glViewport(0, 0, dw, dh));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    uint32_t rendered = 0;
    uint32_t sw_raster = 0;

    for(uint32_t i = 0; i < g_queue_count; i++) {
        lv_gpu2d_cmd_t * cmd = &g_queue[i];
        lv_area_t draw_area;
        if(!lv_area_intersect(&draw_area, &cmd->area, &cmd->clip)) continue;
        apply_scissor(&cmd->clip, dh);

        switch(cmd->type) {
            case LV_GPU_RENDERER_GLES2_CMD_FILL:
                draw_fill_quad(&draw_area, dw, dh, cmd->u.fill.color, cmd->u.fill.opa, cmd->u.fill.radius);
                rendered++;
                break;
            case LV_GPU_RENDERER_GLES2_CMD_BORDER:
                draw_border_gpu(cmd, dw, dh);
                rendered++;
                break;
            case LV_GPU_RENDERER_GLES2_CMD_LABEL: {
                    unsigned int tex = 0;
                    if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_label_sw_cb,
                                             &cmd->u.label, &tex)) {
                        draw_textured_quad(&draw_area, dw, dh, tex);
                        GL_CALL(glDeleteTextures(1, &tex));
                        sw_raster++;
                        rendered++;
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_LETTER: {
                    unsigned int tex = 0;
                    if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_letter_sw_cb,
                                             &cmd->u.letter, &tex)) {
                        draw_textured_quad(&draw_area, dw, dh, tex);
                        GL_CALL(glDeleteTextures(1, &tex));
                        sw_raster++;
                        rendered++;
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_IMAGE: {
                    unsigned int tex = 0;
                    if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_image_sw_cb,
                                             &cmd->u.image, &tex)) {
                        draw_textured_quad(&draw_area, dw, dh, tex);
                        GL_CALL(glDeleteTextures(1, &tex));
                        sw_raster++;
                        rendered++;
                    }
                }
                break;
            default:
                break;
        }
    }

    GL_CALL(glDisable(GL_SCISSOR_TEST));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &fbo));

    if(sw_raster_out) *sw_raster_out = sw_raster;
    return rendered;
}
#endif

#endif /*LV_USE_DRAW_GPU_RENDERER*/
