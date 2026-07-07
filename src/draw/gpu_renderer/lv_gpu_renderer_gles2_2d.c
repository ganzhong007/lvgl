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
#include "lv_gpu_renderer_glyph_atlas.h"

#include "../../draw/sw/lv_draw_sw.h"
#include "../lv_draw_private.h"
#include "../lv_draw_label_private.h"
#include "../lv_draw_image_private.h"
#include "../../draw/lv_image_decoder_private.h"
#include "../../core/lv_refr_private.h"
#include "../../display/lv_display_private.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../include/lvgl/draw/lv_draw_label.h"
#include "../../include/lvgl/draw/lv_draw_image.h"
#include "../../include/lvgl/draw/lv_draw_rect.h"
#include "../../include/lvgl/draw/lv_draw_line.h"
#include "../../include/lvgl/draw/lv_draw_arc.h"
#include "../../include/lvgl/draw/lv_draw_triangle.h"
#include "../../include/lvgl/draw/lv_draw_mask.h"
#include "../../include/lvgl/draw/lv_draw_blur.h"
#include "../../include/lvgl/draw/lv_grad.h"
#include "../sw/blend/lv_draw_sw_blend.h"
#include "../sw/blend/lv_draw_sw_blend_private.h"
#include "../../draw/sw/lv_draw_sw_mask.h"
#if LV_USE_FREETYPE && LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
#include "../../libs/freetype/lv_freetype_private.h"
#include "../../draw/lv_draw_vector_private.h"
#endif
#include "../../include/lvgl/core/lv_matrix.h"
#include "../../misc/lv_color.h"
#include "../../misc/lv_area_private.h"
#include "../../misc/lv_math.h"
#include "../../draw/sw/lv_draw_sw_grad.h"
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
static int loc_fill_color1;
static int loc_fill_rect;
static int loc_fill_radius;
static int loc_fill_grad_mode;
static int loc_fill_grad_p0;
static int loc_fill_grad_p1;
static int loc_fill_grad_lut;
static int loc_fill_grad_extend;
static int loc_fill_rad_x0;
static int loc_fill_rad_y0;
static int loc_fill_rad_r0;
static int loc_fill_rad_a4;
static int loc_fill_rad_bpx;
static int loc_fill_rad_bpy;
static int loc_fill_rad_bc;
static int loc_fill_rad_inv_dr;
static int loc_fill_con_cx;
static int loc_fill_con_cy;
static int loc_fill_con_start;
static int loc_fill_con_span;

static unsigned int prog_line;
static int loc_line_disp;
static int loc_line_p1;
static int loc_line_p2;
static int loc_line_width;
static int loc_line_color;
static int loc_line_round;
static int loc_line_dash_on;
static int loc_line_dash_gap;
static int loc_line_has_dash;

static unsigned int prog_arc;
static int loc_arc_disp;
static int loc_arc_center;
static int loc_arc_radius;
static int loc_arc_width;
static int loc_arc_angles;
static int loc_arc_color;
static int loc_arc_round;
static int loc_arc_tex;
static int loc_arc_use_tex;
static int loc_arc_img_rect;

static unsigned int prog_shadow;
static int loc_shadow_disp;
static int loc_shadow_core;
static int loc_shadow_radius;
static int loc_shadow_color;
static int loc_shadow_blur;
static int loc_shadow_bg_cover;

static unsigned int prog_tri;
static int loc_tri_disp;
static int loc_tri_p0;
static int loc_tri_p1;
static int loc_tri_p2;
static int loc_tri_color;
static int loc_tri_color1;
static int loc_tri_grad_mode;
static int loc_tri_grad_p0;
static int loc_tri_grad_p1;
static int loc_tri_grad_lut;
static int loc_tri_grad_extend;
static int loc_tri_rad_x0;
static int loc_tri_rad_y0;
static int loc_tri_rad_r0;
static int loc_tri_rad_a4;
static int loc_tri_rad_bpx;
static int loc_tri_rad_bpy;
static int loc_tri_rad_bc;
static int loc_tri_rad_inv_dr;
static int loc_tri_con_cx;
static int loc_tri_con_cy;
static int loc_tri_con_start;
static int loc_tri_con_span;

static unsigned int g_gpu2d_grad_lut;

static unsigned int prog_glyph;
static int loc_glyph_disp;
static int loc_glyph_sampler;
static int loc_glyph_color;
static int loc_glyph_opa;
static int loc_glyph_sdf;
static int loc_glyph_smooth;

static unsigned int prog_img;
static int loc_img_disp;
static int loc_img_sampler;
static int loc_img_opa;
static int loc_img_recolor;
static int loc_img_use_recolor;
static int loc_img_mask_sampler;
static int loc_img_use_mask;
static int loc_img_mask_uv;
static int loc_img_use_diff;
static int loc_img_dst_sampler;
static int loc_img_dst_rect;

static unsigned int g_img_dst_snap_tex;
static int32_t g_img_dst_snap_w;
static int32_t g_img_dst_snap_h;
static int32_t g_img_dst_snap_x;
static int32_t g_img_dst_snap_y;
static int loc_tex_disp;
static int loc_tex_sampler;

static lv_draw_task_t g_gpu_text_task;
static lv_draw_label_dsc_t g_gpu_label_dsc;
static int32_t g_gpu2d_dw;
static int32_t g_gpu2d_dh;
static uint32_t g_glyph_overflow_count;

static unsigned int prog_mask;
static int loc_mask_disp;
static int loc_mask_rect;
static int loc_mask_radius;
static int loc_mask_keep_out;

static unsigned int prog_blur;
static int loc_blur_disp;
static int loc_blur_src;
static int loc_blur_dir;
static int loc_blur_radius;
static int loc_blur_step;
static int loc_blur_rect;
static int loc_blur_corner;
static int loc_blur_use_corner;

static unsigned int g_blur_temp_tex;
static unsigned int g_blur_temp_fbo;
static int32_t g_blur_temp_w;
static int32_t g_blur_temp_h;

static unsigned int prog_tex;
static int loc_tex_disp;
static int loc_tex_sampler;

static lv_draw_buf_t g_raster_buf;

typedef struct {
    const lv_draw_box_shadow_dsc_t * sd;
    lv_area_t coords;
} shadow_raster_ctx_t;

static shadow_raster_ctx_t g_shadow_ctx;

static void image_dsc_to_matrix(lv_matrix_t * matrix, int32_t x, int32_t y, const lv_draw_image_dsc_t * dsc);
static void matrix_transform_point(const lv_matrix_t * m, float x, float y, float * ox, float * oy);
static bool draw_image_gpu(const lv_area_t * coords, const lv_area_t * clip,
                            const lv_draw_image_dsc_t * dsc, int32_t dw, int32_t dh);
static void draw_textured_quad(const lv_area_t * area, int32_t dw, int32_t dh, unsigned int tex);
static bool upload_mask_texture(const void * mask_src, unsigned int * tex_out);
static bool upload_mask_from_decoded(const lv_draw_buf_t * decoded, unsigned int * tex_out);
static bool upload_decoded_image(const lv_draw_buf_t * decoded, unsigned int * tex_out);

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
    "uniform vec4 u_color1;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "uniform int u_grad_mode;\n"
    "uniform vec2 u_grad_p0;\n"
    "uniform vec2 u_grad_p1;\n"
    "uniform sampler2D u_grad_lut;\n"
    "uniform int u_grad_extend;\n"
    "uniform float u_rad_x0;\n"
    "uniform float u_rad_y0;\n"
    "uniform float u_rad_r0;\n"
    "uniform float u_rad_a4;\n"
    "uniform float u_rad_bpx;\n"
    "uniform float u_rad_bpy;\n"
    "uniform float u_rad_bc;\n"
    "uniform float u_rad_inv_dr;\n"
    "uniform float u_con_cx;\n"
    "uniform float u_con_cy;\n"
    "uniform float u_con_start;\n"
    "uniform float u_con_span;\n"
    "varying vec2 v_p;\n"
    "float fill_grad_extend(float t){\n"
    "  if(u_grad_extend==0) return clamp(t,0.0,1.0);\n"
    "  if(u_grad_extend==1){ float f=fract(t); return f<0.0?f+1.0:f; }\n"
    "  float f=mod(t,2.0); return f>1.0?2.0-f:f;\n"
    "}\n"
    "void main(){\n"
    "  vec4 base=u_color;\n"
    "  if(u_grad_mode==1){ float t=clamp((v_p.y-u_rect.y)/max(u_rect.w,1.0),0.0,1.0); base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==2){ float t=clamp((v_p.x-u_rect.x)/max(u_rect.z,1.0),0.0,1.0); base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==3){\n"
    "    vec2 se=u_grad_p1-u_grad_p0; float len2=dot(se,se);\n"
    "    float t=len2>0.0001?clamp(dot(v_p-u_grad_p0,se)/len2,0.0,1.0):0.0; base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==4){\n"
    "    float xp=v_p.x; float yp=v_p.y;\n"
    "    float b=xp*u_rad_bpx+yp*u_rad_bpy+u_rad_bc;\n"
    "    float c=u_rad_r0*u_rad_r0-(xp-u_rad_x0)*(xp-u_rad_x0)-(yp-u_rad_y0)*(yp-u_rad_y0);\n"
    "    float t=0.0;\n"
    "    if(abs(u_rad_a4)<0.000001) t=abs(b)<0.000001?0.0:-c/b;\n"
    "    else if(abs(u_rad_bpx)>0.000001||abs(u_rad_bpy)>0.000001){\n"
    "      float det=b*b-u_rad_a4*c; t=det<0.0?0.0:(-b+sqrt(det))/u_rad_a4; }\n"
    "    else t=(length(v_p-vec2(u_rad_x0,u_rad_y0))-u_rad_r0)*u_rad_inv_dr;\n"
    "    t=fill_grad_extend(t); base=texture2D(u_grad_lut,vec2(t,0.5)); }\n"
    "  else if(u_grad_mode==5){\n"
    "    float ang=degrees(atan(v_p.y-u_con_cy,v_p.x-u_con_cx)); if(ang<0.0) ang+=360.0;\n"
    "    float t=fill_grad_extend((ang-u_con_start)/max(u_con_span,0.001));\n"
    "    base=texture2D(u_grad_lut,vec2(t,0.5)); }\n"
    "  vec2 h=u_rect.zw*0.5; vec2 c=u_rect.xy+h;\n"
    "  vec2 q=abs(v_p-c)-h+u_radius;\n"
    "  float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-u_radius;\n"
    "  float a=base.a*(1.0-smoothstep(-1.0,1.0,d));\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(base.rgb,a);\n"
    "}\n";
static const char * fs_line =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform vec2 u_p1;\n"
    "uniform vec2 u_p2;\n"
    "uniform float u_width;\n"
    "uniform float u_round;\n"
    "uniform float u_dash_on;\n"
    "uniform float u_dash_gap;\n"
    "uniform int u_has_dash;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 pa=v_p-u_p1; vec2 ba=u_p2-u_p1;\n"
    "  float h=clamp(dot(pa,ba)/max(dot(ba,ba),0.0001),0.0,1.0);\n"
    "  if(u_has_dash>0){\n"
    "    float period=u_dash_on+u_dash_gap;\n"
    "    if(period>0.0){ float along=h*length(ba); if(mod(along,period)>u_dash_on) discard; }\n"
    "  }\n"
    "  float dist=length(pa-ba*h);\n"
    "  float half_w=u_width*0.5;\n"
    "  float a=u_color.a*(1.0-smoothstep(half_w-0.75,half_w+0.25,dist));\n"
    "  if(u_round>0.5){\n"
    "    float d1=length(v_p-u_p1); float d2=length(v_p-u_p2);\n"
    "    float ra=u_color.a*(1.0-smoothstep(half_w-0.75,half_w+0.25,min(d1,d2)));\n"
    "    a=max(a,ra);\n"
    "  }\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(u_color.rgb,a);\n"
    "}\n";
static const char * fs_arc =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform vec2 u_center;\n"
    "uniform float u_radius;\n"
    "uniform float u_width;\n"
    "uniform vec2 u_angles;\n"
    "uniform float u_round;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_use_tex;\n"
    "uniform vec4 u_img_rect;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 d=v_p-u_center;\n"
    "  float dist=length(d);\n"
    "  float half_w=u_width*0.5;\n"
    "  float ring=1.0-smoothstep(half_w-0.75,half_w+0.25,abs(dist-u_radius));\n"
    "  if(ring<0.004) discard;\n"
    "  float ang=degrees(atan(d.y,d.x)); if(ang<0.0) ang+=360.0;\n"
    "  float a0=u_angles.x; float a1=u_angles.y;\n"
    "  float in_arc=0.0;\n"
    "  if(a0<=a1){ in_arc=step(a0,ang)*step(ang,a1); }\n"
    "  else { in_arc=step(a0,ang)+step(ang,a1); in_arc=clamp(in_arc,0.0,1.0); }\n"
    "  vec4 col=u_color;\n"
    "  if(u_use_tex>0){ vec2 uv=(v_p-u_img_rect.xy)/max(u_img_rect.zw,vec2(0.001)); col=texture2D(u_tex,uv); }\n"
    "  float a=col.a*ring*in_arc;\n"
    "  if(u_round>0.5 && in_arc<0.5){\n"
    "    vec2 p0=u_center+vec2(cos(radians(a0)),sin(radians(a0)))*u_radius;\n"
    "    vec2 p1=u_center+vec2(cos(radians(a1)),sin(radians(a1)))*u_radius;\n"
    "    float cap=1.0-smoothstep(half_w-0.75,half_w+0.25,min(length(v_p-p0),length(v_p-p1)));\n"
    "    a=max(a,col.a*cap);\n"
    "  }\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(col.rgb,a);\n"
    "}\n";
static const char * fs_shadow =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform vec4 u_core;\n"
    "uniform float u_radius;\n"
    "uniform float u_blur;\n"
    "uniform float u_bg_cover;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 c=u_core.xy+u_core.zw*0.5;\n"
    "  vec2 h=u_core.zw*0.5; float r=min(u_radius,min(h.x,h.y));\n"
    "  vec2 q=abs(v_p-c)-h+r; float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;\n"
    "  if(u_bg_cover>0.5 && d<0.0) discard;\n"
    "  float blur=max(u_blur,1.0);\n"
    "  float a=u_color.a*(1.0-smoothstep(0.0,blur,d));\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(u_color.rgb,a);\n"
    "}\n";
static const char * fs_tri =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "uniform vec4 u_color1;\n"
    "uniform vec2 u_p0;\n"
    "uniform vec2 u_p1;\n"
    "uniform vec2 u_p2;\n"
    "uniform int u_grad_mode;\n"
    "uniform vec2 u_grad_p0;\n"
    "uniform vec2 u_grad_p1;\n"
    "uniform sampler2D u_grad_lut;\n"
    "uniform int u_grad_extend;\n"
    "uniform float u_rad_x0;\n"
    "uniform float u_rad_y0;\n"
    "uniform float u_rad_r0;\n"
    "uniform float u_rad_a4;\n"
    "uniform float u_rad_bpx;\n"
    "uniform float u_rad_bpy;\n"
    "uniform float u_rad_bc;\n"
    "uniform float u_rad_inv_dr;\n"
    "uniform float u_con_cx;\n"
    "uniform float u_con_cy;\n"
    "uniform float u_con_start;\n"
    "uniform float u_con_span;\n"
    "varying vec2 v_p;\n"
    "float tri_grad_extend(float t){\n"
    "  if(u_grad_extend==0) return clamp(t,0.0,1.0);\n"
    "  if(u_grad_extend==1){ float f=fract(t); return f<0.0?f+1.0:f; }\n"
    "  float f=mod(t,2.0); return f>1.0?2.0-f:f;\n"
    "}\n"
    "void main(){\n"
    "  vec2 v0=u_p2-u_p0; vec2 v1=u_p1-u_p0; vec2 v2=v_p-u_p0;\n"
    "  float dot00=dot(v0,v0); float dot01=dot(v0,v1); float dot02=dot(v0,v2);\n"
    "  float dot11=dot(v1,v1); float dot12=dot(v1,v2);\n"
    "  float inv=1.0/(dot00*dot11-dot01*dot01);\n"
    "  float u=(dot11*dot02-dot01*dot12)*inv; float v=(dot00*dot12-dot01*dot02)*inv;\n"
    "  if(u<0.0||v<0.0||u+v>1.0) discard;\n"
    "  vec4 base=u_color;\n"
    "  if(u_grad_mode==1){ float t=clamp((v_p.y-u_grad_p0.y)/max(u_grad_p1.y-u_grad_p0.y,1.0),0.0,1.0); base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==2){ float t=clamp((v_p.x-u_grad_p0.x)/max(u_grad_p1.x-u_grad_p0.x,1.0),0.0,1.0); base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==3){\n"
    "    vec2 se=u_grad_p1-u_grad_p0; float len2=dot(se,se);\n"
    "    float t=len2>0.0001?clamp(dot(v_p-u_grad_p0,se)/len2,0.0,1.0):0.0; base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==4){\n"
    "    float xp=v_p.x; float yp=v_p.y;\n"
    "    float b=xp*u_rad_bpx+yp*u_rad_bpy+u_rad_bc;\n"
    "    float c=u_rad_r0*u_rad_r0-(xp-u_rad_x0)*(xp-u_rad_x0)-(yp-u_rad_y0)*(yp-u_rad_y0);\n"
    "    float t=0.0;\n"
    "    if(abs(u_rad_a4)<0.000001){ t=abs(b)<0.000001?0.0:-c/b; }\n"
    "    else if(abs(u_rad_bpx)>0.000001||abs(u_rad_bpy)>0.000001){\n"
    "      float det=b*b-u_rad_a4*c; t=det<0.0?0.0:(-b+sqrt(det))/u_rad_a4; }\n"
    "    else { t=(length(v_p-vec2(u_rad_x0,u_rad_y0))-u_rad_r0)*u_rad_inv_dr; }\n"
    "    t=tri_grad_extend(t); base=texture2D(u_grad_lut,vec2(t,0.5)); }\n"
    "  else if(u_grad_mode==5){\n"
    "    float ang=degrees(atan(v_p.y-u_con_cy,v_p.x-u_con_cx)); if(ang<0.0) ang+=360.0;\n"
    "    float t=tri_grad_extend((ang-u_con_start)/max(u_con_span,0.001));\n"
    "    base=texture2D(u_grad_lut,vec2(t,0.5)); }\n"
    "  if(base.a<0.004) discard;\n"
    "  gl_FragColor=base;\n"
    "}\n";
