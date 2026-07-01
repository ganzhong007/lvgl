#ifndef LV_TEST_DISPLAY_GPU_RENDERER_H
#define LV_TEST_DISPLAY_GPU_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lvgl.h"

#if LV_BUILD_TEST && LV_USE_DRAW_GPU_RENDERER && LV_USE_GLFW

lv_display_t * lv_test_display_gpu_renderer_create(int32_t hor_res, int32_t ver_res);

#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_TEST_DISPLAY_GPU_RENDERER_H */
