/**
 * @file lv_evgpu_text_hash.c
 */

#include "lv_evgpu_text_hash.h"

#if LV_USE_DRAW_EVGPU

#include "../../stdlib/lv_string.h"
#include "../../misc/lv_text_private.h"

static uint32_t lv_evgpu_text_len_resolve(const char * text, uint32_t text_length)
{
    if(!text) return 0;
    if(text_length == 0 || text_length == LV_TEXT_LEN_MAX) {
        return (uint32_t)lv_strlen(text);
    }
    return text_length;
}

uint32_t lv_evgpu_text_hash_compute(const char * text, uint32_t text_length)
{
    if(!text) return 0;

    const uint32_t len = lv_evgpu_text_len_resolve(text, text_length);

    uint32_t h = 2166136261u;
    for(uint32_t i = 0; i < len; i++) {
        h ^= (uint8_t)text[i];
        h *= 16777619u;
    }
    return h;
}

void lv_evgpu_label_text_key_from_dsc(lv_evgpu_label_text_key_t * key, const lv_draw_label_dsc_t * dsc)
{
    LV_ASSERT_NULL(key);
    LV_ASSERT_NULL(dsc);

    lv_memzero(key, sizeof(*key));
    if(!dsc->font || !dsc->text) return;

    key->font = dsc->font;
    key->text_len = lv_evgpu_text_len_resolve(dsc->text, dsc->text_length);
    key->text_hash = lv_evgpu_text_hash_compute(dsc->text, dsc->text_length);
    key->color = lv_color_to_32(dsc->color, dsc->opa);
    key->sel_color = lv_color_to_32(dsc->sel_color, dsc->opa);
    key->sel_bg_color = lv_color_to_32(dsc->sel_bg_color, dsc->opa);
    key->outline_stroke_color = lv_color_to_32(dsc->outline_stroke_color, dsc->outline_stroke_opa);
    key->line_space = dsc->line_space;
    key->letter_space = dsc->letter_space;
    key->ofs_x = dsc->ofs_x;
    key->ofs_y = dsc->ofs_y;
    key->rotation = dsc->rotation;
    key->sel_start = dsc->sel_start;
    key->sel_end = dsc->sel_end;
    key->opa = dsc->opa;
    key->outline_stroke_opa = dsc->outline_stroke_opa;
    key->outline_stroke_width = dsc->outline_stroke_width;
    key->align = dsc->align;
    key->bidi_dir = dsc->bidi_dir;
    key->decor = dsc->decor;
    key->flag = dsc->flag;
}

bool lv_evgpu_label_text_key_equal(const lv_evgpu_label_text_key_t * a, const lv_evgpu_label_text_key_t * b)
{
    LV_ASSERT_NULL(a);
    LV_ASSERT_NULL(b);

    if(a->font != b->font) return false;
    if(a->text_len != b->text_len) return false;
    if(a->text_hash != b->text_hash) return false;
    if(!lv_color32_eq(a->color, b->color)) return false;
    if(!lv_color32_eq(a->sel_color, b->sel_color)) return false;
    if(!lv_color32_eq(a->sel_bg_color, b->sel_bg_color)) return false;
    if(!lv_color32_eq(a->outline_stroke_color, b->outline_stroke_color)) return false;
    if(a->line_space != b->line_space) return false;
    if(a->letter_space != b->letter_space) return false;
    if(a->ofs_x != b->ofs_x) return false;
    if(a->ofs_y != b->ofs_y) return false;
    if(a->rotation != b->rotation) return false;
    if(a->sel_start != b->sel_start) return false;
    if(a->sel_end != b->sel_end) return false;
    if(a->opa != b->opa) return false;
    if(a->outline_stroke_opa != b->outline_stroke_opa) return false;
    if(a->outline_stroke_width != b->outline_stroke_width) return false;
    if(a->align != b->align) return false;
    if(a->bidi_dir != b->bidi_dir) return false;
    if(a->decor != b->decor) return false;
    if(a->flag != b->flag) return false;
    return true;
}

void lv_evgpu_label_text_cache_init(lv_evgpu_label_text_cache_t * cache)
{
    LV_ASSERT_NULL(cache);
    lv_memzero(cache, sizeof(*cache));
}

bool lv_evgpu_label_text_cache_touch(lv_evgpu_label_text_cache_t * cache, const lv_draw_label_dsc_t * dsc)
{
    LV_ASSERT_NULL(cache);
    LV_ASSERT_NULL(dsc);

    lv_evgpu_label_text_key_t key;
    lv_evgpu_label_text_key_from_dsc(&key, dsc);

    if(cache->valid && lv_evgpu_label_text_key_equal(&cache->last_key, &key)) {
        cache->streak++;
        if(cache->streak == 1 || (cache->streak % 128) == 0) {
            LV_LOG_INFO("EVGPU label text hash hit (hash=0x%08x, streak=%u)", (unsigned)key.text_hash,
                        (unsigned)cache->streak);
        }
        return true;
    }

    LV_LOG_INFO("EVGPU label text hash miss (hash=0x%08x, len=%u, static=%u)", (unsigned)key.text_hash,
                (unsigned)key.text_len, (unsigned)dsc->text_static);
    cache->last_key = key;
    cache->valid = true;
    cache->streak = 0;
    return false;
}

#endif /*LV_USE_DRAW_EVGPU*/