static const char * fs_mask =
    "precision mediump float;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "uniform float u_keep_out;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  float rx=u_rect.x; float ry=u_rect.y; float rw=u_rect.z; float rh=u_rect.w;\n"
    "  vec2 c=vec2(rx,ry)+vec2(rw,rh)*0.5; vec2 h=vec2(rw,rh)*0.5; float r=u_radius;\n"
    "  vec2 q=abs(v_p-c)-h+r; float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;\n"
    "  float m=0.0;\n"
    "  if(u_keep_out<0.5){\n"
    "    if(v_p.x<rx||v_p.y<ry||v_p.x>rx+rw||v_p.y>ry+rh) m=1.0;\n"
    "    else m=smoothstep(-1.0,1.0,d);\n"
    "  } else {\n"
    "    if(v_p.x>=rx&&v_p.y>=ry&&v_p.x<=rx+rw&&v_p.y<=ry+rh) m=1.0-smoothstep(-1.0,1.0,-d);\n"
    "  }\n"
    "  if(m<0.004) discard;\n"
    "  gl_FragColor=vec4(0.0,0.0,0.0,m);\n"
    "}\n";
static const char * fs_blur =
    "precision mediump float;\n"
    "uniform sampler2D u_src;\n"
    "uniform vec2 u_disp;\n"
    "uniform vec2 u_dir;\n"
    "uniform float u_radius;\n"
    "uniform float u_step;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_corner_r;\n"
    "uniform int u_use_corner;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 texel=1.0/u_disp; vec2 uv=v_p/u_disp;\n"
    "  float step=max(u_step,1.0); float r=u_radius;\n"
    "  int cnt=int(ceil(r/step)); if(cnt>32) cnt=32;\n"
    "  vec4 acc=vec4(0.0); float wsum=0.0;\n"
    "  for(int i=-32;i<=32;i++){\n"
    "    if(i<-cnt||i>cnt) continue;\n"
    "    acc+=texture2D(u_src,uv+u_dir*texel*float(i)*step); wsum+=1.0;\n"
    "  }\n"
    "  if(wsum<0.5) discard;\n"
    "  gl_FragColor=acc/wsum;\n"
    "  if(u_use_corner>0){\n"
    "    vec2 c=u_rect.xy+u_rect.zw*0.5; vec2 h=u_rect.zw*0.5; float cr=u_corner_r;\n"
    "    vec2 q=abs(v_p-c)-h+cr; float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-cr;\n"
    "    if(d>0.5) discard;\n"
    "    gl_FragColor.a*=1.0-smoothstep(-1.0,1.0,d);\n"
    "  }\n"
    "}\n";
static const char * vs_line =
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_p;\n"
    "void main(){ v_p=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * vs_arc =
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_p;\n"
    "void main(){ v_p=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * vs_shadow =
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_p;\n"
    "void main(){ v_p=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * vs_tex =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_uv;\n"
    "varying vec2 v_scr;\n"
    "void main(){ v_uv=a_uv; v_scr=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * fs_tex =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main(){ gl_FragColor=texture2D(u_tex,v_uv);}\n";
static const char * fs_glyph =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec4 u_color;\n"
    "uniform float u_opa;\n"
    "uniform float u_sdf;\n"
    "uniform float u_smooth;\n"
    "void main(){\n"
    "  float d=texture2D(u_tex,v_uv).a;\n"
    "  float a;\n"
    "  if(u_sdf>0.5){ a=smoothstep(0.5-u_smooth,0.5+u_smooth,d); }\n"
    "  else { a=d; }\n"
    "  a*=u_opa*u_color.a;\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(u_color.rgb,a);\n"
    "}\n";
static const char * fs_img =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform sampler2D u_mask;\n"
    "uniform sampler2D u_dst;\n"
    "uniform float u_opa;\n"
    "uniform vec4 u_recolor;\n"
    "uniform int u_use_recolor;\n"
    "uniform int u_use_mask;\n"
    "uniform vec4 u_mask_uv;\n"
    "uniform int u_use_diff;\n"
    "uniform vec4 u_dst_rect;\n"
    "varying vec2 v_scr;\n"
    "void main(){\n"
    "  vec4 c=texture2D(u_tex,v_uv);\n"
    "  if(u_use_recolor>0){ c.rgb=mix(c.rgb,u_recolor.rgb,u_recolor.a); }\n"
    "  if(u_use_mask>0){\n"
    "    vec2 muv=vec2(mix(u_mask_uv.x,u_mask_uv.z,v_uv.x),mix(u_mask_uv.y,u_mask_uv.w,v_uv.y));\n"
    "    c.a*=texture2D(u_mask,muv).a;\n"
    "  }\n"
    "  c.a*=u_opa;\n"
    "  if(c.a<0.004) discard;\n"
    "  if(u_use_diff>0){\n"
    "    vec2 duv=(v_scr-u_dst_rect.xy)/max(u_dst_rect.zw,vec2(0.001));\n"
    "    vec4 d=texture2D(u_dst, duv);\n"
    "    gl_FragColor=vec4(abs(d.rgb-c.rgb),max(d.a,c.a));\n"
    "    return;\n"
    "  }\n"
    "  gl_FragColor=c;\n"
    "}\n";
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
    "uniform vec4 u_color1;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "uniform int u_grad_mode;\n"
    "uniform vec2 u_grad_p0;\n"
    "uniform vec2 u_grad_p1;\n"
    "uniform sampler2D u_grad_lut;\n"
    "uniform int u_grad_extend;\n"
    "uniform float u_rad_x0;\n"
    "uniform float u_rad_y0;\n"
    "uniform float u_rad_r0;\n"
    "uniform float u_rad_a4;\n"
    "uniform float u_rad_bpx;\n"
    "uniform float u_rad_bpy;\n"
    "uniform float u_rad_bc;\n"
    "uniform float u_rad_inv_dr;\n"
    "uniform float u_con_cx;\n"
    "uniform float u_con_cy;\n"
    "uniform float u_con_start;\n"
    "uniform float u_con_span;\n"
    "varying vec2 v_p;\n"
    "float fill_grad_extend(float t){\n"
    "  if(u_grad_extend==0) return clamp(t,0.0,1.0);\n"
    "  if(u_grad_extend==1){ float f=fract(t); return f<0.0?f+1.0:f; }\n"
    "  float f=mod(t,2.0); return f>1.0?2.0-f:f;\n"
    "}\n"
    "void main(){\n"
    "  vec4 base=u_color;\n"
    "  if(u_grad_mode==1){ float t=clamp((v_p.y-u_rect.y)/max(u_rect.w,1.0),0.0,1.0); base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==2){ float t=clamp((v_p.x-u_rect.x)/max(u_rect.z,1.0),0.0,1.0); base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==3){\n"
    "    vec2 se=u_grad_p1-u_grad_p0; float len2=dot(se,se);\n"
    "    float t=len2>0.0001?clamp(dot(v_p-u_grad_p0,se)/len2,0.0,1.0):0.0; base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==4){\n"
    "    float xp=v_p.x; float yp=v_p.y;\n"
    "    float b=xp*u_rad_bpx+yp*u_rad_bpy+u_rad_bc;\n"
    "    float c=u_rad_r0*u_rad_r0-(xp-u_rad_x0)*(xp-u_rad_x0)-(yp-u_rad_y0)*(yp-u_rad_y0);\n"
    "    float t=0.0;\n"
    "    if(abs(u_rad_a4)<0.000001) t=abs(b)<0.000001?0.0:-c/b;\n"
    "    else if(abs(u_rad_bpx)>0.000001||abs(u_rad_bpy)>0.000001){\n"
    "      float det=b*b-u_rad_a4*c; t=det<0.0?0.0:(-b+sqrt(det))/u_rad_a4; }\n"
    "    else t=(length(v_p-vec2(u_rad_x0,u_rad_y0))-u_rad_r0)*u_rad_inv_dr;\n"
    "    t=fill_grad_extend(t); base=texture2D(u_grad_lut,vec2(t,0.5)); }\n"
    "  else if(u_grad_mode==5){\n"
    "    float ang=degrees(atan(v_p.y-u_con_cy,v_p.x-u_con_cx)); if(ang<0.0) ang+=360.0;\n"
    "    float t=fill_grad_extend((ang-u_con_start)/max(u_con_span,0.001));\n"
    "    base=texture2D(u_grad_lut,vec2(t,0.5)); }\n"
    "  vec2 h=u_rect.zw*0.5; vec2 c=u_rect.xy+h;\n"
    "  vec2 q=abs(v_p-c)-h+u_radius;\n"
    "  float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-u_radius;\n"
    "  float a=base.a*(1.0-smoothstep(-1.0,1.0,d));\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(base.rgb,a);\n"
    "}\n";
static const char * fs_line =
    "#version 120\n"
    "uniform vec4 u_color;\n"
    "uniform vec2 u_p1;\n"
    "uniform vec2 u_p2;\n"
    "uniform float u_width;\n"
    "uniform float u_round;\n"
    "uniform float u_dash_on;\n"
    "uniform float u_dash_gap;\n"
    "uniform int u_has_dash;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 pa=v_p-u_p1; vec2 ba=u_p2-u_p1;\n"
    "  float h=clamp(dot(pa,ba)/max(dot(ba,ba),0.0001),0.0,1.0);\n"
    "  if(u_has_dash>0){ float period=u_dash_on+u_dash_gap;\n"
    "    if(period>0.0){ float along=h*length(ba); if(mod(along,period)>u_dash_on) discard; } }\n"
    "  float dist=length(pa-ba*h); float half_w=u_width*0.5;\n"
    "  float a=u_color.a*(1.0-smoothstep(half_w-0.75,half_w+0.25,dist));\n"
    "  if(u_round>0.5){ float d1=length(v_p-u_p1); float d2=length(v_p-u_p2);\n"
    "    a=max(a,u_color.a*(1.0-smoothstep(half_w-0.75,half_w+0.25,min(d1,d2)))); }\n"
    "  if(a<0.004) discard; gl_FragColor=vec4(u_color.rgb,a);\n"
    "}\n";
static const char * fs_arc =
    "#version 120\n"
    "uniform vec4 u_color;\n"
    "uniform vec2 u_center;\n"
    "uniform float u_radius;\n"
    "uniform float u_width;\n"
    "uniform vec2 u_angles;\n"
    "uniform float u_round;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_use_tex;\n"
    "uniform vec4 u_img_rect;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 d=v_p-u_center; float dist=length(d); float half_w=u_width*0.5;\n"
    "  float ring=1.0-smoothstep(half_w-0.75,half_w+0.25,abs(dist-u_radius));\n"
    "  if(ring<0.004) discard;\n"
    "  float ang=degrees(atan(d.y,d.x)); if(ang<0.0) ang+=360.0;\n"
    "  float a0=u_angles.x; float a1=u_angles.y; float in_arc=0.0;\n"
    "  if(a0<=a1) in_arc=step(a0,ang)*step(ang,a1); else in_arc=clamp(step(a0,ang)+step(ang,a1),0.0,1.0);\n"
    "  vec4 col=u_color;\n"
    "  if(u_use_tex>0){ vec2 uv=(v_p-u_img_rect.xy)/max(u_img_rect.zw,vec2(0.001)); col=texture2D(u_tex,uv); }\n"
    "  float a=col.a*ring*in_arc;\n"
    "  if(u_round>0.5 && in_arc<0.5){\n"
    "    vec2 p0=u_center+vec2(cos(radians(a0)),sin(radians(a0)))*u_radius;\n"
    "    vec2 p1=u_center+vec2(cos(radians(a1)),sin(radians(a1)))*u_radius;\n"
    "    a=max(a,col.a*(1.0-smoothstep(half_w-0.75,half_w+0.25,min(length(v_p-p0),length(v_p-p1))))); }\n"
    "  if(a<0.004) discard; gl_FragColor=vec4(col.rgb,a);\n"
    "}\n";
static const char * fs_shadow =
    "#version 120\n"
    "uniform vec4 u_color;\n"
    "uniform vec4 u_core;\n"
    "uniform float u_radius;\n"
    "uniform float u_blur;\n"
    "uniform float u_bg_cover;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 c=u_core.xy+u_core.zw*0.5; vec2 h=u_core.zw*0.5; float r=min(u_radius,min(h.x,h.y));\n"
    "  vec2 q=abs(v_p-c)-h+r; float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;\n"
    "  if(u_bg_cover>0.5 && d<0.0) discard;\n"
    "  float a=u_color.a*(1.0-smoothstep(0.0,max(u_blur,1.0),d));\n"
    "  if(a<0.004) discard; gl_FragColor=vec4(u_color.rgb,a);\n"
    "}\n";
static const char * fs_tri =
    "#version 120\n"
    "uniform vec4 u_color;\n"
    "uniform vec4 u_color1;\n"
    "uniform vec2 u_p0;\n"
    "uniform vec2 u_p1;\n"
    "uniform vec2 u_p2;\n"
    "uniform int u_grad_mode;\n"
    "uniform vec2 u_grad_p0;\n"
    "uniform vec2 u_grad_p1;\n"
    "uniform sampler2D u_grad_lut;\n"
    "uniform int u_grad_extend;\n"
    "uniform float u_rad_x0;\n"
    "uniform float u_rad_y0;\n"
    "uniform float u_rad_r0;\n"
    "uniform float u_rad_a4;\n"
    "uniform float u_rad_bpx;\n"
    "uniform float u_rad_bpy;\n"
    "uniform float u_rad_bc;\n"
    "uniform float u_rad_inv_dr;\n"
    "uniform float u_con_cx;\n"
    "uniform float u_con_cy;\n"
    "uniform float u_con_start;\n"
    "uniform float u_con_span;\n"
    "varying vec2 v_p;\n"
    "float tri_grad_extend(float t){\n"
    "  if(u_grad_extend==0) return clamp(t,0.0,1.0);\n"
    "  if(u_grad_extend==1){ float f=fract(t); return f<0.0?f+1.0:f; }\n"
    "  float f=mod(t,2.0); return f>1.0?2.0-f:f;\n"
    "}\n"
    "void main(){\n"
    "  vec2 v0=u_p2-u_p0; vec2 v1=u_p1-u_p0; vec2 v2=v_p-u_p0;\n"
    "  float dot00=dot(v0,v0); float dot01=dot(v0,v1); float dot02=dot(v0,v2);\n"
    "  float dot11=dot(v1,v1); float dot12=dot(v1,v2);\n"
    "  float inv=1.0/(dot00*dot11-dot01*dot01);\n"
    "  float u=(dot11*dot02-dot01*dot12)*inv; float v=(dot00*dot12-dot01*dot02)*inv;\n"
    "  if(u<0.0||v<0.0||u+v>1.0) discard;\n"
    "  vec4 base=u_color;\n"
    "  if(u_grad_mode==1){ float t=clamp((v_p.y-u_grad_p0.y)/max(u_grad_p1.y-u_grad_p0.y,1.0),0.0,1.0); base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==2){ float t=clamp((v_p.x-u_grad_p0.x)/max(u_grad_p1.x-u_grad_p0.x,1.0),0.0,1.0); base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==3){\n"
    "    vec2 se=u_grad_p1-u_grad_p0; float len2=dot(se,se);\n"
    "    float t=len2>0.0001?clamp(dot(v_p-u_grad_p0,se)/len2,0.0,1.0):0.0; base=mix(u_color,u_color1,t); }\n"
    "  else if(u_grad_mode==4){\n"
    "    float xp=v_p.x; float yp=v_p.y;\n"
    "    float b=xp*u_rad_bpx+yp*u_rad_bpy+u_rad_bc;\n"
    "    float c=u_rad_r0*u_rad_r0-(xp-u_rad_x0)*(xp-u_rad_x0)-(yp-u_rad_y0)*(yp-u_rad_y0);\n"
    "    float t=0.0;\n"
    "    if(abs(u_rad_a4)<0.000001) t=abs(b)<0.000001?0.0:-c/b;\n"
    "    else if(abs(u_rad_bpx)>0.000001||abs(u_rad_bpy)>0.000001){\n"
    "      float det=b*b-u_rad_a4*c; t=det<0.0?0.0:(-b+sqrt(det))/u_rad_a4; }\n"
    "    else t=(length(v_p-vec2(u_rad_x0,u_rad_y0))-u_rad_r0)*u_rad_inv_dr;\n"
    "    t=tri_grad_extend(t); base=texture2D(u_grad_lut,vec2(t,0.5)); }\n"
    "  else if(u_grad_mode==5){\n"
    "    float ang=degrees(atan(v_p.y-u_con_cy,v_p.x-u_con_cx)); if(ang<0.0) ang+=360.0;\n"
    "    float t=tri_grad_extend((ang-u_con_start)/max(u_con_span,0.001));\n"
    "    base=texture2D(u_grad_lut,vec2(t,0.5)); }\n"
    "  if(base.a<0.004) discard; gl_FragColor=base;\n"
    "}\n";
