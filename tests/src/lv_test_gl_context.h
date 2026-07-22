/**
 * @file lv_test_gl_context.h
 * Headless EGL + GLES2 context for EVGPU / C_R_T Unity tests.
 */
#ifndef LV_TEST_GL_CONTEXT_H
#define LV_TEST_GL_CONTEXT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a surfaceless/pbuffer GLES2 context, load GL entry points, and call
 * lv_opengles_init() so EVGPU / C_R_T draw units register.
 * No-op (returns true) when neither draw unit is enabled.
 */
bool lv_test_gl_context_init(void);

/** Tear down EGL context created by lv_test_gl_context_init(). */
void lv_test_gl_context_deinit(void);

/**
 * Ensure the default display's layer_head has a GPU FBO, refresh, then
 * read back into the CPU draw buffer used by screenshot compare.
 * No-op when neither EVGPU nor C_R_T is enabled.
 */
void lv_test_gl_prepare_screenshot(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_TEST_GL_CONTEXT_H */
