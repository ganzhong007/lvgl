/**
 * @file lv_linux_drm_common.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../../lvgl_public.h"

#if LV_USE_LINUX_DRM

#include <dirent.h>
#include <string.h>
#include <xf86drmMode.h>
#include "lv_linux_drm_private.h"

/*********************
 *      DEFINES
 *********************/

#define LV_DRM_CLASS_DIR "/sys/class/drm"
#define LV_DRM_CARD_PATH "/dev/dri/card"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static char * find_by_class(void);

static int drm_connector_dir_score(const char * name)
{
    if(!name) return 0;
    /* Prefer DP (zynqmp-display) over DPI panel when both are connected */
    if(strstr(name, "-DP-") != NULL) return 100;
    if(strstr(name, "-HDMI-") != NULL) return 90;
    if(strstr(name, "-DPI-") != NULL) return 10;
    return 50;
}

static char * card_path_from_connector_dir(const char * name)
{
    if(!name || lv_strncmp(name, "card", 4) != 0) {
        return NULL;
    }
    const size_t buf_size = lv_strlen(LV_DRM_CARD_PATH) + 3;
    char * card_path = lv_zalloc(buf_size);
    if(!card_path) {
        return NULL;
    }
    if(name[5] != '-') {
        lv_snprintf(card_path, buf_size, LV_DRM_CARD_PATH "%c%c", name[4], name[5]);
    }
    else {
        lv_snprintf(card_path, buf_size, LV_DRM_CARD_PATH "%c", name[4]);
    }
    return card_path;
}

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

char * lv_linux_drm_find_device_path(void)
{
    return find_by_class();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static char * find_by_class(void)
{
    DIR * d = opendir(LV_DRM_CLASS_DIR);
    if(!d) {
        return NULL;
    }

    int best_score = -1;
    char best_name[64];
    best_name[0] = '\0';

    struct dirent * ent;
    while((ent = readdir(d)) != NULL) {
        if(lv_strcmp(ent->d_name, ".") == 0 || lv_strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        bool is_card = lv_strncmp(ent->d_name, "card", 4) == 0;
        bool is_connected = lv_strchr(ent->d_name, '-') != NULL;

        if(!is_card || !is_connected) {
            continue;
        }

        int score = drm_connector_dir_score(ent->d_name);
        if(score > best_score) {
            best_score = score;
            lv_strncpy(best_name, ent->d_name, sizeof(best_name) - 1);
            best_name[sizeof(best_name) - 1] = '\0';
        }
    }

    closedir(d);

    if(best_score < 0) {
        return NULL;
    }

    return card_path_from_connector_dir(best_name);
}

int32_t lv_linux_drm_mode_get_horizontal_resolution(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return 0;
    }
    return mode->mode_info->hdisplay;
}

int32_t lv_linux_drm_mode_get_vertical_resolution(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return 0;
    }
    return mode->mode_info->vdisplay;
}

int32_t lv_linux_drm_mode_get_refresh_rate(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return 0;
    }
    return mode->mode_info->vrefresh;
}

bool lv_linux_drm_mode_is_preferred(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return false;
    }
    return (mode->mode_info->type & DRM_MODE_TYPE_PREFERRED) != 0;
}

void * lv_linux_drm_mode_get_raw(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return NULL;
    }
    return mode->mode_info;
}

#endif /*LV_USE_LINUX_DRM*/