static const char * fs_mask =
    "#version 120\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_radius;\n"
    "uniform float u_keep_out;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  float rx=u_rect.x; float ry=u_rect.y; float rw=u_rect.z; float rh=u_rect.w;\n"
    "  vec2 c=vec2(rx,ry)+vec2(rw,rh)*0.5; vec2 h=vec2(rw,rh)*0.5; float r=u_radius;\n"
    "  vec2 q=abs(v_p-c)-h+r; float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;\n"
    "  float m=0.0;\n"
    "  if(u_keep_out<0.5){\n"
    "    if(v_p.x<rx||v_p.y<ry||v_p.x>rx+rw||v_p.y>ry+rh) m=1.0;\n"
    "    else m=smoothstep(-1.0,1.0,d);\n"
    "  } else if(v_p.x>=rx&&v_p.y>=ry&&v_p.x<=rx+rw&&v_p.y<=ry+rh) m=1.0-smoothstep(-1.0,1.0,-d);\n"
    "  if(m<0.004) discard; gl_FragColor=vec4(0.0,0.0,0.0,m);\n"
    "}\n";
static const char * fs_blur =
    "#version 120\n"
    "uniform sampler2D u_src;\n"
    "uniform vec2 u_disp;\n"
    "uniform vec2 u_dir;\n"
    "uniform float u_radius;\n"
    "uniform float u_step;\n"
    "uniform vec4 u_rect;\n"
    "uniform float u_corner_r;\n"
    "uniform int u_use_corner;\n"
    "varying vec2 v_p;\n"
    "void main(){\n"
    "  vec2 texel=1.0/u_disp; vec2 uv=v_p/u_disp;\n"
    "  float step=max(u_step,1.0); float r=u_radius;\n"
    "  int cnt=int(ceil(r/step)); if(cnt>32) cnt=32;\n"
    "  vec4 acc=vec4(0.0); float wsum=0.0;\n"
    "  for(int i=-32;i<=32;i++){\n"
    "    if(i<-cnt||i>cnt) continue;\n"
    "    acc+=texture2D(u_src,uv+u_dir*texel*float(i)*step); wsum+=1.0;\n"
    "  }\n"
    "  if(wsum<0.5) discard;\n"
    "  gl_FragColor=acc/wsum;\n"
    "  if(u_use_corner>0){\n"
    "    vec2 c=u_rect.xy+u_rect.zw*0.5; vec2 h=u_rect.zw*0.5; float cr=u_corner_r;\n"
    "    vec2 q=abs(v_p-c)-h+cr; float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-cr;\n"
    "    if(d>0.5) discard; gl_FragColor.a*=1.0-smoothstep(-1.0,1.0,d);\n"
    "  }\n"
    "}\n";
static const char * vs_line =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_p;\n"
    "void main(){ v_p=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * vs_arc =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_p;\n"
    "void main(){ v_p=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * vs_shadow =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_p;\n"
    "void main(){ v_p=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * vs_tex =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "uniform vec2 u_disp;\n"
    "varying vec2 v_uv;\n"
    "varying vec2 v_scr;\n"
    "void main(){ v_uv=a_uv; v_scr=a_pos; vec2 ndc; ndc.x=a_pos.x/u_disp.x*2.0-1.0; ndc.y=a_pos.y/u_disp.y*2.0-1.0; gl_Position=vec4(ndc,0.0,1.0);}\n";
static const char * fs_tex =
    "#version 120\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main(){ gl_FragColor=texture2D(u_tex,v_uv);}\n";
static const char * fs_glyph =
    "#version 120\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec4 u_color;\n"
    "uniform float u_opa;\n"
    "uniform float u_sdf;\n"
    "uniform float u_smooth;\n"
    "void main(){\n"
    "  float d=texture2D(u_tex,v_uv).a;\n"
    "  float a;\n"
    "  if(u_sdf>0.5){ a=smoothstep(0.5-u_smooth,0.5+u_smooth,d); }\n"
    "  else { a=d; }\n"
    "  a*=u_opa*u_color.a;\n"
    "  if(a<0.004) discard;\n"
    "  gl_FragColor=vec4(u_color.rgb,a);\n"
    "}\n";
static const char * fs_img =
    "#version 120\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform sampler2D u_mask;\n"
    "uniform sampler2D u_dst;\n"
    "uniform float u_opa;\n"
    "uniform vec4 u_recolor;\n"
    "uniform int u_use_recolor;\n"
    "uniform int u_use_mask;\n"
    "uniform vec4 u_mask_uv;\n"
    "uniform int u_use_diff;\n"
    "uniform vec4 u_dst_rect;\n"
    "varying vec2 v_scr;\n"
    "void main(){\n"
    "  vec4 c=texture2D(u_tex,v_uv);\n"
    "  if(u_use_recolor>0){ c.rgb=mix(c.rgb,u_recolor.rgb,u_recolor.a); }\n"
    "  if(u_use_mask>0){\n"
    "    vec2 muv=vec2(mix(u_mask_uv.x,u_mask_uv.z,v_uv.x),mix(u_mask_uv.y,u_mask_uv.w,v_uv.y));\n"
    "    c.a*=texture2D(u_mask,muv).a;\n"
    "  }\n"
    "  c.a*=u_opa;\n"
    "  if(c.a<0.004) discard;\n"
    "  if(u_use_diff>0){\n"
    "    vec2 duv=(v_scr-u_dst_rect.xy)/max(u_dst_rect.zw,vec2(0.001));\n"
    "    vec4 d=texture2D(u_dst, duv);\n"
    "    gl_FragColor=vec4(abs(d.rgb-c.rgb),max(d.a,c.a));\n"
    "    return;\n"
    "  }\n"
    "  gl_FragColor=c;\n"
    "}\n";
#endif

void lv_gpu_renderer_gles2_2d_init(void)
{
    if(prog_fill) return;
    prog_fill = link_program(vs_fill, fs_fill);
    if(prog_fill) {
        loc_fill_disp = glGetUniformLocation(prog_fill, "u_disp");
        loc_fill_color = glGetUniformLocation(prog_fill, "u_color");
        loc_fill_color1 = glGetUniformLocation(prog_fill, "u_color1");
        loc_fill_rect = glGetUniformLocation(prog_fill, "u_rect");
        loc_fill_radius = glGetUniformLocation(prog_fill, "u_radius");
        loc_fill_grad_mode = glGetUniformLocation(prog_fill, "u_grad_mode");
        loc_fill_grad_p0 = glGetUniformLocation(prog_fill, "u_grad_p0");
        loc_fill_grad_p1 = glGetUniformLocation(prog_fill, "u_grad_p1");
        loc_fill_grad_lut = glGetUniformLocation(prog_fill, "u_grad_lut");
        loc_fill_grad_extend = glGetUniformLocation(prog_fill, "u_grad_extend");
        loc_fill_rad_x0 = glGetUniformLocation(prog_fill, "u_rad_x0");
        loc_fill_rad_y0 = glGetUniformLocation(prog_fill, "u_rad_y0");
        loc_fill_rad_r0 = glGetUniformLocation(prog_fill, "u_rad_r0");
        loc_fill_rad_a4 = glGetUniformLocation(prog_fill, "u_rad_a4");
        loc_fill_rad_bpx = glGetUniformLocation(prog_fill, "u_rad_bpx");
        loc_fill_rad_bpy = glGetUniformLocation(prog_fill, "u_rad_bpy");
        loc_fill_rad_bc = glGetUniformLocation(prog_fill, "u_rad_bc");
        loc_fill_rad_inv_dr = glGetUniformLocation(prog_fill, "u_rad_inv_dr");
        loc_fill_con_cx = glGetUniformLocation(prog_fill, "u_con_cx");
        loc_fill_con_cy = glGetUniformLocation(prog_fill, "u_con_cy");
        loc_fill_con_start = glGetUniformLocation(prog_fill, "u_con_start");
        loc_fill_con_span = glGetUniformLocation(prog_fill, "u_con_span");
    }
    prog_line = link_program(vs_line, fs_line);
    if(prog_line) {
        loc_line_disp = glGetUniformLocation(prog_line, "u_disp");
        loc_line_p1 = glGetUniformLocation(prog_line, "u_p1");
        loc_line_p2 = glGetUniformLocation(prog_line, "u_p2");
        loc_line_width = glGetUniformLocation(prog_line, "u_width");
        loc_line_color = glGetUniformLocation(prog_line, "u_color");
        loc_line_round = glGetUniformLocation(prog_line, "u_round");
        loc_line_dash_on = glGetUniformLocation(prog_line, "u_dash_on");
        loc_line_dash_gap = glGetUniformLocation(prog_line, "u_dash_gap");
        loc_line_has_dash = glGetUniformLocation(prog_line, "u_has_dash");
    }
    prog_arc = link_program(vs_arc, fs_arc);
    if(prog_arc) {
        loc_arc_disp = glGetUniformLocation(prog_arc, "u_disp");
        loc_arc_center = glGetUniformLocation(prog_arc, "u_center");
        loc_arc_radius = glGetUniformLocation(prog_arc, "u_radius");
        loc_arc_width = glGetUniformLocation(prog_arc, "u_width");
        loc_arc_angles = glGetUniformLocation(prog_arc, "u_angles");
        loc_arc_color = glGetUniformLocation(prog_arc, "u_color");
        loc_arc_round = glGetUniformLocation(prog_arc, "u_round");
        loc_arc_tex = glGetUniformLocation(prog_arc, "u_tex");
        loc_arc_use_tex = glGetUniformLocation(prog_arc, "u_use_tex");
        loc_arc_img_rect = glGetUniformLocation(prog_arc, "u_img_rect");
    }
    prog_shadow = link_program(vs_shadow, fs_shadow);
    if(prog_shadow) {
        loc_shadow_disp = glGetUniformLocation(prog_shadow, "u_disp");
        loc_shadow_core = glGetUniformLocation(prog_shadow, "u_core");
        loc_shadow_radius = glGetUniformLocation(prog_shadow, "u_radius");
        loc_shadow_color = glGetUniformLocation(prog_shadow, "u_color");
        loc_shadow_blur = glGetUniformLocation(prog_shadow, "u_blur");
        loc_shadow_bg_cover = glGetUniformLocation(prog_shadow, "u_bg_cover");
    }
    prog_tri = link_program(vs_line, fs_tri);
    if(prog_tri) {
        loc_tri_disp = glGetUniformLocation(prog_tri, "u_disp");
        loc_tri_p0 = glGetUniformLocation(prog_tri, "u_p0");
        loc_tri_p1 = glGetUniformLocation(prog_tri, "u_p1");
        loc_tri_p2 = glGetUniformLocation(prog_tri, "u_p2");
        loc_tri_color = glGetUniformLocation(prog_tri, "u_color");
        loc_tri_color1 = glGetUniformLocation(prog_tri, "u_color1");
        loc_tri_grad_mode = glGetUniformLocation(prog_tri, "u_grad_mode");
        loc_tri_grad_p0 = glGetUniformLocation(prog_tri, "u_grad_p0");
        loc_tri_grad_p1 = glGetUniformLocation(prog_tri, "u_grad_p1");
        loc_tri_grad_lut = glGetUniformLocation(prog_tri, "u_grad_lut");
        loc_tri_grad_extend = glGetUniformLocation(prog_tri, "u_grad_extend");
        loc_tri_rad_x0 = glGetUniformLocation(prog_tri, "u_rad_x0");
        loc_tri_rad_y0 = glGetUniformLocation(prog_tri, "u_rad_y0");
        loc_tri_rad_r0 = glGetUniformLocation(prog_tri, "u_rad_r0");
        loc_tri_rad_a4 = glGetUniformLocation(prog_tri, "u_rad_a4");
        loc_tri_rad_bpx = glGetUniformLocation(prog_tri, "u_rad_bpx");
        loc_tri_rad_bpy = glGetUniformLocation(prog_tri, "u_rad_bpy");
        loc_tri_rad_bc = glGetUniformLocation(prog_tri, "u_rad_bc");
        loc_tri_rad_inv_dr = glGetUniformLocation(prog_tri, "u_rad_inv_dr");
        loc_tri_con_cx = glGetUniformLocation(prog_tri, "u_con_cx");
        loc_tri_con_cy = glGetUniformLocation(prog_tri, "u_con_cy");
        loc_tri_con_start = glGetUniformLocation(prog_tri, "u_con_start");
        loc_tri_con_span = glGetUniformLocation(prog_tri, "u_con_span");
    }
    prog_mask = link_program(vs_line, fs_mask);
    if(prog_mask) {
        loc_mask_disp = glGetUniformLocation(prog_mask, "u_disp");
        loc_mask_rect = glGetUniformLocation(prog_mask, "u_rect");
        loc_mask_radius = glGetUniformLocation(prog_mask, "u_radius");
        loc_mask_keep_out = glGetUniformLocation(prog_mask, "u_keep_out");
    }
    prog_blur = link_program(vs_line, fs_blur);
    if(prog_blur) {
        loc_blur_disp = glGetUniformLocation(prog_blur, "u_disp");
        loc_blur_src = glGetUniformLocation(prog_blur, "u_src");
        loc_blur_dir = glGetUniformLocation(prog_blur, "u_dir");
        loc_blur_radius = glGetUniformLocation(prog_blur, "u_radius");
        loc_blur_step = glGetUniformLocation(prog_blur, "u_step");
        loc_blur_rect = glGetUniformLocation(prog_blur, "u_rect");
        loc_blur_corner = glGetUniformLocation(prog_blur, "u_corner_r");
        loc_blur_use_corner = glGetUniformLocation(prog_blur, "u_use_corner");
    }
    prog_tex = link_program(vs_tex, fs_tex);
    if(prog_tex) {
        loc_tex_disp = glGetUniformLocation(prog_tex, "u_disp");
        loc_tex_sampler = glGetUniformLocation(prog_tex, "u_tex");
    }
    prog_glyph = link_program(vs_tex, fs_glyph);
    if(prog_glyph) {
        loc_glyph_disp = glGetUniformLocation(prog_glyph, "u_disp");
        loc_glyph_sampler = glGetUniformLocation(prog_glyph, "u_tex");
        loc_glyph_color = glGetUniformLocation(prog_glyph, "u_color");
        loc_glyph_opa = glGetUniformLocation(prog_glyph, "u_opa");
        loc_glyph_sdf = glGetUniformLocation(prog_glyph, "u_sdf");
        loc_glyph_smooth = glGetUniformLocation(prog_glyph, "u_smooth");
        GL_CALL(glBindAttribLocation(prog_glyph, 1, "a_uv"));
    }
    prog_img = link_program(vs_tex, fs_img);
    if(prog_img) {
        loc_img_disp = glGetUniformLocation(prog_img, "u_disp");
        loc_img_sampler = glGetUniformLocation(prog_img, "u_tex");
        loc_img_opa = glGetUniformLocation(prog_img, "u_opa");
        loc_img_recolor = glGetUniformLocation(prog_img, "u_recolor");
        loc_img_use_recolor = glGetUniformLocation(prog_img, "u_use_recolor");
        loc_img_mask_sampler = glGetUniformLocation(prog_img, "u_mask");
        loc_img_use_mask = glGetUniformLocation(prog_img, "u_use_mask");
        loc_img_mask_uv = glGetUniformLocation(prog_img, "u_mask_uv");
        loc_img_use_diff = glGetUniformLocation(prog_img, "u_use_diff");
        loc_img_dst_sampler = glGetUniformLocation(prog_img, "u_dst");
        loc_img_dst_rect = glGetUniformLocation(prog_img, "u_dst_rect");
        GL_CALL(glBindAttribLocation(prog_img, 1, "a_uv"));
    }
    lv_gpu_glyph_atlas_init();
    lv_draw_buf_init(&g_raster_buf, 0, 0, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO, NULL, 0);
}

