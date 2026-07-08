/**
 * @file lv_gpu_renderer_glyph_atlas.c — dynamic glyph texture atlas for GPU text
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "lv_gpu_renderer_glyph_atlas.h"
#include "lv_gpu_renderer_gles2_sdf.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../stdlib/lv_mem.h"
#include <stdlib.h>
#include <string.h>

#define LV_GPU_GLYPH_ATLAS_SIZE_DEFAULT 1024
#define LV_GPU_GLYPH_CACHE_MAX  512
#define LV_GPU_GLYPH_PAD        4
#define LV_GPU_GLYPH_SDF_PAD    4

static int32_t glyph_atlas_size(void)
{
    static int32_t size = -1;
    if(size < 0) {
        const char * env = getenv("LVGL_GPU_GLYPH_ATLAS_SIZE");
        size = env && env[0] ? (int32_t)atoi(env) : LV_GPU_GLYPH_ATLAS_SIZE_DEFAULT;
        if(size < 64) size = 64;
        if(size > 2048) size = 2048;
    }
    return size;
}

typedef struct {
    const lv_font_t * font;
    uint32_t glyph_id;
    lv_gpu_glyph_atlas_uv_t uv;
} lv_gpu_glyph_cache_entry_t;

static unsigned int g_atlas_tex;
static int32_t g_shelf_x;
static int32_t g_shelf_y;
static int32_t g_shelf_h;
static lv_gpu_glyph_cache_entry_t g_cache[LV_GPU_GLYPH_CACHE_MAX];
static uint32_t g_cache_count;

static bool atlas_reset_shelf(void)
{
    g_shelf_x = LV_GPU_GLYPH_PAD;
    g_shelf_y = LV_GPU_GLYPH_PAD;
    g_shelf_h = 0;
    g_cache_count = 0;
    return true;
}

void lv_gpu_glyph_atlas_init(void)
{
    if(g_atlas_tex) return;

    GL_CALL(glGenTextures(1, &g_atlas_tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, g_atlas_tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    uint8_t clear_px = 128;
    for(int32_t y = 0; y < glyph_atlas_size(); y++) {
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, glyph_atlas_size(), 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, &clear_px));
    }
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    atlas_reset_shelf();
}

void lv_gpu_glyph_atlas_deinit(void)
{
    if(g_atlas_tex) {
        GL_CALL(glDeleteTextures(1, &g_atlas_tex));
        g_atlas_tex = 0;
    }
    g_cache_count = 0;
}

static const lv_gpu_glyph_cache_entry_t * cache_find(const lv_font_t * font, uint32_t glyph_id)
{
    for(uint32_t i = 0; i < g_cache_count; i++) {
        if(g_cache[i].font == font && g_cache[i].glyph_id == glyph_id) {
            return &g_cache[i];
        }
    }
    return NULL;
}

bool lv_gpu_glyph_atlas_acquire(const lv_font_t * font, uint32_t glyph_id,
                                 const uint8_t * bitmap, int32_t bw, int32_t bh, int32_t stride,
                                 lv_gpu_glyph_atlas_uv_t * out)
{
    if(!out || !font || !bitmap || bw < 1 || bh < 1 || stride < 1) return false;
    if(!g_atlas_tex) lv_gpu_glyph_atlas_init();

    const lv_gpu_glyph_cache_entry_t * hit = cache_find(font, glyph_id);
    if(hit) {
        *out = hit->uv;
        return true;
    }

    const int32_t pad = LV_GPU_GLYPH_SDF_PAD;
    const int32_t sdf_w = bw + pad * 2;
    const int32_t sdf_h = bh + pad * 2;
    const int32_t need_w = sdf_w + LV_GPU_GLYPH_PAD * 2;
    const int32_t need_h = sdf_h + LV_GPU_GLYPH_PAD * 2;

    if(need_w >= glyph_atlas_size() - LV_GPU_GLYPH_PAD) return false;

    if(g_shelf_x + need_w >= glyph_atlas_size()) {
        g_shelf_x = LV_GPU_GLYPH_PAD;
        g_shelf_y += g_shelf_h + LV_GPU_GLYPH_PAD;
        g_shelf_h = 0;
    }
    if(g_shelf_y + need_h >= glyph_atlas_size()) {
        return false;
    }

    const int32_t dst_x = g_shelf_x + LV_GPU_GLYPH_PAD;
    const int32_t dst_y = g_shelf_y + LV_GPU_GLYPH_PAD;
    if(dst_x + sdf_w > glyph_atlas_size() || dst_y + sdf_h > glyph_atlas_size()) return false;

    if(!lv_gpu_renderer_gles2_sdf_upload_atlas(g_atlas_tex, dst_x, dst_y, bitmap, bw, bh, stride, sdf_w, sdf_h)) {
        return false;
    }

    g_shelf_x += need_w;
    if(need_h > g_shelf_h) g_shelf_h = need_h;

    lv_gpu_glyph_atlas_uv_t uv;
    uv.tex = g_atlas_tex;
    const float inv = 1.0f / (float)glyph_atlas_size();
    uv.u0 = (float)dst_x * inv;
    uv.v0 = (float)dst_y * inv;
    uv.u1 = (float)(dst_x + sdf_w) * inv;
    uv.v1 = (float)(dst_y + sdf_h) * inv;
    uv.sdf = 1;
    *out = uv;

    if(g_cache_count < LV_GPU_GLYPH_CACHE_MAX) {
        lv_gpu_glyph_cache_entry_t * e = &g_cache[g_cache_count++];
        e->font = font;
        e->glyph_id = glyph_id;
        e->uv = uv;
    }
    return true;
}

#endif /*LV_USE_DRAW_GPU_RENDERER*/
