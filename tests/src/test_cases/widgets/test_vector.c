#if LV_BUILD_TEST
#include "../lvgl.h"
#include "../../lvgl_private.h"

#include "unity/unity.h"

#ifndef NON_AMD64_BUILD
    #define EXT_NAME ".lp64.png"
#else
    #define EXT_NAME ".lp32.png"
#endif

void setUp(void)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
}

void tearDown(void)
{
    lv_obj_clean(lv_screen_active());
}

#if LV_USE_VECTOR_GRAPHIC

#define CANVAS_W 200
#define CANVAS_H 160

static lv_obj_t * make_canvas(lv_draw_buf_t ** out_buf)
{
    lv_draw_buf_t * draw_buf = lv_draw_buf_create(CANVAS_W, CANVAS_H, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    TEST_ASSERT_NOT_NULL(draw_buf);

    lv_obj_t * canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(canvas, draw_buf);
    lv_canvas_fill_bg(canvas, lv_color_hex3(0xeee), LV_OPA_COVER);
    lv_obj_center(canvas);

    *out_buf = draw_buf;
    return canvas;
}

void test_vector_canvas_fill_path(void)
{
    lv_draw_buf_t * draw_buf = NULL;
    lv_obj_t * canvas = make_canvas(&draw_buf);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_vector_dsc_t * dsc = lv_draw_vector_dsc_create(&layer);
    lv_vector_path_t * path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_MEDIUM);

    lv_fpoint_t pts[] = {{20, 20}, {160, 40}, {100, 140}};
    lv_vector_path_move_to(path, &pts[0]);
    lv_vector_path_line_to(path, &pts[1]);
    lv_vector_path_line_to(path, &pts[2]);
    lv_vector_path_close(path);

    lv_draw_vector_dsc_set_fill_color(dsc, lv_color_make(0x00, 0x80, 0xff));
    lv_draw_vector_dsc_add_path(dsc, path);
    lv_draw_vector(dsc);

    lv_vector_path_delete(path);
    lv_draw_vector_dsc_delete(dsc);
    lv_canvas_finish_layer(canvas, &layer);

    TEST_ASSERT_EQUAL_SCREENSHOT("widgets/vector_canvas_fill" EXT_NAME);

    lv_image_cache_drop(draw_buf);
    lv_draw_buf_destroy(draw_buf);
}

void test_vector_canvas_stroke_path(void)
{
    lv_draw_buf_t * draw_buf = NULL;
    lv_obj_t * canvas = make_canvas(&draw_buf);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_vector_dsc_t * dsc = lv_draw_vector_dsc_create(&layer);
    lv_vector_path_t * path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_MEDIUM);

    lv_draw_vector_dsc_set_fill_opa(dsc, LV_OPA_0);
    lv_draw_vector_dsc_set_stroke_opa(dsc, LV_OPA_COVER);
    lv_draw_vector_dsc_set_stroke_color(dsc, lv_color_make(0xe0, 0x20, 0x20));
    lv_draw_vector_dsc_set_stroke_width(dsc, 6.0f);

    lv_fpoint_t pts[] = {{30, 30}, {170, 50}, {50, 130}, {150, 130}};
    lv_vector_path_move_to(path, &pts[0]);
    lv_vector_path_line_to(path, &pts[1]);
    lv_vector_path_line_to(path, &pts[2]);
    lv_vector_path_line_to(path, &pts[3]);
    lv_draw_vector_dsc_add_path(dsc, path);
    lv_draw_vector(dsc);

    lv_vector_path_delete(path);
    lv_draw_vector_dsc_delete(dsc);
    lv_canvas_finish_layer(canvas, &layer);

    TEST_ASSERT_EQUAL_SCREENSHOT("widgets/vector_canvas_stroke" EXT_NAME);

    lv_image_cache_drop(draw_buf);
    lv_draw_buf_destroy(draw_buf);
}

static void vector_draw_main_cb(lv_event_t * e)
{
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_draw_vector_dsc_t * dsc = lv_draw_vector_dsc_create(layer);
    lv_vector_path_t * path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_MEDIUM);

    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(e), &area);

    lv_fpoint_t c = {
        (area.x1 + area.x2) * 0.5f,
        (area.y1 + area.y2) * 0.5f
    };
    lv_vector_path_append_circle(path, &c, 40, 40);
    lv_draw_vector_dsc_set_fill_color(dsc, lv_palette_main(LV_PALETTE_ORANGE));
    lv_draw_vector_dsc_add_path(dsc, path);
    lv_draw_vector(dsc);

    lv_vector_path_delete(path);
    lv_draw_vector_dsc_delete(dsc);
}

void test_vector_draw_main_event(void)
{
    lv_obj_t * host = lv_obj_create(lv_screen_active());
    lv_obj_set_size(host, 220, 180);
    lv_obj_center(host);
    lv_obj_set_style_bg_color(host, lv_color_hex3(0xddd), 0);
    lv_obj_set_style_border_width(host, 0, 0);
    lv_obj_add_event_cb(host, vector_draw_main_cb, LV_EVENT_DRAW_MAIN, NULL);

    TEST_ASSERT_EQUAL_SCREENSHOT("widgets/vector_draw_main" EXT_NAME);
}

#else /* !LV_USE_VECTOR_GRAPHIC */

void test_vector_canvas_fill_path(void)
{
    TEST_IGNORE_MESSAGE("LV_USE_VECTOR_GRAPHIC is disabled");
}

void test_vector_canvas_stroke_path(void)
{
    TEST_IGNORE_MESSAGE("LV_USE_VECTOR_GRAPHIC is disabled");
}

void test_vector_draw_main_event(void)
{
    TEST_IGNORE_MESSAGE("LV_USE_VECTOR_GRAPHIC is disabled");
}

#endif /* LV_USE_VECTOR_GRAPHIC */

#endif /*LV_BUILD_TEST*/