void lv_gpu_renderer_gles2_2d_deinit(void)
{
    if(prog_fill) GL_CALL(glDeleteProgram(prog_fill));
    if(prog_line) GL_CALL(glDeleteProgram(prog_line));
    if(prog_arc) GL_CALL(glDeleteProgram(prog_arc));
    if(prog_shadow) GL_CALL(glDeleteProgram(prog_shadow));
    if(prog_tri) GL_CALL(glDeleteProgram(prog_tri));
    if(prog_mask) GL_CALL(glDeleteProgram(prog_mask));
    if(prog_blur) GL_CALL(glDeleteProgram(prog_blur));
    if(prog_tex) GL_CALL(glDeleteProgram(prog_tex));
    if(prog_glyph) GL_CALL(glDeleteProgram(prog_glyph));
    if(prog_img) GL_CALL(glDeleteProgram(prog_img));
    prog_fill = prog_line = prog_arc = prog_shadow = prog_tri = prog_mask = prog_blur = prog_tex = 0;
    prog_glyph = prog_img = 0;
    if(g_img_dst_snap_tex) GL_CALL(glDeleteTextures(1, &g_img_dst_snap_tex));
    g_img_dst_snap_tex = 0;
    g_img_dst_snap_w = g_img_dst_snap_h = 0;
    g_img_dst_snap_x = g_img_dst_snap_y = 0;
    if(g_blur_temp_tex) {
        GL_CALL(glDeleteTextures(1, &g_blur_temp_tex));
        g_blur_temp_tex = 0;
    }
    if(g_blur_temp_fbo) {
        GL_CALL(glDeleteFramebuffers(1, &g_blur_temp_fbo));
        g_blur_temp_fbo = 0;
    }
    if(g_gpu2d_grad_lut) {
        GL_CALL(glDeleteTextures(1, &g_gpu2d_grad_lut));
        g_gpu2d_grad_lut = 0;
    }
    lv_gpu_glyph_atlas_deinit();
    g_blur_temp_w = g_blur_temp_h = 0;
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
    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = opa;
    dsc.radius = radius;
    return lv_gpu_renderer_gles2_2d_queue_fill_dsc(area, clip, &dsc);
}

bool lv_gpu_renderer_gles2_2d_queue_fill_dsc(const lv_area_t * area, const lv_area_t * clip,
                                              const lv_draw_fill_dsc_t * dsc)
{
    if(!dsc) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_FILL;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.fill = *dsc;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_line(const lv_area_t * area, const lv_area_t * clip,
                                          const lv_draw_line_dsc_t * dsc)
{
    if(!dsc) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_LINE;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.line = *dsc;
    cmd.line_pts_n = 0;
    if(dsc->points && dsc->point_cnt >= 2) {
        int32_t n = dsc->point_cnt;
        if(n > LV_GPU_RENDERER_GLES2_LINE_PT_MAX) n = LV_GPU_RENDERER_GLES2_LINE_PT_MAX;
        lv_memcpy(cmd.line_pts, dsc->points, (size_t)n * sizeof(lv_point_precise_t));
        cmd.line_pts_n = (uint8_t)n;
        cmd.u.line.points = NULL;
        cmd.u.line.point_cnt = n;
    }
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_arc(const lv_area_t * area, const lv_area_t * clip,
                                         const lv_draw_arc_dsc_t * dsc)
{
    if(!dsc) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_ARC;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.arc = *dsc;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_box_shadow(const lv_area_t * area, const lv_area_t * clip,
                                                const lv_draw_box_shadow_dsc_t * dsc)
{
    if(!dsc) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_BOX_SHADOW;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.box_shadow = *dsc;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_triangle(const lv_area_t * area, const lv_area_t * clip,
                                                const lv_draw_triangle_dsc_t * dsc)
{
    if(!dsc) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_TRIANGLE;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.triangle = *dsc;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_mask_rect(const lv_area_t * area, const lv_area_t * clip,
                                               const lv_draw_mask_rect_dsc_t * dsc)
{
    if(!dsc) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_MASK_RECT;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.mask_rect = *dsc;
    return queue_push(&cmd);
}

bool lv_gpu_renderer_gles2_2d_queue_blur(const lv_area_t * area, const lv_area_t * clip,
                                          const lv_draw_blur_dsc_t * dsc, const lv_area_t * coords)
{
    if(!dsc || !coords) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_BLUR;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.blur.blur = *dsc;
    cmd.u.blur.coords = *coords;
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

#if LV_USE_VECTOR_GRAPHIC
bool lv_gpu_renderer_gles2_2d_queue_vector(const lv_area_t * area, const lv_area_t * clip,
                                              const lv_draw_vector_dsc_t * dsc)
{
    if(!dsc) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_VECTOR;
    cmd.area = *area;
    cmd.clip = *clip;
    cmd.u.vector = *dsc;
    return queue_push(&cmd);
}
#endif

bool lv_gpu_renderer_gles2_2d_queue_mask_bitmap(const lv_area_t * area, const lv_area_t * clip,
                                                   const lv_image_dsc_t * mask_src,
                                                   const lv_area_t * mask_area, const lv_area_t * blend_area)
{
    if(!mask_src || !mask_area || !blend_area) return false;
    lv_gpu2d_cmd_t cmd;
    lv_memzero(&cmd, sizeof(cmd));
    cmd.type = LV_GPU_RENDERER_GLES2_CMD_MASK_BITMAP;
    cmd.area = *blend_area;
    cmd.clip = *clip;
    cmd.u.mask_bitmap.mask_src = mask_src;
    cmd.u.mask_bitmap.mask_area = *mask_area;
    cmd.u.mask_bitmap.blend_area = *blend_area;
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

static void area_to_verts(const lv_area_t * area, float * verts)
{
    float x1 = (float)area->x1;
    float y1 = (float)area->y1;
    float x2 = (float)area->x2 + 1.0f;
    float y2 = (float)area->y2 + 1.0f;
    verts[0] = x1; verts[1] = y1;
    verts[2] = x2; verts[3] = y1;
    verts[4] = x2; verts[5] = y2;
    verts[6] = x1; verts[7] = y1;
    verts[8] = x2; verts[9] = y2;
    verts[10] = x1; verts[11] = y2;
}

static int fill_grad_mode(const lv_draw_fill_dsc_t * fd)
{
    if(!fd || fd->grad.dir == LV_GRAD_DIR_NONE) return 0;
    if(fd->grad.dir == LV_GRAD_DIR_VER) return 1;
    if(fd->grad.dir == LV_GRAD_DIR_HOR) return 2;
    if(fd->grad.dir == LV_GRAD_DIR_LINEAR) return 3;
    if(fd->grad.dir == LV_GRAD_DIR_RADIAL) return 4;
    if(fd->grad.dir == LV_GRAD_DIR_CONICAL) return 5;
    return -1;
}

static lv_color32_t grad_stop_color32(const lv_grad_dsc_t * grad, uint32_t idx, lv_opa_t fill_opa)
{
    if(idx >= grad->stops_count) return lv_color_to_32(lv_color_black(), fill_opa);
    const lv_grad_stop_t * s = &grad->stops[idx];
    lv_opa_t opa = LV_OPA_MIX2(fill_opa, s->opa);
    return lv_color_to_32(s->color, opa);
}

static bool gpu2d_grad_lut_upload(const lv_grad_dsc_t * grad, lv_opa_t fill_opa);
static void triangle_radial_uniforms(const lv_grad_dsc_t * grad, const lv_area_t * area,
                                      float * x0, float * y0, float * r0,
                                      float * a4, float * bpx, float * bpy, float * bc, float * inv_dr);
static void triangle_conical_uniforms(const lv_grad_dsc_t * grad, const lv_area_t * area,
                                       float * cx, float * cy, float * start, float * span);

static bool draw_fill_gpu(const lv_area_t * area, int32_t dw, int32_t dh, const lv_draw_fill_dsc_t * fd)
{
    if(!prog_fill || !fd) return false;
    const int grad_mode = fill_grad_mode(fd);
    if(grad_mode < 0) return false;

    if((grad_mode == 4 || grad_mode == 5) && !gpu2d_grad_lut_upload(&fd->grad, fd->opa)) {
        return false;
    }

    float verts[12];
    area_to_verts(area, verts);
    float x1 = verts[0];
    float y1 = verts[1];
    float x2 = verts[4];
    float y2 = verts[11];
    float rw = x2 - x1;
    float rh = y2 - y1;
    float rad = (float)fd->radius;
    if(rad > rw * 0.5f) rad = rw * 0.5f;
    if(rad > rh * 0.5f) rad = rh * 0.5f;

    lv_color32_t c0 = grad_mode == 0 ? lv_color_to_32(fd->color, fd->opa) : grad_stop_color32(&fd->grad, 0, fd->opa);
    lv_color32_t c1 = grad_mode == 0 ? c0 : grad_stop_color32(&fd->grad, grad_mode == 0 ? 0 : 1, fd->opa);

    GL_CALL(glUseProgram(prog_fill));
    GL_CALL(glUniform2f(loc_fill_disp, (float)dw, (float)dh));
    gpu_comp_uniform_rgba(loc_fill_color, c0);
    gpu_comp_uniform_rgba(loc_fill_color1, c1);
    GL_CALL(glUniform4f(loc_fill_rect, x1, y1, rw, rh));
    GL_CALL(glUniform1f(loc_fill_radius, rad));
    GL_CALL(glUniform1i(loc_fill_grad_mode, grad_mode));
    if(grad_mode == 3) {
        const lv_area_t rel = *area;
        float gx0 = (float)(rel.x1 + fd->grad.params.linear.start.x);
        float gy0 = (float)(rel.y1 + fd->grad.params.linear.start.y);
        float gx1 = (float)(rel.x1 + fd->grad.params.linear.end.x);
        float gy1 = (float)(rel.y1 + fd->grad.params.linear.end.y);
        GL_CALL(glUniform2f(loc_fill_grad_p0, gx0, gy0));
        GL_CALL(glUniform2f(loc_fill_grad_p1, gx1, gy1));
    }
    else if(grad_mode == 4) {
        float rx0, ry0, rr0, ra4, rbpx, rbpy, rbc, rinv_dr;
        triangle_radial_uniforms(&fd->grad, area, &rx0, &ry0, &rr0, &ra4, &rbpx, &rbpy, &rbc, &rinv_dr);
        GL_CALL(glUniform1i(loc_fill_grad_extend, (int)fd->grad.extend));
        GL_CALL(glUniform1f(loc_fill_rad_x0, rx0));
        GL_CALL(glUniform1f(loc_fill_rad_y0, ry0));
        GL_CALL(glUniform1f(loc_fill_rad_r0, rr0));
        GL_CALL(glUniform1f(loc_fill_rad_a4, ra4));
        GL_CALL(glUniform1f(loc_fill_rad_bpx, rbpx));
        GL_CALL(glUniform1f(loc_fill_rad_bpy, rbpy));
        GL_CALL(glUniform1f(loc_fill_rad_bc, rbc));
        GL_CALL(glUniform1f(loc_fill_rad_inv_dr, rinv_dr));
        GL_CALL(glActiveTexture(GL_TEXTURE1));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, g_gpu2d_grad_lut));
        GL_CALL(glUniform1i(loc_fill_grad_lut, 1));
        GL_CALL(glActiveTexture(GL_TEXTURE0));
        GL_CALL(glUniform2f(loc_fill_grad_p0, 0.0f, 0.0f));
        GL_CALL(glUniform2f(loc_fill_grad_p1, 0.0f, 0.0f));
    }
    else if(grad_mode == 5) {
        float ccx, ccy, cstart, cspan;
        triangle_conical_uniforms(&fd->grad, area, &ccx, &ccy, &cstart, &cspan);
        GL_CALL(glUniform1i(loc_fill_grad_extend, (int)fd->grad.extend));
        GL_CALL(glUniform1f(loc_fill_con_cx, ccx));
        GL_CALL(glUniform1f(loc_fill_con_cy, ccy));
        GL_CALL(glUniform1f(loc_fill_con_start, cstart));
        GL_CALL(glUniform1f(loc_fill_con_span, cspan));
        GL_CALL(glActiveTexture(GL_TEXTURE1));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, g_gpu2d_grad_lut));
        GL_CALL(glUniform1i(loc_fill_grad_lut, 1));
        GL_CALL(glActiveTexture(GL_TEXTURE0));
        GL_CALL(glUniform2f(loc_fill_grad_p0, 0.0f, 0.0f));
        GL_CALL(glUniform2f(loc_fill_grad_p1, 0.0f, 0.0f));
    }
    else {
        GL_CALL(glUniform2f(loc_fill_grad_p0, 0.0f, 0.0f));
        GL_CALL(glUniform2f(loc_fill_grad_p1, 0.0f, 0.0f));
    }
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
    if(grad_mode == 4 || grad_mode == 5) {
        GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    }
    return true;
}

static bool draw_line_segment_gpu(const lv_area_t * area, int32_t dw, int32_t dh,
                                   const lv_draw_line_dsc_t * ld, bool round_start, bool round_end)
{
    if(!prog_line || !ld || ld->width < 1 || ld->opa <= LV_OPA_MIN) return false;

    float verts[12];
    area_to_verts(area, verts);
    lv_color32_t c32 = lv_color_to_32(ld->color, ld->opa);
    const bool has_dash = ld->dash_width > 0 && ld->dash_gap > 0;

    GL_CALL(glUseProgram(prog_line));
    GL_CALL(glUniform2f(loc_line_disp, (float)dw, (float)dh));
    gpu_comp_uniform_rgba(loc_line_color, c32);
    GL_CALL(glUniform2f(loc_line_p1, (float)ld->p1.x, (float)ld->p1.y));
    GL_CALL(glUniform2f(loc_line_p2, (float)ld->p2.x, (float)ld->p2.y));
    GL_CALL(glUniform1f(loc_line_width, (float)ld->width));
    GL_CALL(glUniform1f(loc_line_round, (round_start || round_end) ? 1.0f : 0.0f));
    GL_CALL(glUniform1f(loc_line_dash_on, (float)ld->dash_width));
    GL_CALL(glUniform1f(loc_line_dash_gap, (float)ld->dash_gap));
    GL_CALL(glUniform1i(loc_line_has_dash, has_dash ? 1 : 0));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
    return true;
}

static bool draw_line_gpu(const lv_area_t * area, int32_t dw, int32_t dh, const lv_gpu2d_cmd_t * cmd)
{
    const lv_draw_line_dsc_t * ld = &cmd->u.line;
    if(!ld || ld->width < 1 || ld->opa <= LV_OPA_MIN) return false;

    if(cmd->line_pts_n >= 2) {
        lv_draw_line_dsc_t seg = *ld;
        seg.points = NULL;
        seg.point_cnt = 0;
        for(uint8_t i = 0; i + 1 < cmd->line_pts_n; i++) {
            seg.p1 = cmd->line_pts[i];
            seg.p2 = cmd->line_pts[i + 1];
            const bool rs = (i == 0) ? ld->round_start : false;
            const bool re = (i + 2 >= cmd->line_pts_n) ? ld->round_end : false;
            if(!draw_line_segment_gpu(area, dw, dh, &seg, rs, re)) return false;
        }
        return true;
    }

    return draw_line_segment_gpu(area, dw, dh, ld, ld->round_start, ld->round_end);
}

static int triangle_grad_mode(const lv_draw_triangle_dsc_t * td)
{
    if(!td || td->grad.dir == LV_GRAD_DIR_NONE) return 0;
    if(td->grad.dir == LV_GRAD_DIR_VER) return 1;
    if(td->grad.dir == LV_GRAD_DIR_HOR) return 2;
    if(td->grad.dir == LV_GRAD_DIR_LINEAR) return 3;
    if(td->grad.dir == LV_GRAD_DIR_RADIAL) return 4;
    if(td->grad.dir == LV_GRAD_DIR_CONICAL) return 5;
    return -1;
}

static bool gpu2d_grad_lut_upload(const lv_grad_dsc_t * grad, lv_opa_t fill_opa)
{
#if LV_USE_DRAW_SW
    lv_draw_sw_grad_calc_t * lut = lv_draw_sw_grad_get(grad, 256, 0);
    if(!lut) return false;

    uint8_t px[256 * 4];
    for(uint32_t i = 0; i < 256; i++) {
        lv_opa_t opa = LV_OPA_MIX2(fill_opa, lut->opa_map[i]);
        lv_color32_t c32 = lv_color_to_32(lut->color_map[i], opa);
        px[i * 4 + 0] = c32.blue;
        px[i * 4 + 1] = c32.green;
        px[i * 4 + 2] = c32.red;
        px[i * 4 + 3] = c32.alpha;
    }
    lv_draw_sw_grad_cleanup(lut);

    if(g_gpu2d_grad_lut == 0) {
        GL_CALL(glGenTextures(1, &g_gpu2d_grad_lut));
    }
    GL_CALL(glBindTexture(GL_TEXTURE_2D, g_gpu2d_grad_lut));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    return true;
#else
    LV_UNUSED(grad);
    LV_UNUSED(fill_opa);
    return false;
#endif
}

static void triangle_radial_uniforms(const lv_grad_dsc_t * grad, const lv_area_t * area,
                                      float * x0, float * y0, float * r0,
                                      float * a4, float * bpx, float * bpy, float * bc, float * inv_dr)
{
    const int32_t wdt = lv_area_get_width(area);
    const int32_t hgt = lv_area_get_height(area);
    const float ox = (float)area->x1;
    const float oy = (float)area->y1;

    lv_point_t start = grad->params.radial.focal;
    lv_point_t end = grad->params.radial.end;
    lv_point_t start_extent = grad->params.radial.focal_extent;
    lv_point_t end_extent = grad->params.radial.end_extent;

    const float fx0 = (float)lv_pct_to_px(start.x, wdt) + ox;
    const float fy0 = (float)lv_pct_to_px(start.y, hgt) + oy;
    const float fx1 = (float)lv_pct_to_px(end.x, wdt) + ox;
    const float fy1 = (float)lv_pct_to_px(end.y, hgt) + oy;
    const float fsex = (float)lv_pct_to_px(start_extent.x, wdt) + ox;
    const float fsey = (float)lv_pct_to_px(start_extent.y, hgt) + oy;
    const float feex = (float)lv_pct_to_px(end_extent.x, wdt) + ox;
    const float feey = (float)lv_pct_to_px(end_extent.y, hgt) + oy;

    const float fr0 = (float)lv_sqrt32(lv_sqr((int32_t)(fsex - fx0)) + lv_sqr((int32_t)(fsey - fy0)));
    float fr1 = (float)lv_sqrt32(lv_sqr((int32_t)(feex - fx1)) + lv_sqr((int32_t)(feey - fy1)));
    if(fr1 < 0.001f) fr1 = 0.001f;

    const float dx = fx1 - fx0;
    const float dy = fy1 - fy0;
    const float dr = fr1 - fr0;

    *x0 = fx0;
    *y0 = fy0;
    *r0 = fr0;
    *bpx = 2.0f * dx;
    *bpy = 2.0f * dy;
    *bc = 2.0f * (fr0 * dr - fx0 * dx - fy0 * dy);
    *a4 = (float)((int32_t)lv_sqr((int32_t)dr) - lv_sqr((int32_t)dx) - lv_sqr((int32_t)dy));
    *inv_dr = (dr > 0.001f || dr < -0.001f) ? (1.0f / dr) : 0.0f;
}

static void triangle_conical_uniforms(const lv_grad_dsc_t * grad, const lv_area_t * area,
                                       float * cx, float * cy, float * start, float * span)
{
    const int32_t wdt = lv_area_get_width(area);
    const int32_t hgt = lv_area_get_height(area);

    *cx = (float)lv_pct_to_px(grad->params.conical.center.x, wdt) + (float)area->x1;
    *cy = (float)lv_pct_to_px(grad->params.conical.center.y, hgt) + (float)area->y1;
    float alpha = (float)(grad->params.conical.start_angle % 360);
    float beta = (float)(grad->params.conical.end_angle % 360);
    if(beta <= alpha) beta += 360.0f;
    *start = alpha;
    *span = beta - alpha;
}

static bool draw_triangle_gpu(const lv_area_t * area, int32_t dw, int32_t dh, const lv_draw_triangle_dsc_t * td)
{
    if(!prog_tri || !td || td->opa <= LV_OPA_MIN) return false;
    const int grad_mode = triangle_grad_mode(td);
    if(grad_mode < 0) return false;

    if((grad_mode == 4 || grad_mode == 5) && !gpu2d_grad_lut_upload(&td->grad, td->opa)) {
        return false;
    }

    float verts[12];
    area_to_verts(area, verts);
    lv_color32_t c0 = grad_mode == 0 ? lv_color_to_32(td->color, td->opa)
                                     : grad_stop_color32(&td->grad, 0, td->opa);
    lv_color32_t c1 = grad_mode == 0 ? c0 : grad_stop_color32(&td->grad, 1, td->opa);

    GL_CALL(glUseProgram(prog_tri));
    GL_CALL(glUniform2f(loc_tri_disp, (float)dw, (float)dh));
    gpu_comp_uniform_rgba(loc_tri_color, c0);
    gpu_comp_uniform_rgba(loc_tri_color1, c1);
    GL_CALL(glUniform2f(loc_tri_p0, (float)td->p[0].x, (float)td->p[0].y));
    GL_CALL(glUniform2f(loc_tri_p1, (float)td->p[1].x, (float)td->p[1].y));
    GL_CALL(glUniform2f(loc_tri_p2, (float)td->p[2].x, (float)td->p[2].y));
    GL_CALL(glUniform1i(loc_tri_grad_mode, grad_mode));
    if(grad_mode == 1 || grad_mode == 2) {
        float y0 = (float)area->y1;
        float y1 = (float)area->y2 + 1.0f;
        float x0 = (float)area->x1;
        float x1 = (float)area->x2 + 1.0f;
        GL_CALL(glUniform2f(loc_tri_grad_p0, x0, y0));
        GL_CALL(glUniform2f(loc_tri_grad_p1, x1, y1));
    }
    else if(grad_mode == 3) {
        float gx0 = (float)area->x1 + td->grad.params.linear.start.x;
        float gy0 = (float)area->y1 + td->grad.params.linear.start.y;
        float gx1 = (float)area->x1 + td->grad.params.linear.end.x;
        float gy1 = (float)area->y1 + td->grad.params.linear.end.y;
        GL_CALL(glUniform2f(loc_tri_grad_p0, gx0, gy0));
        GL_CALL(glUniform2f(loc_tri_grad_p1, gx1, gy1));
    }
    else if(grad_mode == 4) {
        float rx0, ry0, rr0, ra4, rbpx, rbpy, rbc, rinv_dr;
        triangle_radial_uniforms(&td->grad, area, &rx0, &ry0, &rr0, &ra4, &rbpx, &rbpy, &rbc, &rinv_dr);
        GL_CALL(glUniform1i(loc_tri_grad_extend, (int)td->grad.extend));
        GL_CALL(glUniform1f(loc_tri_rad_x0, rx0));
        GL_CALL(glUniform1f(loc_tri_rad_y0, ry0));
        GL_CALL(glUniform1f(loc_tri_rad_r0, rr0));
        GL_CALL(glUniform1f(loc_tri_rad_a4, ra4));
        GL_CALL(glUniform1f(loc_tri_rad_bpx, rbpx));
        GL_CALL(glUniform1f(loc_tri_rad_bpy, rbpy));
        GL_CALL(glUniform1f(loc_tri_rad_bc, rbc));
        GL_CALL(glUniform1f(loc_tri_rad_inv_dr, rinv_dr));
        GL_CALL(glActiveTexture(GL_TEXTURE1));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, g_gpu2d_grad_lut));
        GL_CALL(glUniform1i(loc_tri_grad_lut, 1));
        GL_CALL(glActiveTexture(GL_TEXTURE0));
    }
    else if(grad_mode == 5) {
        float ccx, ccy, cstart, cspan;
        triangle_conical_uniforms(&td->grad, area, &ccx, &ccy, &cstart, &cspan);
        GL_CALL(glUniform1i(loc_tri_grad_extend, (int)td->grad.extend));
        GL_CALL(glUniform1f(loc_tri_con_cx, ccx));
        GL_CALL(glUniform1f(loc_tri_con_cy, ccy));
        GL_CALL(glUniform1f(loc_tri_con_start, cstart));
        GL_CALL(glUniform1f(loc_tri_con_span, cspan));
        GL_CALL(glActiveTexture(GL_TEXTURE1));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, g_gpu2d_grad_lut));
        GL_CALL(glUniform1i(loc_tri_grad_lut, 1));
        GL_CALL(glActiveTexture(GL_TEXTURE0));
    }
    else {
        GL_CALL(glUniform2f(loc_tri_grad_p0, 0.0f, 0.0f));
        GL_CALL(glUniform2f(loc_tri_grad_p1, 0.0f, 0.0f));
    }
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
    if(grad_mode == 4 || grad_mode == 5) {
        GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    }
    return true;
}

