#if LV_BUILD_TEST
#include "../lvgl.h"
#include "../../lvgl_private.h"
#include "unity/unity.h"

#if LV_USE_DRAW_GPU_RENDERER && LV_USE_3D && LV_USE_3DBUTTON
#include "src/draw/gpu_renderer/lv_draw_gpu_renderer.h"

static void build_3dbutton_scene(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_0, 0);

    lv_obj_t * cam = lv_3dcamera_create(scr);
    lv_3dcamera_set_perspective(cam, 38.0f, 10.0f, 5000.0f);
    lv_3dcamera_look_at(cam,
                        (lv_vec3_t) { 0.0f, 180.0f, 320.0f },
                        (lv_vec3_t) { 0.0f, 0.0f, -280.0f },
                        (lv_vec3_t) { 0.0f, 1.0f, 0.0f });

    lv_obj_t * scene = lv_3dscene_create(scr);
    lv_obj_t * vp = lv_3dviewport_create(scr);
    lv_obj_set_size(vp, lv_pct(100), lv_pct(100));
    lv_3dviewport_set_camera(vp, cam);
    lv_3dviewport_set_scene(vp, scene);

    lv_obj_t * btn = lv_3dbutton_create(scene);
    lv_3dbutton_set_box_size(btn, 300.0f, 18.0f, 72.0f);
    lv_3dbutton_set_corner_radius(btn, 16.0f);
    lv_3dbutton_set_colors(btn, lv_color_hex(0x1E88E5), lv_color_hex(0x0D47A1));
    lv_3dbutton_set_hover_lift(btn, 12.0f, 1.035f);
    lv_3dbutton_set_press_depth(btn, -16.0f, 0.96f);
    lv_3dbutton_set_tilt(btn, 0.0f, 0.0f);
    lv_3dbutton_place(btn, 0.0f, 0.0f, -280.0f);
}
#endif

void setUp(void)
{
}

void tearDown(void)
{
    lv_obj_clean(lv_screen_active());
}

void test_gpu_renderer_config_enabled(void)
{
#if LV_USE_DRAW_GPU_RENDERER && LV_USE_3D
    TEST_ASSERT_EQUAL(1, LV_USE_DRAW_GPU_RENDERER);
    TEST_ASSERT_EQUAL(1, LV_USE_3D);
#else
    TEST_IGNORE_MESSAGE("gpu_renderer not enabled in this build");
#endif
}

void test_gpu_renderer_3dbutton_framegraph(void)
{
#if LV_USE_DRAW_GPU_RENDERER && LV_USE_3D && LV_USE_3DBUTTON
    build_3dbutton_scene();

    lv_refr_now(NULL);
    lv_test_wait(16);
    lv_refr_now(NULL);

    lv_gpu_renderer_verify_stats_t stats;
    lv_display_t * disp = lv_display_get_default();
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_TRUE(lv_gpu_renderer_verify_stats(disp, &stats));

    TEST_ASSERT_GREATER_OR_EQUAL(1, stats.gpu_3d_draws);
    TEST_ASSERT_GREATER_OR_EQUAL(1, stats.fg_pass_count);
    TEST_ASSERT_LESS_OR_EQUAL(1, stats.fg_gl_finish_count);
    TEST_ASSERT_GREATER_OR_EQUAL(32, stats.region_max_alpha);
    TEST_ASSERT_GREATER_OR_EQUAL(4, stats.region_bluish_count);
#else
    TEST_IGNORE_MESSAGE("gpu_renderer / 3D / 3dbutton not enabled");
#endif
}

#endif
