/**
 * @file lv_evgpu_lib.c
 * @brief EVGPU GLES2 runtime facade (shader + context + evgpu_evgr); draw/evgpu stays task adapter.
 */

#include "lv_evgpu_lib.h"

#if LV_USE_DRAW_EVGPU && LV_USE_EVGPU_LIB

#include "../../draw/evgpu/lv_evgpu_context.h"
#include "../../draw/evgpu/lv_evgpu_shader.h"
#include "../../draw/evgpu/lv_draw_evgpu_private.h"

static lv_evgpu_lib_caps_t g_caps;

void lv_evgpu_lib_init(lv_draw_evgpu_unit_t * unit)
{
    lv_memzero(&g_caps, sizeof(g_caps));
    LV_ASSERT_NULL(unit);

    lv_evgpu_context_init(unit, &unit->ctx);
    lv_evgpu_shader_init(unit, &unit->ctx, &unit->shader);

    g_caps.context_ok = lv_evgpu_context_is_ready(&unit->ctx);
    g_caps.shader_ok = lv_evgpu_shader_is_ready(&unit->shader);

    if(g_caps.context_ok && g_caps.shader_ok) {
        LV_LOG_INFO("EVGPU lib ready (shader=%u, path=libs/evgpu)",
                    (unsigned)lv_evgpu_shader_get_solid_program(&unit->shader));
    }
    else {
        LV_LOG_WARN("EVGPU lib init incomplete (context=%d shader=%d)",
                    g_caps.context_ok, g_caps.shader_ok);
    }
}

void lv_evgpu_lib_deinit(lv_draw_evgpu_unit_t * unit)
{
    LV_ASSERT_NULL(unit);
    lv_evgpu_shader_deinit(&unit->shader);
    lv_evgpu_context_deinit(&unit->ctx);
    lv_memzero(&g_caps, sizeof(g_caps));
}

bool lv_evgpu_lib_is_ready(void)
{
    return g_caps.context_ok && g_caps.shader_ok;
}

const lv_evgpu_lib_caps_t * lv_evgpu_lib_get_caps(void)
{
    return &g_caps;
}

#endif /*LV_USE_DRAW_EVGPU && LV_USE_EVGPU_LIB*/