static bool draw_mask_gpu(const lv_area_t * area, int32_t dw, int32_t dh, const lv_draw_mask_rect_dsc_t * md)
{
    if(!prog_mask || !md) return false;

    float verts[12];
    area_to_verts(area, verts);
    const lv_area_t * ma = &md->area;
    float rw = (float)lv_area_get_width(ma);
    float rh = (float)lv_area_get_height(ma);

    GL_CALL(glUseProgram(prog_mask));
    GL_CALL(glUniform2f(loc_mask_disp, (float)dw, (float)dh));
    GL_CALL(glUniform4f(loc_mask_rect, (float)ma->x1, (float)ma->y1, rw, rh));
    GL_CALL(glUniform1f(loc_mask_radius, (float)md->radius));
    GL_CALL(glUniform1f(loc_mask_keep_out, md->keep_outside ? 1.0f : 0.0f));

    GLenum src_rgb = GL_ZERO;
    GLenum dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
    GLenum src_a = GL_ZERO;
    GLenum dst_a = GL_ONE_MINUS_SRC_ALPHA;
    GL_CALL(glBlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a));

    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));

    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    return true;
}

static bool blur_temp_ensure(int32_t w, int32_t h)
{
    if(w < 1 || h < 1) return false;
    if(g_blur_temp_tex && g_blur_temp_fbo && g_blur_temp_w == w && g_blur_temp_h == h) return true;

    if(g_blur_temp_tex) {
        GL_CALL(glDeleteTextures(1, &g_blur_temp_tex));
        g_blur_temp_tex = 0;
    }
    if(g_blur_temp_fbo) {
        GL_CALL(glDeleteFramebuffers(1, &g_blur_temp_fbo));
        g_blur_temp_fbo = 0;
    }

    GL_CALL(glGenTextures(1, &g_blur_temp_tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, g_blur_temp_tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    GL_CALL(glGenFramebuffers(1, &g_blur_temp_fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, g_blur_temp_fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_blur_temp_tex, 0));
#if !LV_USE_EGL
    {
        GLenum db = GL_COLOR_ATTACHMENT0;
        GL_CALL(glDrawBuffers(1, &db));
    }
#endif
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    if(!ok) return false;

    g_blur_temp_w = w;
    g_blur_temp_h = h;
    return true;
}

static bool draw_blur_pass(const lv_area_t * area, int32_t dw, int32_t dh, unsigned int src_tex,
                            float dir_x, float dir_y, float radius, float step, unsigned int dst_fbo,
                            const lv_area_t * blur_coords, float corner_r, bool corner_mask)
{
    if(!prog_blur || src_tex == 0) return false;

    float verts[12];
    area_to_verts(area, verts);

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo));
    GL_CALL(glViewport(0, 0, dw, dh));
    GL_CALL(glUseProgram(prog_blur));
    GL_CALL(glUniform2f(loc_blur_disp, (float)dw, (float)dh));
    GL_CALL(glUniform2f(loc_blur_dir, dir_x, dir_y));
    GL_CALL(glUniform1f(loc_blur_radius, radius));
    GL_CALL(glUniform1f(loc_blur_step, step));
    if(blur_coords) {
        float rw = (float)lv_area_get_width(blur_coords);
        float rh = (float)lv_area_get_height(blur_coords);
        GL_CALL(glUniform4f(loc_blur_rect, (float)blur_coords->x1, (float)blur_coords->y1, rw, rh));
    }
    else {
        GL_CALL(glUniform4f(loc_blur_rect, 0.0f, 0.0f, 0.0f, 0.0f));
    }
    GL_CALL(glUniform1f(loc_blur_corner, corner_r));
    GL_CALL(glUniform1i(loc_blur_use_corner, corner_mask ? 1 : 0));
    GL_CALL(glActiveTexture(GL_TEXTURE0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, src_tex));
    GL_CALL(glUniform1i(loc_blur_src, 0));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    return true;
}

static float blur_pass_step(float radius)
{
    return radius > 32.0f ? (radius / 32.0f) : 1.0f;
}

static bool draw_blur_chunk(unsigned int color_tex, int32_t dw, int32_t dh, const lv_area_t * draw_area,
                             int32_t chunk_radius, const lv_area_t * coords, float corner_r, bool apply_corner_mask)
{
    const unsigned int dst_fbo = lv_gpu_renderer_tex_fbo_bind(color_tex);
    const float radius = (float)chunk_radius;
    const float step = blur_pass_step(radius);

    if(!draw_blur_pass(draw_area, dw, dh, color_tex, 1.0f, 0.0f, radius, step, g_blur_temp_fbo,
                       coords, corner_r, false)) {
        return false;
    }
    if(!draw_blur_pass(draw_area, dw, dh, g_blur_temp_tex, 0.0f, 1.0f, radius, step, dst_fbo,
                       coords, corner_r, apply_corner_mask)) {
        return false;
    }
    return true;
}

static bool draw_blur_gpu(unsigned int color_tex, int32_t dw, int32_t dh, const lv_area_t * coords,
                           const lv_area_t * clip, const lv_draw_blur_dsc_t * bd)
{
    if(!prog_blur || !bd || bd->blur_radius < 1) return false;

    lv_area_t draw_area;
    if(!lv_area_intersect(&draw_area, coords, clip)) return false;
    if(!blur_temp_ensure(dw, dh)) return false;
    if(!lv_gpu_renderer_tex_fbo_bind_complete(color_tex)) return false;

    int32_t corner_r = bd->corner_radius;
    const int32_t cw = lv_area_get_width(coords);
    const int32_t ch = lv_area_get_height(coords);
    const int32_t short_side = LV_MIN(cw, ch);
    if(corner_r > short_side >> 1) corner_r = short_side >> 1;

    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_ONE, GL_ZERO));

    int32_t left = bd->blur_radius;
    while(left > 0) {
        const int32_t chunk = left > 64 ? 64 : left;
        const bool last_chunk = (left - chunk) <= 0;
        const bool corner_mask = last_chunk && corner_r > 0;
        if(!draw_blur_chunk(color_tex, dw, dh, &draw_area, chunk, coords, (float)corner_r, corner_mask)) {
            GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            return false;
        }
        left -= chunk;
    }

    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    return true;
}

