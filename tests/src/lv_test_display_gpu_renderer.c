#include "../lvgl.h"

#if LV_BUILD_TEST && LV_USE_DRAW_GPU_RENDERER && LV_USE_GLFW

#include "lv_test_display_gpu_renderer.h"
#include <GLFW/glfw3.h>
#include "src/drivers/opengles/lv_opengles_glfw.h"
#include "src/drivers/opengles/lv_opengles_window.h"
#include "src/draw/gpu_renderer/lv_draw_gpu_renderer.h"

lv_display_t * lv_test_display_gpu_renderer_create(int32_t hor_res, int32_t ver_res)
{
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    lv_opengles_window_t * window =
        lv_opengles_glfw_window_create_ex(hor_res, ver_res, false, false, false, "LVGL gpu_renderer test");
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    if(!window) return NULL;

    lv_display_t * disp = lv_opengles_window_display_create(window, hor_res, ver_res);
    if(!disp) {
        lv_opengles_window_delete(window);
        return NULL;
    }

    lv_gpu_renderer_set_ui_mode(LV_GPU_RENDERER_UI_GENERIC);
    return disp;
}

#endif
