#ifndef LV_TEST_CONF_GPU_RENDERER_H
#define LV_TEST_CONF_GPU_RENDERER_H

/** LVGL 2D/3D gpu_renderer backend (OpenGLES + GLFW headless window in tests). */

#define LV_USE_DRAW_GPU_RENDERER        1
#define LV_USE_GPU_RENDERER             1

#define LV_USE_OPENGLES                 1
#define LV_USE_GLFW                     1
#define LV_USE_DRAW_OPENGLES            0
#define LV_USE_DRAW_NANOVG              0
#define LV_USE_NANOVG                   0

#define LV_USE_3D                       1
#define LV_USE_3D_WIDGETS               1
#define LV_USE_3DBUTTON                 1
#define LV_USE_3DSTACK                  1
#define LV_USE_3D_SEGMENT_POOL          1

#define LV_USE_APPWINDOW                0

#define LV_GPU_RENDERER_AR_PASSTHROUGH  1
#define LV_GPU_RENDERER_GLES_API        2
#define LV_GPU_RENDERER_ALLOW_GLES3     0
#define LV_GPU_RENDERER_DEPTH_BITS      16
#define LV_GPU_RENDERER_USE_ETC1        1
#define LV_GPU_RENDERER_LOG_CAPS        0
#define LV_GPU_RENDERER_MSAA_SAMPLES    0
#define LV_GPU_RENDERER_FLUSH_MAX_BATCHES 8

#endif /* LV_TEST_CONF_GPU_RENDERER_H */