static bool draw_arc_gpu(const lv_area_t * area, int32_t dw, int32_t dh, const lv_draw_arc_dsc_t * ad)
{
    if(!prog_arc || !ad || ad->width < 1 || ad->opa <= LV_OPA_MIN) return false;

    unsigned int img_tex = 0;
    int32_t img_w = 0;
    int32_t img_h = 0;
    if(ad->img_src) {
        lv_image_decoder_dsc_t dec;
        lv_memzero(&dec, sizeof(dec));
        if(lv_image_decoder_open(&dec, ad->img_src, NULL) != LV_RESULT_OK || !dec.decoded) {
            lv_image_decoder_close(&dec);
            return false;
        }
        if(!upload_decoded_image(dec.decoded, &img_tex)) {
            lv_image_decoder_close(&dec);
            return false;
        }
        img_w = dec.decoded->header.w;
        img_h = dec.decoded->header.h;
        lv_image_decoder_close(&dec);
    }

    float verts[12];
    area_to_verts(area, verts);
    lv_color32_t c32 = lv_color_to_32(ad->color, ad->opa);

    GL_CALL(glUseProgram(prog_arc));
    GL_CALL(glUniform2f(loc_arc_disp, (float)dw, (float)dh));
    gpu_comp_uniform_rgba(loc_arc_color, c32);
    GL_CALL(glUniform2f(loc_arc_center, (float)ad->center.x, (float)ad->center.y));
    GL_CALL(glUniform1f(loc_arc_radius, (float)ad->radius));
    GL_CALL(glUniform1f(loc_arc_width, (float)ad->width));
    GL_CALL(glUniform2f(loc_arc_angles, (float)ad->start_angle, (float)ad->end_angle));
    GL_CALL(glUniform1f(loc_arc_round, ad->rounded ? 1.0f : 0.0f));
    if(img_tex) {
        const float iw = (float)img_w;
        const float ih = (float)img_h;
        const float ox = (float)ad->center.x - iw * 0.5f;
        const float oy = (float)ad->center.y - ih * 0.5f;
        GL_CALL(glUniform1i(loc_arc_use_tex, 1));
        GL_CALL(glUniform4f(loc_arc_img_rect, ox, oy, iw, ih));
        GL_CALL(glActiveTexture(GL_TEXTURE0));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, img_tex));
        GL_CALL(glUniform1i(loc_arc_tex, 0));
    }
    else {
        GL_CALL(glUniform1i(loc_arc_use_tex, 0));
    }
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    if(img_tex) GL_CALL(glDeleteTextures(1, &img_tex));
    return true;
}

static bool draw_box_shadow_gpu(const lv_area_t * coords, const lv_area_t * clip, int32_t dw, int32_t dh,
                                 const lv_draw_box_shadow_dsc_t * sd)
{
    if(!prog_shadow || !sd || sd->width < 1 || sd->opa <= LV_OPA_MIN) return false;

    lv_area_t core;
    core.x1 = coords->x1 + sd->ofs_x - sd->spread;
    core.x2 = coords->x2 + sd->ofs_x + sd->spread;
    core.y1 = coords->y1 + sd->ofs_y - sd->spread;
    core.y2 = coords->y2 + sd->ofs_y + sd->spread;

    lv_area_t shadow_area;
    shadow_area.x1 = core.x1 - sd->width / 2 - 1;
    shadow_area.x2 = core.x2 + sd->width / 2 + 1;
    shadow_area.y1 = core.y1 - sd->width / 2 - 1;
    shadow_area.y2 = core.y2 + sd->width / 2 + 1;

    lv_area_t draw_area;
    if(!lv_area_intersect(&draw_area, &shadow_area, clip)) return false;

    float verts[12];
    area_to_verts(&draw_area, verts);
    float cw = (float)lv_area_get_width(&core);
    float ch = (float)lv_area_get_height(&core);
    float rad = (float)sd->radius;
    if(rad > cw * 0.5f) rad = cw * 0.5f;
    if(rad > ch * 0.5f) rad = ch * 0.5f;

    lv_color32_t c32 = lv_color_to_32(sd->color, sd->opa);
    GL_CALL(glUseProgram(prog_shadow));
    GL_CALL(glUniform2f(loc_shadow_disp, (float)dw, (float)dh));
    gpu_comp_uniform_rgba(loc_shadow_color, c32);
    GL_CALL(glUniform4f(loc_shadow_core, (float)core.x1, (float)core.y1, cw, ch));
    GL_CALL(glUniform1f(loc_shadow_radius, rad));
    GL_CALL(glUniform1f(loc_shadow_blur, (float)sd->width));
    GL_CALL(glUniform1f(loc_shadow_bg_cover, sd->bg_cover ? 1.0f : 0.0f));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(0));
    return true;
}

static void draw_fill_quad(const lv_area_t * area, int32_t dw, int32_t dh,
                           lv_color_t color, lv_opa_t opa, int32_t radius)
{
    lv_draw_fill_dsc_t fd;
    lv_draw_fill_dsc_init(&fd);
    fd.color = color;
    fd.opa = opa;
    fd.radius = radius;
    draw_fill_gpu(area, dw, dh, &fd);
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

static void draw_fill_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    lv_draw_fill(layer, dsc, rel);
}

static void draw_line_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    LV_UNUSED(rel);
    lv_draw_line(layer, dsc);
}

static void draw_arc_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    LV_UNUSED(rel);
    lv_draw_arc(layer, dsc);
}

static void draw_triangle_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    LV_UNUSED(rel);
    lv_draw_triangle(layer, dsc);
}

static void draw_blur_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    lv_draw_blur(layer, dsc, rel);
}

static void draw_box_shadow_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    LV_UNUSED(dsc);
    LV_UNUSED(rel);
    lv_draw_box_shadow(layer, g_shadow_ctx.sd, &g_shadow_ctx.coords);
}

static bool shadow_compute_area(const lv_area_t * coords, const lv_draw_box_shadow_dsc_t * sd, lv_area_t * shadow_out)
{
    if(!coords || !sd || !shadow_out) return false;
    lv_area_t core;
    core.x1 = coords->x1 + sd->ofs_x - sd->spread;
    core.x2 = coords->x2 + sd->ofs_x + sd->spread;
    core.y1 = coords->y1 + sd->ofs_y - sd->spread;
    core.y2 = coords->y2 + sd->ofs_y + sd->spread;
    shadow_out->x1 = core.x1 - sd->width / 2 - 1;
    shadow_out->x2 = core.x2 + sd->width / 2 + 1;
    shadow_out->y1 = core.y1 - sd->width / 2 - 1;
    shadow_out->y2 = core.y2 + sd->width / 2 + 1;
    return true;
}

static bool raster_shadow_sw(const lv_area_t * coords, const lv_area_t * clip,
                              const lv_draw_box_shadow_dsc_t * sd, unsigned int * tex_out)
{
    lv_area_t shadow_area;
    if(!shadow_compute_area(coords, sd, &shadow_area)) return false;
    g_shadow_ctx.sd = sd;
    g_shadow_ctx.coords = *coords;
    return raster_sw_to_texture(&shadow_area, clip, draw_box_shadow_sw_cb, (void *)sd, tex_out);
}

static void restore_blend_mode(void)
{
    GL_CALL(glBlendEquation(GL_FUNC_ADD));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
}

static void apply_blend_mode(lv_blend_mode_t mode)
{
    GL_CALL(glEnable(GL_BLEND));
    switch(mode) {
        case LV_BLEND_MODE_ADDITIVE:
            GL_CALL(glBlendEquation(GL_FUNC_ADD));
            GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE));
            break;
        case LV_BLEND_MODE_MULTIPLY:
            GL_CALL(glBlendEquation(GL_FUNC_ADD));
            GL_CALL(glBlendFunc(GL_DST_COLOR, GL_ZERO));
            break;
        case LV_BLEND_MODE_SUBTRACTIVE:
            GL_CALL(glBlendEquation(GL_FUNC_REVERSE_SUBTRACT));
            GL_CALL(glBlendFunc(GL_ONE, GL_ONE));
            break;
        case LV_BLEND_MODE_DIFFERENCE:
            GL_CALL(glBlendEquation(GL_FUNC_ADD));
            GL_CALL(glBlendFunc(GL_ONE, GL_ZERO));
            break;
        case LV_BLEND_MODE_NORMAL:
        default:
            GL_CALL(glBlendEquation(GL_FUNC_ADD));
            GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
    }
}

static void quad_bounds(const float * pos, float * minx, float * miny, float * maxx, float * maxy)
{
    *minx = *miny = 1e9f;
    *maxx = *maxy = -1e9f;
    for(int i = 0; i < 6; i++) {
        const float x = pos[i * 2];
        const float y = pos[i * 2 + 1];
        if(x < *minx) *minx = x;
        if(y < *miny) *miny = y;
        if(x > *maxx) *maxx = x;
        if(y > *maxy) *maxy = y;
    }
}

static bool ensure_dst_snap_tex(int32_t w, int32_t h)
{
    if(w < 1 || h < 1) return false;
    if(g_img_dst_snap_tex && g_img_dst_snap_w == w && g_img_dst_snap_h == h) return true;
    if(g_img_dst_snap_tex) GL_CALL(glDeleteTextures(1, &g_img_dst_snap_tex));
    GL_CALL(glGenTextures(1, &g_img_dst_snap_tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, g_img_dst_snap_tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    g_img_dst_snap_w = w;
    g_img_dst_snap_h = h;
    return g_img_dst_snap_tex != 0;
}

static bool snap_dst_for_quad(int32_t dw, int32_t dh, const float * pos)
{
    float minx, miny, maxx, maxy;
    quad_bounds(pos, &minx, &miny, &maxx, &maxy);
    int32_t x = (int32_t)floorf(minx);
    int32_t y = (int32_t)floorf(miny);
    int32_t w = (int32_t)ceilf(maxx) - x + 1;
    int32_t h = (int32_t)ceilf(maxy) - y + 1;
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x + w > dw) w = dw - x;
    if(y + h > dh) h = dh - y;
    if(w < 1 || h < 1) return false;
    if(!ensure_dst_snap_tex(w, h)) return false;
    g_img_dst_snap_x = x;
    g_img_dst_snap_y = y;
    const int32_t gl_y = dh - y - h;
    GL_CALL(glBindTexture(GL_TEXTURE_2D, g_img_dst_snap_tex));
    GL_CALL(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, gl_y, w, h));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    return true;
}

static void compute_mask_uv(const lv_draw_image_dsc_t * dsc, const lv_area_t * coords, int32_t img_w, int32_t img_h,
                             float uv_out[4])
{
    const lv_image_dsc_t * mask = dsc->bitmap_mask_src;
    lv_area_t image_area;
    if(lv_area_get_width(&dsc->image_area) >= 0) image_area = dsc->image_area;
    else image_area = *coords;

    lv_area_t mask_area;
    lv_area_set(&mask_area, 0, 0, (int32_t)mask->header.w - 1, (int32_t)mask->header.h - 1);
    lv_area_align(&image_area, &mask_area, LV_ALIGN_CENTER, 0, 0);

    const float inv_mw = 1.0f / (float)mask->header.w;
    const float inv_mh = 1.0f / (float)mask->header.h;
    uv_out[0] = ((float)coords->x1 - (float)mask_area.x1) * inv_mw;
    uv_out[1] = ((float)coords->y1 - (float)mask_area.y1) * inv_mh;
    uv_out[2] = ((float)coords->x1 + (float)img_w - (float)mask_area.x1) * inv_mw;
    uv_out[3] = ((float)coords->y1 + (float)img_h - (float)mask_area.y1) * inv_mh;
}

static bool upload_mask_from_decoded(const lv_draw_buf_t * decoded, unsigned int * tex_out)
{
    if(!decoded || !decoded->data || !tex_out) return false;
    const lv_image_dsc_t wrap = {
        .header = decoded->header,
        .data_size = decoded->data_size,
        .data = decoded->data,
    };
    return upload_mask_texture(&wrap, tex_out);
}

static bool upload_mask_texture(const void * mask_src, unsigned int * tex_out)
{
    if(!mask_src || !tex_out) return false;

    const lv_image_dsc_t * mask = mask_src;
    if(lv_image_src_get_type(mask_src) == LV_IMAGE_SRC_VARIABLE && mask->data) {
        unsigned int tex = 0;
        GL_CALL(glGenTextures(1, &tex));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

        if(mask->header.cf == LV_COLOR_FORMAT_A8 || mask->header.cf == LV_COLOR_FORMAT_L8) {
            const GLenum fmt = GL_ALPHA;
            GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, fmt, mask->header.w, mask->header.h, 0, fmt,
                                 GL_UNSIGNED_BYTE, mask->data));
        }
        else if(mask->header.cf == LV_COLOR_FORMAT_ARGB8888 || mask->header.cf == LV_COLOR_FORMAT_XRGB8888
                || mask->header.cf == LV_COLOR_FORMAT_RGB888) {
            lv_opengles_teximage_bgra8888(0, mask->header.w, mask->header.h, mask->data, mask->header.stride);
        }
        else if(mask->header.cf == LV_COLOR_FORMAT_RGB565A8) {
            GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mask->header.w, mask->header.h, 0,
                                 GL_RGBA, GL_UNSIGNED_BYTE, NULL));
            const uint8_t * rgb = mask->data;
            const uint8_t * a8 = mask->data + mask->header.stride * mask->header.h;
            const int32_t rgb_stride = mask->header.stride;
            lv_draw_buf_t tmp;
            uint32_t sz = LV_DRAW_BUF_SIZE(mask->header.w, mask->header.h, LV_COLOR_FORMAT_ARGB8888);
            uint8_t * mem = lv_malloc(sz);
            if(!mem) {
                GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
                GL_CALL(glDeleteTextures(1, &tex));
                return false;
            }
            lv_draw_buf_init(&tmp, mask->header.w, mask->header.h, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO, mem, sz);
            for(int32_t row = 0; row < mask->header.h; row++) {
                const uint16_t * row565 = (const uint16_t *)(rgb + (uint32_t)row * (uint32_t)rgb_stride);
                uint8_t * dst = tmp.data + (uint32_t)row * (uint32_t)tmp.header.stride;
                const uint8_t * alpha = a8 + (uint32_t)row * (uint32_t)mask->header.w;
                for(int32_t col = 0; col < mask->header.w; col++) {
                    const uint16_t c = row565[col];
                    dst[col * 4 + 0] = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
                    dst[col * 4 + 1] = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
                    dst[col * 4 + 2] = (uint8_t)((c & 0x1F) * 255 / 31);
                    dst[col * 4 + 3] = alpha[col];
                }
            }
            lv_opengles_teximage_bgra8888(0, mask->header.w, mask->header.h, tmp.data, tmp.header.stride);
            lv_free(mem);
        }
        else {
            lv_image_decoder_dsc_t dec;
            lv_memzero(&dec, sizeof(dec));
            if(lv_image_decoder_open(&dec, mask_src, NULL) != LV_RESULT_OK || !dec.decoded) {
                GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
                GL_CALL(glDeleteTextures(1, &tex));
                lv_image_decoder_close(&dec);
                return false;
            }
            lv_opengles_teximage_bgra8888(0, dec.decoded->header.w, dec.decoded->header.h,
                                          dec.decoded->data, dec.decoded->header.stride);
            lv_image_decoder_close(&dec);
        }
        GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
        *tex_out = tex;
        return true;
    }

    lv_image_decoder_dsc_t dec;
    lv_memzero(&dec, sizeof(dec));
    if(lv_image_decoder_open(&dec, mask_src, NULL) != LV_RESULT_OK || !dec.decoded) {
        lv_image_decoder_close(&dec);
        return false;
    }
    const bool ok = upload_mask_from_decoded(dec.decoded, tex_out);
    lv_image_decoder_close(&dec);
    return ok;
}

static void draw_uv_quad(int32_t dw, int32_t dh, unsigned int prog, int loc_disp, int loc_sampler,
                          const float * pos, const float * uv, unsigned int tex)
{
    GL_CALL(glUseProgram(prog));
    GL_CALL(glUniform2f(loc_disp, (float)dw, (float)dh));
    GL_CALL(glActiveTexture(GL_TEXTURE0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glUniform1i(loc_sampler, 0));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glEnableVertexAttribArray(1));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos));
    GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(1));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

