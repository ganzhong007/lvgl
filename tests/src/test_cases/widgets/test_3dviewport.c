#if LV_BUILD_TEST
#include "../lvgl.h"
#include "../../lvgl_private.h"

#include "unity/unity.h"

void setUp(void)
{
}

void tearDown(void)
{
    lv_obj_clean(lv_screen_active());
}

#if LV_USE_3D_DRAW_TASKS && LV_USE_3DVIEWPORT && \
    (LV_USE_DRAW_EVGPU || LV_USE_DRAW_EVGPU_C_R_T)

/**
 * Smoke: viewport clear + grid → 3D_VIEWPORT / 3D_CLEAR / 3D_LINE.
 * Requires EVGPU or EVGPU_C_R_T (SW has no 3D_* execute path).
 */
void test_3dviewport_grid_smoke(void)
{
    lv_obj_t * vp = lv_3dviewport_create(lv_screen_active());
    TEST_ASSERT_NOT_NULL(vp);

    lv_obj_set_size(vp, 320, 240);
    lv_obj_center(vp);
    lv_3dviewport_set_clear_color(vp, lv_color_hex(0x203040), LV_OPA_COVER);
    lv_3dviewport_set_clear_depth(vp, true);
    lv_3dviewport_set_grid_visible(vp, true);
    lv_3dviewport_set_orbit(vp, 0.6f, 0.35f, 4.0f);

    lv_obj_invalidate(vp);
    lv_refr_now(NULL);

    TEST_ASSERT_EQUAL_INT(320, lv_obj_get_width(vp));
    TEST_ASSERT_EQUAL_INT(240, lv_obj_get_height(vp));
}

#if LV_USE_3DMESH

/**
 * Smoke: mesh box child → 3D_MESH (plus viewport pass tasks).
 */
void test_3dviewport_mesh_box_smoke(void)
{
    lv_obj_t * vp = lv_3dviewport_create(lv_screen_active());
    TEST_ASSERT_NOT_NULL(vp);

    lv_obj_set_size(vp, 320, 240);
    lv_obj_center(vp);
    lv_3dviewport_set_clear_color(vp, lv_color_hex(0x101820), LV_OPA_COVER);
    lv_3dviewport_set_grid_visible(vp, false);
    lv_3dviewport_set_orbit(vp, 0.8f, 0.4f, 3.5f);

    lv_obj_t * mesh = lv_3dmesh_create(vp);
    TEST_ASSERT_NOT_NULL(mesh);
    lv_3dmesh_set_box(mesh, 1.0f, 1.0f, 1.0f);
    lv_3dmesh_set_color(mesh, lv_palette_main(LV_PALETTE_ORANGE), LV_OPA_COVER);
    lv_3dmesh_set_depth_test(mesh, true);

    lv_obj_invalidate(vp);
    lv_refr_now(NULL);

    TEST_ASSERT_EQUAL_PTR(vp, lv_obj_get_parent(mesh));
}

#else /* !LV_USE_3DMESH */

void test_3dviewport_mesh_box_smoke(void)
{
    TEST_IGNORE_MESSAGE("LV_USE_3DMESH is disabled");
}

#endif /* LV_USE_3DMESH */

#else /* 3D path unavailable */

void test_3dviewport_grid_smoke(void)
{
    TEST_IGNORE_MESSAGE("3D viewport needs LV_USE_3D_DRAW_TASKS + EVGPU/C_R_T");
}

void test_3dviewport_mesh_box_smoke(void)
{
    TEST_IGNORE_MESSAGE("3D viewport needs LV_USE_3D_DRAW_TASKS + EVGPU/C_R_T");
}

#endif

#endif /*LV_BUILD_TEST*/
