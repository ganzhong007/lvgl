/**
 * @file lv_gpu_renderer_layer.c — GPU FBO targets for offscreen layers
 */

#ifdef LV_CONF_PATH
#include LV_CONF_PATH
#else
#include "../../lv_conf_internal.h"
#endif

#if LV_USE_DRAW_GPU_RENDERER

#include "lv_gpu_renderer_layer.h"
#include "lv_draw_gpu_renderer.h"
#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include "../../misc/lv_area_private.h"
#include <string.h>

#define LV_GPU_LAYER_MAX 16
#define LV_GPU_LAYER_CMD_MAX 256

typedef struct {
    lv_layer_t * layer;
    unsigned int tex;
    unsigned int fbo;
    int32_t w;
    int32_t h;
    lv_gpu_renderer_gles2_cmd_t cmds[LV_GPU_LAYER_CMD_MAX];
    uint32_t cmd_count;
} lv_gpu_layer_slot_t;

static lv_gpu_layer_slot_t g_slots[LV_GPU_LAYER_MAX];

static lv_gpu_layer_slot_t * slot_find(const lv_layer_t * layer)
{
    for(uint32_t i = 0; i < LV_GPU_LAYER_MAX; i++) {
        if(g_slots[i].layer == layer) return &g_slots[i];
    }
    return NULL;
}

static lv_gpu_layer_slot_t * slot_alloc(lv_layer_t * layer)
{
    lv_gpu_layer_slot_t * existing = slot_find(layer);
    if(existing) return existing;

    for(uint32_t i = 0; i < LV_GPU_LAYER_MAX; i++) {
        if(g_slots[i].layer == NULL) {
            lv_memzero(&g_slots[i], sizeof(g_slots[i]));
            g_slots[i].layer = layer;
            return &g_slots[i];
        }
    }
    return NULL;
}

static bool slot_ensure_fbo(lv_gpu_layer_slot_t * slot, int32_t w, int32_t h)
{
    if(!slot || w < 1 || h < 1) return false;

    if(slot->tex && slot->w == w && slot->h == h) return true;

    if(slot->fbo) {
        GL_CALL(glDeleteFramebuffers(1, &slot->fbo));
        slot->fbo = 0;
    }
    if(slot->tex) {
        GL_CALL(glDeleteTextures(1, &slot->tex));
        slot->tex = 0;
    }

    GL_CALL(glGenTextures(1, &slot->tex));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, slot->tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

    GL_CALL(glGenFramebuffers(1, &slot->fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, slot->fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, slot->tex, 0));
#if !LV_USE_EGL
    {
        GLenum db = GL_COLOR_ATTACHMENT0;
        GL_CALL(glDrawBuffers(1, &db));
        GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0));
    }
#endif
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    if(!ok) return false;

    slot->w = w;
    slot->h = h;
    return true;
}

void lv_gpu_renderer_layer_init(void)
{
    lv_memzero(g_slots, sizeof(g_slots));
}

void lv_gpu_renderer_layer_deinit(void)
{
    for(uint32_t i = 0; i < LV_GPU_LAYER_MAX; i++) {
        if(g_slots[i].fbo) GL_CALL(glDeleteFramebuffers(1, &g_slots[i].fbo));
        if(g_slots[i].tex) GL_CALL(glDeleteTextures(1, &g_slots[i].tex));
    }
    lv_memzero(g_slots, sizeof(g_slots));
}

void lv_gpu_renderer_layer_on_created(lv_layer_t * layer)
{
    if(!layer || !layer->parent) return;
    if(layer->color_format != LV_COLOR_FORMAT_ARGB8888
       && layer->color_format != LV_COLOR_FORMAT_XRGB8888) return;

    lv_gpu_layer_slot_t * slot = slot_alloc(layer);
    if(!slot) return;

    const int32_t w = lv_area_get_width(&layer->buf_area);
    const int32_t h = lv_area_get_height(&layer->buf_area);
    if(slot_ensure_fbo(slot, w, h)) {
        layer->user_data = slot;
    }
}

void lv_gpu_renderer_layer_on_deleted(lv_layer_t * layer)
{
    lv_gpu_layer_slot_t * slot = slot_find(layer);
    if(!slot) return;
    if(slot->fbo) GL_CALL(glDeleteFramebuffers(1, &slot->fbo));
    if(slot->tex) GL_CALL(glDeleteTextures(1, &slot->tex));
    lv_memzero(slot, sizeof(*slot));
    if(layer) layer->user_data = NULL;
}

bool lv_gpu_renderer_layer_is_target(const lv_layer_t * layer)
{
    return layer && layer->user_data && slot_find(layer) != NULL;
}

unsigned int lv_gpu_renderer_layer_tex(const lv_layer_t * layer)
{
    lv_gpu_layer_slot_t * slot = slot_find(layer);
    return slot ? slot->tex : 0;
}

bool lv_gpu_renderer_layer_push_cmd(lv_layer_t * layer, const lv_gpu_renderer_gles2_cmd_t * cmd)
{
    lv_gpu_layer_slot_t * slot = slot_find(layer);
    if(!slot || !cmd || slot->cmd_count >= LV_GPU_LAYER_CMD_MAX) return false;
    slot->cmds[slot->cmd_count++] = *cmd;
    return true;
}

bool lv_gpu_renderer_layer_flush(lv_layer_t * layer)
{
    lv_gpu_layer_slot_t * slot = slot_find(layer);
    if(!slot || slot->cmd_count == 0 || slot->tex == 0) return false;

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, slot->fbo));
    GL_CALL(glViewport(0, 0, slot->w, slot->h));
    GL_CALL(glDisable(GL_SCISSOR_TEST));
    GL_CALL(glDisable(GL_BLEND));
    GL_CALL(glClearColor(0.f, 0.f, 0.f, 0.f));
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT));

    uint32_t sw_r = 0;
    lv_gpu_renderer_gles2_2d_render_cmd_list(slot->cmds, slot->cmd_count, slot->tex, slot->w, slot->h, &sw_r);
    slot->cmd_count = 0;

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    return true;
}

#endif /*LV_USE_DRAW_GPU_RENDERER*/