static void draw_glyph_quad_ex(const lv_area_t * letter_coords, int32_t dw, int32_t dh,
                                const lv_gpu_glyph_atlas_uv_t * uv, lv_color_t color, lv_opa_t opa,
                                int32_t rotation_01deg, const lv_point_t * pivot, int32_t glyph_box_h, int32_t glyph_ofs_y)
{
    if(!prog_glyph || !uv || uv->tex == 0) return;
    const float pad = uv->sdf ? 4.0f : 0.0f;
    const float bw = (float)lv_area_get_width(letter_coords) + 1.0f + pad * 2.0f;
    const float bh = (float)lv_area_get_height(letter_coords) + 1.0f + pad * 2.0f;
    const float x0 = (float)letter_coords->x1 - pad;
    const float y0 = (float)letter_coords->y1 - pad;

    float pos[12];
    float uvs[] = { uv->u0, uv->v0, uv->u1, uv->v0, uv->u1, uv->v1,
                    uv->u0, uv->v0, uv->u1, uv->v1, uv->u0, uv->v1 };

    if(rotation_01deg % 3600 == 0) {
        const float x1 = x0 + bw - 1.0f;
        const float y1 = y0 + bh - 1.0f;
        float flat[] = { x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1 };
        lv_memcpy(pos, flat, sizeof(pos));
    }
    else {
        lv_draw_image_dsc_t id;
        lv_draw_image_dsc_init(&id);
        id.rotation = rotation_01deg;
        id.pivot.x = pivot ? pivot->x : 0;
        id.pivot.y = pivot ? (glyph_box_h + glyph_ofs_y) : 0;
        lv_matrix_t matrix;
        image_dsc_to_matrix(&matrix, (int32_t)x0, (int32_t)y0, &id);
        float corners[4][2] = { {0.f, 0.f}, {bw, 0.f}, {bw, bh}, {0.f, bh} };
        for(int i = 0; i < 4; i++) {
            float ox, oy;
            matrix_transform_point(&matrix, corners[i][0], corners[i][1], &ox, &oy);
            corners[i][0] = ox;
            corners[i][1] = oy;
        }
        float tris[] = {
            corners[0][0], corners[0][1], corners[1][0], corners[1][1], corners[2][0], corners[2][1],
            corners[0][0], corners[0][1], corners[2][0], corners[2][1], corners[3][0], corners[3][1],
        };
        lv_memcpy(pos, tris, sizeof(pos));
    }

    lv_color32_t c32 = lv_color_to_32(color, opa);
    GL_CALL(glUseProgram(prog_glyph));
    GL_CALL(glUniform2f(loc_glyph_disp, (float)dw, (float)dh));
    gpu_comp_uniform_rgba(loc_glyph_color, c32);
    GL_CALL(glUniform1f(loc_glyph_opa, (float)opa / (float)LV_OPA_COVER));
    GL_CALL(glUniform1f(loc_glyph_sdf, uv->sdf ? 1.0f : 0.0f));
    GL_CALL(glUniform1f(loc_glyph_smooth, 0.06f));
    GL_CALL(glActiveTexture(GL_TEXTURE0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, uv->tex));
    GL_CALL(glUniform1i(loc_glyph_sampler, 0));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glEnableVertexAttribArray(1));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos));
    GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uvs));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(1));
    GL_CALL(glDisableVertexAttribArray(0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

static void draw_glyph_quad(const lv_area_t * letter_coords, int32_t dw, int32_t dh,
                             const lv_gpu_glyph_atlas_uv_t * uv, lv_color_t color, lv_opa_t opa)
{
    draw_glyph_quad_ex(letter_coords, dw, dh, uv, color, opa, 0, NULL, 0, 0);
}

static bool draw_glyph_bitmap_once(const uint8_t * bitmap, int32_t bw, int32_t bh, int32_t stride,
                                    const lv_area_t * letter_coords, int32_t dw, int32_t dh,
                                    lv_color_t color, lv_opa_t opa, int32_t rotation_01deg,
                                    const lv_point_t * pivot, int32_t glyph_box_h, int32_t glyph_ofs_y)
{
    if(!bitmap || bw < 1 || bh < 1) return false;
    unsigned int tex = 0;
    GL_CALL(glGenTextures(1, &tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, bw, bh, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL));
    for(int32_t row = 0; row < bh; row++) {
        GL_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, bw, 1, GL_ALPHA, GL_UNSIGNED_BYTE,
                                bitmap + (uint32_t)row * (uint32_t)stride));
    }
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    lv_gpu_glyph_atlas_uv_t uv;
    uv.tex = tex;
    uv.u0 = 0.f;
    uv.v0 = 0.f;
    uv.u1 = 1.f;
    uv.v1 = 1.f;
    uv.sdf = 0;
    draw_glyph_quad_ex(letter_coords, dw, dh, &uv, color, opa, rotation_01deg, pivot, glyph_box_h, glyph_ofs_y);
    GL_CALL(glDeleteTextures(1, &tex));
    return true;
}

uint32_t lv_gpu_renderer_gles2_2d_glyph_overflow_count(void)
{
    return g_glyph_overflow_count;
}

#if LV_USE_FREETYPE && LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
static void gpu_freetype_outline_event_cb(lv_event_t * e)
{
    lv_freetype_outline_event_param_t * param = lv_event_get_param(e);
    switch(lv_event_get_code(e)) {
        case LV_EVENT_CREATE:
            param->outline = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);
            break;
        case LV_EVENT_DELETE:
            lv_vector_path_clear(param->outline);
            lv_vector_path_delete(param->outline);
            break;
        case LV_EVENT_INSERT: {
                lv_fpoint_t pnt;
                lv_fpoint_t ctrl_pnt1;
                lv_fpoint_t ctrl_pnt2;
                lv_vector_path_t * path = param->outline;
                switch(param->type) {
                    case LV_FREETYPE_OUTLINE_MOVE_TO:
                        pnt.x = LV_FREETYPE_F26DOT6_TO_FLOAT(param->to.x);
                        pnt.y = LV_FREETYPE_F26DOT6_TO_FLOAT(param->to.y);
                        lv_vector_path_move_to(path, &pnt);
                        break;
                    case LV_FREETYPE_OUTLINE_LINE_TO:
                        pnt.x = LV_FREETYPE_F26DOT6_TO_FLOAT(param->to.x);
                        pnt.y = LV_FREETYPE_F26DOT6_TO_FLOAT(param->to.y);
                        lv_vector_path_line_to(path, &pnt);
                        break;
                    case LV_FREETYPE_OUTLINE_CUBIC_TO:
                        pnt.x = LV_FREETYPE_F26DOT6_TO_FLOAT(param->to.x);
                        pnt.y = LV_FREETYPE_F26DOT6_TO_FLOAT(param->to.y);
                        ctrl_pnt1.x = LV_FREETYPE_F26DOT6_TO_FLOAT(param->control1.x);
                        ctrl_pnt1.y = LV_FREETYPE_F26DOT6_TO_FLOAT(param->control1.y);
                        ctrl_pnt2.x = LV_FREETYPE_F26DOT6_TO_FLOAT(param->control2.x);
                        ctrl_pnt2.y = LV_FREETYPE_F26DOT6_TO_FLOAT(param->control2.y);
                        lv_vector_path_cubic_to(path, &ctrl_pnt1, &ctrl_pnt2, &pnt);
                        break;
                    case LV_FREETYPE_OUTLINE_CONIC_TO:
                        pnt.x = LV_FREETYPE_F26DOT6_TO_FLOAT(param->to.x);
                        pnt.y = LV_FREETYPE_F26DOT6_TO_FLOAT(param->to.y);
                        ctrl_pnt1.x = LV_FREETYPE_F26DOT6_TO_FLOAT(param->control1.x);
                        ctrl_pnt1.y = LV_FREETYPE_F26DOT6_TO_FLOAT(param->control1.y);
                        lv_vector_path_quad_to(path, &ctrl_pnt1, &pnt);
                        break;
                    default:
                        break;
                }
                break;
            }
        default:
            break;
    }
}

