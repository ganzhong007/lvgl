/**
 * @file lv_test_conf_evgpu_c_r_t.h
 * Unity OPTIONS_TEST_EVGPU_C_R_T: DrawUnit C_R_T vs SW ref_imgs/.
 * Include AFTER lv_test_conf_full.h so these overrides win.
 */
#ifndef LV_TEST_CONF_EVGPU_C_R_T_H
#define LV_TEST_CONF_EVGPU_C_R_T_H

#undef LV_USE_OPENGLES
#define LV_USE_OPENGLES 1

#undef LV_USE_EGL
#define LV_USE_EGL 1

#undef LV_USE_NANOVG
#define LV_USE_NANOVG 0

#undef LV_USE_DRAW_NANOVG
#define LV_USE_DRAW_NANOVG 0

#undef LV_USE_DRAW_OPENGLES
#define LV_USE_DRAW_OPENGLES 0

#undef LV_USE_DRAW_EVGPU
#define LV_USE_DRAW_EVGPU 0

#undef LV_USE_DRAW_EVGPUGANESH
#define LV_USE_DRAW_EVGPUGANESH 0

#undef LV_USE_DRAW_EVGPU_C_R_T
#define LV_USE_DRAW_EVGPU_C_R_T 1

#undef LV_USE_DRAW_SW
#define LV_USE_DRAW_SW 1

/* Avoid building unused display backends under Unity (Werror + unused params). */
#undef LV_USE_WAYLAND
#define LV_USE_WAYLAND 0
#undef LV_USE_LINUX_DRM
#define LV_USE_LINUX_DRM 0
#undef LV_USE_X11
#define LV_USE_X11 0
#undef LV_USE_GLFW
#define LV_USE_GLFW 0
#undef LV_USE_SDL
#define LV_USE_SDL 0

/* 3D DrawTasks / widgets for Unity smoke (SW OPTIONS leave these off). */
#undef LV_USE_3D_DRAW_TASKS
#define LV_USE_3D_DRAW_TASKS 1
#undef LV_USE_3DVIEWPORT
#define LV_USE_3DVIEWPORT 1
#undef LV_USE_3DMESH
#define LV_USE_3DMESH 1
#undef LV_USE_3DLIGHT
#define LV_USE_3DLIGHT 1

#endif /* LV_TEST_CONF_EVGPU_C_R_T_H */
