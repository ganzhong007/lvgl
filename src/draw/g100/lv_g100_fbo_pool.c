/**
 * @file lv_g100_fbo_pool.c
 */

#include "lv_g100_fbo_pool.h"

#if LV_USE_DRAW_G100

#include "lv_draw_g100_private.h"
#include "../../libs/nanovg/nanovg_gl_utils.h"

#define G100_FBO_POOL_SLOTS 2

typedef struct {
    NVGcontext * vg;
    NVGLUframebuffer * fb[G100_FBO_POOL_SLOTS];
    int slot_w[G100_FBO_POOL_SLOTS];
    int slot_h[G100_FBO_POOL_SLOTS];
    uint32_t acquire_count;
    uint32_t reuse_count;
    bool ok;
} lv_g100_fbo_pool_t;

static lv_g100_fbo_pool_t g_pool;

static bool ensure_slot(uint32_t slot, int w, int h)
{
    if(slot >= G100_FBO_POOL_SLOTS || !g_pool.vg) return false;

    if(g_pool.fb[slot] && g_pool.slot_w[slot] >= w && g_pool.slot_h[slot] >= h) {
        g_pool.reuse_count++;
        return true;
    }

    if(g_pool.fb[slot]) {
        nvgluDeleteFramebuffer(g_pool.fb[slot]);
        g_pool.fb[slot] = NULL;
        g_pool.slot_w[slot] = 0;
        g_pool.slot_h[slot] = 0;
    }

    g_pool.fb[slot] = nvgluCreateFramebuffer(g_pool.vg, w, h, 0, NVG_TEXTURE_RGBA);
    if(!g_pool.fb[slot]) return false;

    g_pool.slot_w[slot] = w;
    g_pool.slot_h[slot] = h;
    g_pool.acquire_count++;
    return true;
}

void lv_g100_fbo_pool_init(lv_draw_g100_unit_t * unit)
{
    lv_memzero(&g_pool, sizeof(g_pool));
    if(!unit || !unit->vg) return;

    g_pool.vg = unit->vg;

    /* Pre-warm ping-pong slots for typical layer/blur regions (BL-07). */
    const int warm_w = 512;
    const int warm_h = 512;
    g_pool.ok = ensure_slot(0, warm_w, warm_h) && ensure_slot(1, warm_w, warm_h);

    if(g_pool.ok) {
        LV_LOG_INFO("G100 blur FBO pool ready (fbo_ok=true, %dx%d x2)", warm_w, warm_h);
    }
    else {
        LV_LOG_WARN("G100 blur FBO pool init failed (fbo_ok=false)");
    }
}

void lv_g100_fbo_pool_deinit(lv_draw_g100_unit_t * unit)
{
    LV_UNUSED(unit);
    for(uint32_t i = 0; i < G100_FBO_POOL_SLOTS; i++) {
        if(g_pool.fb[i]) {
            nvgluDeleteFramebuffer(g_pool.fb[i]);
            g_pool.fb[i] = NULL;
        }
    }
    lv_memzero(&g_pool, sizeof(g_pool));
}

bool lv_g100_fbo_pool_is_ok(void)
{
    return g_pool.ok;
}

NVGLUframebuffer * lv_g100_fbo_pool_acquire(lv_draw_g100_unit_t * unit, int w, int h, uint32_t slot)
{
    LV_UNUSED(unit);
    if(w <= 0 || h <= 0 || slot >= G100_FBO_POOL_SLOTS) return NULL;
    if(!ensure_slot(slot, w, h)) return NULL;

    static bool reuse_logged;
    if(g_pool.reuse_count > 0 && !reuse_logged) {
        reuse_logged = true;
        LV_LOG_INFO("G100 blur FBO pool reuse active");
    }

    return g_pool.fb[slot];
}

#endif /*LV_USE_DRAW_G100*/
