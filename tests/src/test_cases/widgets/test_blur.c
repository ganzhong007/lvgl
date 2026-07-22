#if LV_BUILD_TEST
#include "../lvgl.h"
#include "../../lvgl_private.h"

#include "unity/unity.h"

void setUp(void)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
}

void tearDown(void)
{
    lv_obj_clean(lv_screen_active());
}

static void add_backdrop_pattern(void)
{
    lv_obj_t * a = lv_obj_create(lv_screen_active());
    lv_obj_set_size(a, 120, 80);
    lv_obj_set_pos(a, 40, 40);
    lv_obj_set_style_bg_color(a, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(a, 0, 0);

    lv_obj_t * b = lv_obj_create(lv_screen_active());
    lv_obj_set_size(b, 120, 80);
    lv_obj_set_pos(b, 200, 100);
    lv_obj_set_style_bg_color(b, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(b, 0, 0);

    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "BLUR backdrop / content");
    lv_obj_set_pos(label, 60, 200);
    lv_obj_set_style_text_color(label, lv_color_hex(0x222222), 0);
}

/**
 * Style blur with backdrop=false: blur the object's own drawn content
 * (LV_EVENT_DRAW_MAIN_END → LV_DRAW_TASK_TYPE_BLUR).
 */
void test_blur_style_radius(void)
{
    add_backdrop_pattern();

    lv_obj_t * panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel, 180, 120);
    lv_obj_set_pos(panel, 280, 40);
    lv_obj_set_style_bg_color(panel, lv_palette_lighten(LV_PALETTE_GREEN, 2), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_blur_radius(panel, 12, 0);
    lv_obj_set_style_blur_backdrop(panel, false, 0);

    lv_obj_t * label = lv_label_create(panel);
    lv_label_set_text(label, "blur me");
    lv_obj_center(label);

    TEST_ASSERT_EQUAL_SCREENSHOT("widgets/blur_style_radius.png");
}

/**
 * Style blur with backdrop=true: blur content behind the object
 * (LV_EVENT_DRAW_MAIN → LV_DRAW_TASK_TYPE_BLUR).
 */
void test_blur_style_backdrop(void)
{
    add_backdrop_pattern();

    lv_obj_t * frost = lv_obj_create(lv_screen_active());
    lv_obj_set_size(frost, 200, 140);
    lv_obj_set_pos(frost, 100, 60);
    lv_obj_set_style_bg_color(frost, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(frost, LV_OPA_40, 0);
    lv_obj_set_style_radius(frost, 16, 0);
    lv_obj_set_style_border_width(frost, 0, 0);
    lv_obj_set_style_blur_radius(frost, 16, 0);
    lv_obj_set_style_blur_backdrop(frost, true, 0);

    lv_obj_t * label = lv_label_create(frost);
    lv_label_set_text(label, "frosted");
    lv_obj_center(label);

    TEST_ASSERT_EQUAL_SCREENSHOT("widgets/blur_style_backdrop.png");
}

#endif /*LV_BUILD_TEST*/
