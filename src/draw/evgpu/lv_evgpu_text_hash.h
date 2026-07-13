/**
 * @file lv_evgpu_text_hash.h
 * @brief Label string content hash for EVGPU (CP-02, §4.8.4).
 */

#ifndef LV_EVGPU_TEXT_HASH_H
#define LV_EVGPU_TEXT_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

#include "../../../include/lvgl/draw/lv_draw_label.h"

/**
 * Cache key derived from label draw descriptor content (never uses text pointer).
 */
typedef struct {
    const lv_font_t * font;
    uint32_t text_len;
    uint32_t text_hash;
    lv_color32_t color;
    lv_color32_t sel_color;
    lv_color32_t sel_bg_color;
    lv_color32_t outline_stroke_color;
    int32_t line_space;
    int32_t letter_space;
    int32_t ofs_x;
    int32_t ofs_y;
    int32_t rotation;
    uint32_t sel_start;
    uint32_t sel_end;
    lv_opa_t opa;
    lv_opa_t outline_stroke_opa;
    int32_t outline_stroke_width;
    lv_text_align_t align;
    lv_base_dir_t bidi_dir;
    lv_text_decor_t decor;
    lv_text_flag_t flag;
} lv_evgpu_label_text_key_t;

struct _lv_evgpu_label_text_cache_t {
    lv_evgpu_label_text_key_t last_key;
    bool valid;
    uint32_t streak;
};

typedef struct _lv_evgpu_label_text_cache_t lv_evgpu_label_text_cache_t;

/**
 * Compute 32-bit hash of label text bytes (FNV-1a).
 */
uint32_t lv_evgpu_text_hash_compute(const char * text, uint32_t text_length);

/**
 * Fill a label cache key from a draw descriptor (uses content hash, not text pointer).
 */
void lv_evgpu_label_text_key_from_dsc(lv_evgpu_label_text_key_t * key, const lv_draw_label_dsc_t * dsc);

/**
 * Compare two label cache keys.
 */
bool lv_evgpu_label_text_key_equal(const lv_evgpu_label_text_key_t * a, const lv_evgpu_label_text_key_t * b);

void lv_evgpu_label_text_cache_init(lv_evgpu_label_text_cache_t * cache);

/**
 * Update cache for the current label draw. Logs hit/miss for TX-03 verification.
 * @return true if content matches the previous label key (hash hit).
 */
bool lv_evgpu_label_text_cache_touch(lv_evgpu_label_text_cache_t * cache, const lv_draw_label_dsc_t * dsc);

#endif /*LV_USE_DRAW_EVGPU*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EVGPU_TEXT_HASH_H*/
