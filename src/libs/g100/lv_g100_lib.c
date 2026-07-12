/**
 * @file lv_g100_lib.c
 * @brief G100 GLES2 runtime facade (shader + context); draw/g100 stays task adapter.
 */

#include "lv_g100_lib.h"

#if LV_USE_DRAW_G100 && LV_USE_G100_LIB

#include "../../draw/g100/lv_g100_context.h"
#include "../../draw/g100/lv_g100_shader.h"
#include "../../draw/g100/lv_draw_g100_private.h"

static lv_g100_lib_caps_t g_caps;

void lv_g100_lib_init(lv_draw_g100_unit_t * unit)
{
    lv_memzero(&g_caps, sizeof(g_caps));
    LV_ASSERT_NULL(unit);

    lv_g100_context_init(unit, &unit->ctx);
    lv_g100_shader_init(unit, &unit->ctx, &unit->shader);

    g_caps.context_ok = lv_g100_context_is_ready(&unit->ctx);
    g_caps.shader_ok = lv_g100_shader_is_ready(&unit->shader);

    if(g_caps.context_ok && g_caps.shader_ok) {
        LV_LOG_INFO("G100 lib ready (shader=%u, path=libs/g100)",
                    (unsigned)lv_g100_shader_get_solid_program(&unit->shader));
    }
    else {
        LV_LOG_WARN("G100 lib init incomplete (context=%d shader=%d)",
                    g_caps.context_ok, g_caps.shader_ok);
    }
}

void lv_g100_lib_deinit(lv_draw_g100_unit_t * unit)
{
    LV_ASSERT_NULL(unit);
    lv_g100_shader_deinit(&unit->shader);
    lv_g100_context_deinit(&unit->ctx);
    lv_memzero(&g_caps, sizeof(g_caps));
}

bool lv_g100_lib_is_ready(void)
{
    return g_caps.context_ok && g_caps.shader_ok;
}

const lv_g100_lib_caps_t * lv_g100_lib_get_caps(void)
{
    return &g_caps;
}

#endif /*LV_USE_DRAW_G100 && LV_USE_G100_LIB*/
