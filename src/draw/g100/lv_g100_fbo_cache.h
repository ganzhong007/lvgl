/**
 * @file lv_g100_fbo_cache.h
 *
 */

#ifndef LV_G100_FBO_CACHE_H
#define LV_G100_FBO_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl_public.h"

#if LV_USE_DRAW_G100

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_draw_g100_unit_t;
struct _lv_cache_entry_t;
struct NVGLUframebuffer;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Initialize the FBO cache
 * @param u pointer to the nanovg unit
 */
void lv_g100_fbo_cache_init(struct _lv_draw_g100_unit_t * u);

/**
 * @brief Deinitialize the FBO cache
 * @param u pointer to the nanovg unit
 */
void lv_g100_fbo_cache_deinit(struct _lv_draw_g100_unit_t * u);

/**
 * @brief Get the FBO from the cache, create a new one if not found
 * @param u pointer to the nanovg unit
 * @param width the width of the FBO
 * @param height the height of the FBO
 * @param flags the FBO flags
 * @param format the texture format
 * @return the FBO cache entry, or NULL if not found
 */
struct _lv_cache_entry_t * lv_g100_fbo_cache_get(struct _lv_draw_g100_unit_t * u, int width, int height, int flags,
                                                   int format);

/**
 * @brief Release the FBO from the cache
 * @param u pointer to the nanovg unit
 * @param entry the FBO cache entry to release
 */
void lv_g100_fbo_cache_release(struct _lv_draw_g100_unit_t * u, struct _lv_cache_entry_t * entry);

/**
 * @brief Convert a cache entry to a framebuffer
 * @param entry the FBO cache entry
 * @return the framebuffer pointer
 */
struct NVGLUframebuffer * lv_g100_fbo_cache_entry_to_fb(struct _lv_cache_entry_t * entry);

/**********************
 *      MACROS
 **********************/

#endif /* LV_USE_DRAW_G100 */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_G100_FBO_CACHE_H*/
