/**
 * @file lv_gpu_composite_caps.c
 */

#include "lv_gpu_composite_caps.h"

#if LV_USE_DRAW_GPU_COMPOSITE

#include "../../drivers/opengles/lv_opengles_debug.h"
#include "../../drivers/opengles/lv_opengles_private.h"
#include <string.h>

void lv_gpu_composite_caps_probe(lv_gpu_composite_caps_t * caps)
{
    lv_memzero(caps, sizeof(*caps));
    const char * ver = (const char *)glGetString(GL_VERSION);
    if(ver && ver[0] == '2') caps->gles_major = 2;
    else if(ver && ver[0] == '3') caps->gles_major = 3;
    else caps->gles_major = 2;

    caps->has_fbo = true;
    caps->has_depth16 = true;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps->max_texture_size);
}

void lv_gpu_composite_caps_log(const lv_gpu_composite_caps_t * caps)
{
#if LV_GPU_COMPOSITE_LOG_CAPS
    LV_LOG_USER("LVGL caps: GLES%u fbo=%d depth16=%d max_tex=%d",
                (unsigned)caps->gles_major, caps->has_fbo, caps->has_depth16, caps->max_texture_size);
#else
    LV_UNUSED(caps);
#endif
}

#endif /*LV_USE_DRAW_GPU_COMPOSITE*/