static bool draw_glyph_vector_gpu(lv_draw_glyph_dsc_t * glyph_draw_dsc, int32_t dw, int32_t dh)
{
    if(!glyph_draw_dsc || !glyph_draw_dsc->letter_coords || !glyph_draw_dsc->g) return false;
    lv_vector_path_t * paths = (lv_vector_path_t *)glyph_draw_dsc->glyph_data;
    if(!paths) return false;

    float scale = 1.0f;
    if(lv_freetype_is_outline_font(glyph_draw_dsc->g->resolved_font)) {
        scale = LV_FREETYPE_F26DOT6_TO_FLOAT(lv_freetype_outline_get_scale(glyph_draw_dsc->g->resolved_font));
    }
    const int32_t w = (int32_t)((float)glyph_draw_dsc->g->box_w + glyph_draw_dsc->outline_stroke_width * 2 * scale);
    const int32_t h = (int32_t)((float)glyph_draw_dsc->g->box_h + glyph_draw_dsc->outline_stroke_width * 2 * scale);
    if(w < 1 || h < 1) return false;

    lv_draw_buf_t * draw_buf = lv_draw_buf_create(w, h, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    if(!draw_buf) return false;
    lv_draw_buf_clear(draw_buf, NULL);

    lv_layer_t layer;
    lv_memzero(&layer, sizeof(layer));
    layer.draw_buf = draw_buf;
    layer.color_format = LV_COLOR_FORMAT_ARGB8888;
    layer.buf_area.x1 = 0;
    layer.buf_area.y1 = 0;
    layer.buf_area.x2 = w - 1;
    layer.buf_area.y2 = h - 1;
    layer._clip_area = layer.buf_area;
    layer.phy_clip_area = layer.buf_area;

    lv_matrix_t matrix;
    lv_matrix_identity(&matrix);
    const int32_t offset_x = (int32_t)((float)glyph_draw_dsc->g->ofs_x - glyph_draw_dsc->outline_stroke_width * scale);
    const int32_t offset_y = (int32_t)((float)glyph_draw_dsc->g->ofs_y - glyph_draw_dsc->outline_stroke_width * scale);
    lv_matrix_scale(&matrix, 1, -1);
    lv_matrix_translate(&matrix, -offset_x, -h - offset_y);
    lv_matrix_scale(&matrix, scale, scale);

    lv_draw_vector_dsc_t * vector_dsc = lv_draw_vector_dsc_create(&layer);
    lv_draw_vector_dsc_set_transform(vector_dsc, &matrix);
    if(glyph_draw_dsc->outline_stroke_width > 0) {
        lv_draw_vector_dsc_set_stroke_color(vector_dsc, glyph_draw_dsc->outline_stroke_color);
        lv_draw_vector_dsc_set_stroke_opa(vector_dsc, glyph_draw_dsc->outline_stroke_opa);
        lv_draw_vector_dsc_set_stroke_width(vector_dsc, glyph_draw_dsc->outline_stroke_width);
        lv_draw_vector_dsc_add_path(vector_dsc, paths);
        lv_draw_vector_dsc_set_stroke_opa(vector_dsc, 0);
        lv_draw_vector_dsc_set_stroke_width(vector_dsc, 0);
    }
    lv_draw_vector_dsc_set_fill_color(vector_dsc, glyph_draw_dsc->color);
    lv_draw_vector_dsc_set_fill_opa(vector_dsc, glyph_draw_dsc->opa);
    lv_draw_vector_dsc_add_path(vector_dsc, paths);

    if(vector_dsc->task_list) {
        lv_draw_task_t dummy_t;
        lv_memzero(&dummy_t, sizeof(dummy_t));
        dummy_t.area = layer._clip_area;
        dummy_t._real_area = layer._clip_area;
        dummy_t.clip_area = layer._clip_area;
        dummy_t.target_layer = &layer;
        dummy_t.type = LV_DRAW_TASK_TYPE_VECTOR;
        dummy_t.opa = LV_OPA_COVER;
        dummy_t.draw_dsc = vector_dsc;
        lv_draw_sw_vector(&dummy_t, dummy_t.draw_dsc);
    }
    lv_draw_vector_dsc_delete(vector_dsc);

    unsigned int tex = 0;
    const bool uploaded = upload_decoded_image(draw_buf, &tex);
    lv_draw_buf_destroy(draw_buf);
    if(!uploaded) return false;

    lv_area_t letter_coords = *glyph_draw_dsc->letter_coords;
    lv_area_set_width(&letter_coords, w);
    lv_area_set_height(&letter_coords, h);
    draw_textured_quad(&letter_coords, dw, dh, tex);
    GL_CALL(glDeleteTextures(1, &tex));
    return true;
}
#endif

static bool draw_glyph_bitmap_gpu(const lv_font_t * font, const lv_font_glyph_dsc_t * g,
                                   const lv_draw_buf_t * draw_buf, lv_draw_glyph_dsc_t * glyph_draw_dsc,
                                   int32_t dw, int32_t dh, lv_color_t color, lv_opa_t opa)
{
    const int32_t bw = draw_buf->header.w > 0 ? draw_buf->header.w : lv_area_get_width(glyph_draw_dsc->letter_coords);
    const int32_t bh = draw_buf->header.h > 0 ? draw_buf->header.h : lv_area_get_height(glyph_draw_dsc->letter_coords);
    const int32_t stride = draw_buf->header.stride;

    lv_gpu_glyph_atlas_uv_t uv;
    if(lv_gpu_glyph_atlas_acquire(font, g->gid.index, draw_buf->data, bw, bh, stride, &uv)) {
        draw_glyph_quad_ex(glyph_draw_dsc->letter_coords, dw, dh, &uv, color, opa,
                           glyph_draw_dsc->rotation, &glyph_draw_dsc->pivot, g->box_h, g->ofs_y);
        return true;
    }

    g_glyph_overflow_count++;
    return draw_glyph_bitmap_once(draw_buf->data, bw, bh, stride, glyph_draw_dsc->letter_coords, dw, dh,
                                  color, opa, glyph_draw_dsc->rotation, &glyph_draw_dsc->pivot, g->box_h, g->ofs_y);
}

static void gpu_glyph_draw_cb(lv_draw_task_t * t, lv_draw_glyph_dsc_t * glyph_draw_dsc,
                               lv_draw_fill_dsc_t * fill_draw_dsc, const lv_area_t * fill_area)
{
    LV_UNUSED(t);
    const int32_t dw = g_gpu2d_dw;
    const int32_t dh = g_gpu2d_dh;
    if(dw < 1 || dh < 1) return;

    if(fill_draw_dsc && fill_area) {
        draw_fill_quad(fill_area, dw, dh, fill_draw_dsc->color, fill_draw_dsc->opa, 0);
        return;
    }

    if(!glyph_draw_dsc || !glyph_draw_dsc->letter_coords || !glyph_draw_dsc->g) return;

    const lv_font_glyph_dsc_t * g = glyph_draw_dsc->g;
    const lv_font_t * font = g->resolved_font;
    if(!font) return;

#if LV_USE_FREETYPE && LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
    if(glyph_draw_dsc->format == LV_FONT_GLYPH_FORMAT_VECTOR) {
        if(!glyph_draw_dsc->glyph_data) {
            glyph_draw_dsc->glyph_data = lv_font_get_glyph_bitmap(g, glyph_draw_dsc->_draw_buf);
        }
        if(draw_glyph_vector_gpu(glyph_draw_dsc, dw, dh)) return;
    }
#endif

    if(glyph_draw_dsc->format == LV_FONT_GLYPH_FORMAT_IMAGE) {
        if(!glyph_draw_dsc->glyph_data) {
            glyph_draw_dsc->glyph_data = lv_font_get_glyph_bitmap(g, glyph_draw_dsc->_draw_buf);
        }
        if(glyph_draw_dsc->glyph_data) {
            lv_draw_image_dsc_t id;
            lv_draw_image_dsc_init(&id);
            id.src = glyph_draw_dsc->glyph_data;
            id.opa = glyph_draw_dsc->opa;
            id.rotation = glyph_draw_dsc->rotation;
            id.pivot = (lv_point_t) {
                .x = glyph_draw_dsc->pivot.x,
                .y = g->box_h + g->ofs_y,
            };
            id.recolor = glyph_draw_dsc->color;
            id.recolor_opa = LV_OPA_COVER;
            draw_image_gpu(glyph_draw_dsc->letter_coords, &g_gpu_text_task.clip_area, &id, dw, dh);
        }
        return;
    }

    if(!glyph_draw_dsc->glyph_data) {
        glyph_draw_dsc->glyph_data = lv_font_get_glyph_bitmap(g, glyph_draw_dsc->_draw_buf);
    }
    const lv_draw_buf_t * draw_buf = glyph_draw_dsc->glyph_data;
    if(!draw_buf) return;

    if(glyph_draw_dsc->outline_stroke_width > 0) {
        static const int8_t dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
        static const int8_t dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
        const int32_t step = LV_MAX(1, glyph_draw_dsc->outline_stroke_width / 2);
        lv_draw_glyph_dsc_t stroke = *glyph_draw_dsc;
        lv_area_t shifted;
        stroke.letter_coords = &shifted;
        for(int i = 0; i < 8; i++) {
            shifted = *glyph_draw_dsc->letter_coords;
            lv_area_move(&shifted, dx[i] * step, dy[i] * step);
            draw_glyph_bitmap_gpu(font, g, draw_buf, &stroke, dw, dh,
                                  glyph_draw_dsc->outline_stroke_color, glyph_draw_dsc->outline_stroke_opa);
        }
    }

    draw_glyph_bitmap_gpu(font, g, draw_buf, glyph_draw_dsc, dw, dh, glyph_draw_dsc->color, glyph_draw_dsc->opa);
}

static bool draw_label_gpu(const lv_area_t * area, const lv_area_t * clip,
                            const lv_draw_label_dsc_t * ld, int32_t dw, int32_t dh)
{
    if(!prog_glyph || !ld) return false;

#if LV_USE_FREETYPE && LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
    static bool ft_evt;
    if(!ft_evt) {
        lv_freetype_outline_add_event(gpu_freetype_outline_event_cb, LV_EVENT_ALL, NULL);
        ft_evt = true;
    }
#endif

    g_gpu2d_dw = dw;
    g_gpu2d_dh = dh;
    lv_memcpy(&g_gpu_label_dsc, ld, sizeof(g_gpu_label_dsc));
    lv_memzero(&g_gpu_text_task, sizeof(g_gpu_text_task));
    g_gpu_text_task.clip_area = *clip;
    g_gpu_text_task.target_layer = lv_refr_get_disp_refreshing()->layer_head;
    g_gpu_text_task.draw_dsc = &g_gpu_label_dsc;

    lv_draw_label_iterate_characters(&g_gpu_text_task, ld, area, gpu_glyph_draw_cb);
    return true;
}

static bool draw_letter_gpu(const lv_area_t * area, const lv_area_t * clip,
                             const lv_draw_letter_dsc_t * ld, int32_t dw, int32_t dh)
{
    if(!prog_glyph || !ld) return false;

    g_gpu2d_dw = dw;
    g_gpu2d_dh = dh;
    lv_memzero(&g_gpu_text_task, sizeof(g_gpu_text_task));
    g_gpu_text_task.clip_area = *clip;
    g_gpu_text_task.target_layer = lv_refr_get_disp_refreshing()->layer_head;

    lv_draw_glyph_dsc_t glyph_dsc;
    lv_draw_glyph_dsc_init(&glyph_dsc);
    glyph_dsc.opa = ld->opa;
    glyph_dsc.color = ld->color;
    glyph_dsc.rotation = ld->rotation;
    glyph_dsc.pivot = ld->pivot;
    glyph_dsc.outline_stroke_width = ld->outline_stroke_width;
    glyph_dsc.outline_stroke_opa = ld->outline_stroke_opa;
    glyph_dsc.outline_stroke_color = ld->outline_stroke_color;

    lv_point_t pt = { area->x1, area->y1 };
    lv_draw_unit_draw_letter(&g_gpu_text_task, &glyph_dsc, &pt, ld->font, ld->unicode, gpu_glyph_draw_cb);

    if(glyph_dsc._draw_buf) {
        lv_draw_buf_destroy(glyph_dsc._draw_buf);
    }
    return true;
}

static void image_dsc_to_matrix(lv_matrix_t * matrix, int32_t x, int32_t y, const lv_draw_image_dsc_t * dsc)
{
    lv_matrix_identity(matrix);
    lv_matrix_translate(matrix, (float)x, (float)y);
    if(dsc->rotation != 0 || dsc->scale_x != LV_SCALE_NONE || dsc->scale_y != LV_SCALE_NONE
       || dsc->skew_x != 0 || dsc->skew_y != 0) {
        lv_matrix_translate(matrix, (float)dsc->pivot.x, (float)dsc->pivot.y);
        if(dsc->skew_x != 0 || dsc->skew_y != 0) {
            lv_matrix_skew(matrix, (float)dsc->skew_x, (float)dsc->skew_y);
        }
        if(dsc->rotation != 0) {
            lv_matrix_rotate(matrix, dsc->rotation * 0.1f);
        }
        if(dsc->scale_x != LV_SCALE_NONE || dsc->scale_y != LV_SCALE_NONE) {
            lv_matrix_scale(matrix, (float)dsc->scale_x / (float)LV_SCALE_NONE,
                            (float)dsc->scale_y / (float)LV_SCALE_NONE);
        }
        lv_matrix_translate(matrix, -(float)dsc->pivot.x, -(float)dsc->pivot.y);
    }
}

static void matrix_transform_point(const lv_matrix_t * m, float x, float y, float * ox, float * oy)
{
    *ox = m->m[0][0] * x + m->m[0][1] * y + m->m[0][2];
    *oy = m->m[1][0] * x + m->m[1][1] * y + m->m[1][2];
}

static bool upload_decoded_image(const lv_draw_buf_t * decoded, unsigned int * tex_out)
{
    if(!decoded || !decoded->data || !tex_out) return false;
    unsigned int tex = 0;
    GL_CALL(glGenTextures(1, &tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    lv_opengles_teximage_bgra8888(0, decoded->header.w, decoded->header.h, decoded->data, decoded->header.stride);
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    *tex_out = tex;
    return true;
}

static bool draw_tex_transformed(unsigned int tex, int32_t img_w, int32_t img_h,
                                  int32_t x, int32_t y, const lv_draw_image_dsc_t * dsc,
                                  const lv_area_t * coords, int32_t dw, int32_t dh)
{
    if(!prog_img || !dsc || !coords || tex == 0 || img_w < 1 || img_h < 1 || dw < 1 || dh < 1) return false;

    lv_matrix_t matrix;
    image_dsc_to_matrix(&matrix, x, y, dsc);

    float x0, y0, x1, y1, x2, y2, x3, y3;
    matrix_transform_point(&matrix, 0.f, 0.f, &x0, &y0);
    matrix_transform_point(&matrix, (float)img_w, 0.f, &x1, &y1);
    matrix_transform_point(&matrix, (float)img_w, (float)img_h, &x2, &y2);
    matrix_transform_point(&matrix, 0.f, (float)img_h, &x3, &y3);
    float pos[] = { x0, y0, x1, y1, x2, y2, x0, y0, x2, y2, x3, y3 };
    float uv[] = {
        0.f, 0.f, 1.f, 0.f, 1.f, 1.f,
        0.f, 0.f, 1.f, 1.f, 0.f, 1.f,
    };

    apply_blend_mode(dsc->blend_mode);

    unsigned int mask_tex = 0;
    bool use_mask = false;
    float mask_uv[4];
    if(dsc->bitmap_mask_src) {
        if(upload_mask_texture(dsc->bitmap_mask_src, &mask_tex)) {
            use_mask = true;
            compute_mask_uv(dsc, coords, img_w, img_h, mask_uv);
        }
        else {
            restore_blend_mode();
            return false;
        }
    }

    const bool use_diff = dsc->blend_mode == LV_BLEND_MODE_DIFFERENCE;
    if(use_diff && !snap_dst_for_quad(dw, dh, pos)) {
        restore_blend_mode();
        return false;
    }

    const bool use_recolor = dsc->recolor_opa > LV_OPA_MIN;
    GL_CALL(glUseProgram(prog_img));
    GL_CALL(glUniform2f(loc_img_disp, (float)dw, (float)dh));
    GL_CALL(glUniform1f(loc_img_opa, (float)dsc->opa / (float)LV_OPA_COVER));
    if(use_recolor) {
        lv_color32_t rc = lv_color_to_32(dsc->recolor, dsc->recolor_opa);
        gpu_comp_uniform_rgba(loc_img_recolor, rc);
        GL_CALL(glUniform1i(loc_img_use_recolor, 1));
    }
    else {
        GL_CALL(glUniform1i(loc_img_use_recolor, 0));
    }
    GL_CALL(glUniform1i(loc_img_use_mask, use_mask ? 1 : 0));
    if(use_mask) {
        GL_CALL(glUniform4f(loc_img_mask_uv, mask_uv[0], mask_uv[1], mask_uv[2], mask_uv[3]));
    }
    GL_CALL(glUniform1i(loc_img_use_diff, use_diff ? 1 : 0));
    if(use_diff) {
        GL_CALL(glUniform4f(loc_img_dst_rect, (float)g_img_dst_snap_x, (float)g_img_dst_snap_y,
                            (float)g_img_dst_snap_w, (float)g_img_dst_snap_h));
    }

    GL_CALL(glActiveTexture(GL_TEXTURE0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CALL(glUniform1i(loc_img_sampler, 0));
    if(use_mask) {
        GL_CALL(glActiveTexture(GL_TEXTURE1));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, mask_tex));
        GL_CALL(glUniform1i(loc_img_mask_sampler, 1));
    }
    if(use_diff) {
        GL_CALL(glActiveTexture(GL_TEXTURE2));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, g_img_dst_snap_tex));
        GL_CALL(glUniform1i(loc_img_dst_sampler, 2));
    }

    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glEnableVertexAttribArray(1));
    GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos));
    GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
    GL_CALL(glDisableVertexAttribArray(1));
    GL_CALL(glDisableVertexAttribArray(0));

    GL_CALL(glActiveTexture(GL_TEXTURE0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    if(mask_tex) GL_CALL(glDeleteTextures(1, &mask_tex));

    restore_blend_mode();
    return true;
}

bool lv_gpu_renderer_gles2_2d_composite_layer(unsigned int dst_tex, unsigned int src_tex,
                                               int32_t src_w, int32_t src_h,
                                               const lv_draw_image_dsc_t * dsc, const lv_area_t * coords,
                                               int32_t dw, int32_t dh)
{
    if(!dsc || !coords || dst_tex == 0 || src_tex == 0 || src_w < 1 || src_h < 1 || dw < 1 || dh < 1) {
        return false;
    }
    if(!lv_gpu_renderer_tex_fbo_bind_complete(dst_tex)) return false;

    g_gpu2d_dw = dw;
    g_gpu2d_dh = dh;
    lv_opengles_reinit_state();
    const bool ok = draw_tex_transformed(src_tex, src_w, src_h, coords->x1, coords->y1, dsc, coords, dw, dh);
    lv_gpu_renderer_restore_default_framebuffer();
    return ok;
}

static bool draw_image_gpu(const lv_area_t * coords, const lv_area_t * clip,
                            const lv_draw_image_dsc_t * dsc, int32_t dw, int32_t dh)
{
    LV_UNUSED(clip);
    if(!prog_img || !dsc || !coords) return false;

    lv_image_decoder_dsc_t decoder_dsc;
    lv_memzero(&decoder_dsc, sizeof(decoder_dsc));
    lv_result_t res = lv_image_decoder_open(&decoder_dsc, dsc->src, NULL);
    if(res != LV_RESULT_OK || !decoder_dsc.decoded) {
        lv_image_decoder_close(&decoder_dsc);
        return false;
    }

    unsigned int tex = 0;
    if(!upload_decoded_image(decoder_dsc.decoded, &tex)) {
        lv_image_decoder_close(&decoder_dsc);
        return false;
    }

    const int32_t img_w = decoder_dsc.decoded->header.w;
    const int32_t img_h = decoder_dsc.decoded->header.h;
    bool ok = false;

    if(!dsc->tile) {
        ok = draw_tex_transformed(tex, img_w, img_h, coords->x1, coords->y1, dsc, coords, dw, dh);
    }
    else {
        lv_area_t tile_area;
        if(lv_area_get_width(&dsc->image_area) >= 0) tile_area = dsc->image_area;
        else tile_area = *coords;
        lv_area_set_width(&tile_area, img_w);
        lv_area_set_height(&tile_area, img_h);

        const int32_t tile_x_start = tile_area.x1;
        ok = true;
        while(tile_area.y1 <= coords->y2 && ok) {
            while(tile_area.x1 <= coords->x2 && ok) {
                lv_area_t clipped;
                if(lv_area_intersect(&clipped, &tile_area, coords)) {
                    if(!draw_tex_transformed(tex, img_w, img_h, tile_area.x1, tile_area.y1, dsc, &tile_area, dw, dh)) {
                        ok = false;
                    }
                }
                tile_area.x1 += img_w;
                tile_area.x2 += img_w;
            }
            tile_area.y1 += img_h;
            tile_area.y2 += img_h;
            tile_area.x1 = tile_x_start;
            tile_area.x2 = tile_x_start + img_w - 1;
        }
    }

    GL_CALL(glDeleteTextures(1, &tex));
    lv_image_decoder_close(&decoder_dsc);
    return ok;
}

#if LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
static void draw_vector_sw_cb(lv_layer_t * layer, void * dsc, const lv_area_t * rel)
{
    LV_UNUSED(rel);
    lv_draw_task_t dummy;
    lv_memzero(&dummy, sizeof(dummy));
    dummy.target_layer = layer;
    dummy.clip_area = layer->_clip_area;
    dummy.draw_dsc = dsc;
    lv_draw_sw_vector(&dummy, dsc);
}
#endif

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

    g_gpu2d_dw = dw;
    g_gpu2d_dh = dh;

    uint32_t rendered = 0;
    uint32_t sw_raster = 0;

    for(uint32_t i = 0; i < count; i++) {
        const lv_gpu_renderer_gles2_cmd_t * cmd = &cmds[i];
        lv_gpu2d_cmd_t * mut_cmd = (lv_gpu2d_cmd_t *)(uintptr_t)cmd;
        lv_area_t draw_area;
        if(!lv_area_intersect(&draw_area, &cmd->area, &cmd->clip)) continue;
        apply_scissor(&cmd->clip, dh);

        switch(cmd->type) {
            case LV_GPU_RENDERER_GLES2_CMD_FILL: {
                    if(draw_fill_gpu(&draw_area, dw, dh, &cmd->u.fill)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_fill_sw_cb,
                                                 &cmd->u.fill, &tex)) {
                            draw_textured_quad(&draw_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_BORDER:
                draw_border_gpu(mut_cmd, dw, dh);
                rendered++;
                break;
            case LV_GPU_RENDERER_GLES2_CMD_LINE: {
                    if(draw_line_gpu(&draw_area, dw, dh, cmd)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_line_sw_cb,
                                                 &cmd->u.line, &tex)) {
                            draw_textured_quad(&draw_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_ARC: {
                    if(draw_arc_gpu(&draw_area, dw, dh, &cmd->u.arc)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_arc_sw_cb,
                                                 &cmd->u.arc, &tex)) {
                            draw_textured_quad(&draw_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_BOX_SHADOW: {
                    if(draw_box_shadow_gpu(&cmd->area, &cmd->clip, dw, dh, &cmd->u.box_shadow)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_shadow_sw(&cmd->area, &cmd->clip, &cmd->u.box_shadow, &tex)) {
                            lv_area_t shadow_area;
                            shadow_compute_area(&cmd->area, &cmd->u.box_shadow, &shadow_area);
                            lv_area_t draw_area2;
                            if(lv_area_intersect(&draw_area2, &shadow_area, &cmd->clip)) {
                                draw_textured_quad(&draw_area2, dw, dh, tex);
                            }
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_TRIANGLE: {
                    if(draw_triangle_gpu(&draw_area, dw, dh, &cmd->u.triangle)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_triangle_sw_cb,
                                                 &cmd->u.triangle, &tex)) {
                            draw_textured_quad(&draw_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_MASK_RECT: {
                    if(draw_mask_gpu(&draw_area, dw, dh, &cmd->u.mask_rect)) {
                        rendered++;
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_BLUR: {
                    if(draw_blur_gpu(color_tex, dw, dh, &cmd->u.blur.coords, &cmd->clip, &cmd->u.blur.blur)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->u.blur.coords, &cmd->clip, draw_blur_sw_cb,
                                                 &cmd->u.blur.blur, &tex)) {
                            draw_textured_quad(&draw_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_LABEL: {
                    if(draw_label_gpu(&cmd->area, &cmd->clip, &cmd->u.label, dw, dh)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_label_sw_cb,
                                                 &cmd->u.label, &tex)) {
                            draw_textured_quad(&draw_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_LETTER: {
                    if(draw_letter_gpu(&cmd->area, &cmd->clip, &cmd->u.letter, dw, dh)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_letter_sw_cb,
                                                 &cmd->u.letter, &tex)) {
                            draw_textured_quad(&draw_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
            case LV_GPU_RENDERER_GLES2_CMD_IMAGE: {
                    if(draw_image_gpu(&cmd->area, &cmd->clip, &cmd->u.image, dw, dh)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_image_sw_cb,
                                                 &cmd->u.image, &tex)) {
                            draw_textured_quad(&draw_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
                    }
                }
                break;
#if LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
            case LV_GPU_RENDERER_GLES2_CMD_VECTOR: {
                    unsigned int tex = 0;
                    if(raster_sw_to_texture(&cmd->area, &cmd->clip, draw_vector_sw_cb,
                                             &cmd->u.vector, &tex)) {
                        draw_textured_quad(&draw_area, dw, dh, tex);
                        GL_CALL(glDeleteTextures(1, &tex));
                        sw_raster++;
                        rendered++;
                    }
                }
                break;
#endif
            case LV_GPU_RENDERER_GLES2_CMD_MASK_BITMAP: {
                    lv_draw_image_dsc_t img_dsc;
                    lv_draw_image_dsc_init(&img_dsc);
                    img_dsc.src = cmd->u.mask_bitmap.mask_src;
                    img_dsc.opa = LV_OPA_COVER;
                    if(draw_image_gpu(&cmd->u.mask_bitmap.blend_area, &cmd->clip, &img_dsc, dw, dh)) {
                        rendered++;
                    }
                    else {
                        unsigned int tex = 0;
                        if(raster_sw_to_texture(&cmd->u.mask_bitmap.blend_area, &cmd->clip, draw_image_sw_cb,
                                                 &img_dsc, &tex)) {
                            draw_textured_quad(&cmd->u.mask_bitmap.blend_area, dw, dh, tex);
                            GL_CALL(glDeleteTextures(1, &tex));
                            sw_raster++;
                            rendered++;
                        }
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
