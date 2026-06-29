/**
 * @file lv_3d_plane_bake.c
 */

#include "../include/lvgl/3d/lv_3d_plane_bake.h"

#if LV_USE_3D && LV_USE_SNAPSHOT

#include "../include/lvgl/draw/lv_snapshot.h"
#include "../drivers/opengles/lv_opengles_debug.h"
#include "../drivers/opengles/lv_opengles_private.h"
#include "../misc/lv_color.h"
#include <stdio.h>

#define LV_3D_SNAPSHOT_POOL_SIZE 32

typedef struct {
    bool used;
    lv_obj_t * source;
    lv_3d_plane_src_t src;
    bool invalidated;
    lv_draw_buf_t * buf;
    unsigned int tex_id;
    bool tex_uploaded;
} lv_3d_snapshot_slot_t;

static lv_3d_snapshot_slot_t snapshot_pool[LV_3D_SNAPSHOT_POOL_SIZE];

static lv_3d_snapshot_slot_t * slot_by_id(lv_3d_snapshot_id_t id)
{
    if(id == 0 || id >= LV_3D_SNAPSHOT_POOL_SIZE) return NULL;
    if(!snapshot_pool[id].used) return NULL;
    return &snapshot_pool[id];
}

static lv_3d_snapshot_slot_t * slot_by_source(lv_obj_t * obj)
{
    for(uint32_t i = 1; i < LV_3D_SNAPSHOT_POOL_SIZE; i++) {
        if(snapshot_pool[i].used && snapshot_pool[i].source == obj) return &snapshot_pool[i];
    }
    return NULL;
}

static lv_3d_snapshot_id_t slot_alloc(lv_obj_t * obj, lv_3d_plane_src_t src)
{
    lv_3d_snapshot_slot_t * existing = slot_by_source(obj);
    if(existing) return (lv_3d_snapshot_id_t)(existing - snapshot_pool);

    for(uint32_t i = 1; i < LV_3D_SNAPSHOT_POOL_SIZE; i++) {
        if(!snapshot_pool[i].used) {
            snapshot_pool[i].used = true;
            snapshot_pool[i].source = obj;
            snapshot_pool[i].src = src;
            snapshot_pool[i].invalidated = true;
            snapshot_pool[i].buf = NULL;
            snapshot_pool[i].tex_id = 0;
            snapshot_pool[i].tex_uploaded = false;
            return (lv_3d_snapshot_id_t)i;
        }
    }
    return LV_3D_SNAPSHOT_ID_NONE;
}

static bool slot_refresh(lv_3d_snapshot_slot_t * slot)
{
    if(!slot || !slot->source) return false;
    if(slot->src == LV_3D_PLANE_SRC_SNAPSHOT && slot->buf && !slot->invalidated) return true;

    lv_draw_buf_t * snap = lv_snapshot_take(slot->source, LV_COLOR_FORMAT_ARGB8888);
    if(!snap) return false;

    if(slot->buf) lv_draw_buf_destroy(slot->buf);
    slot->buf = snap;
    slot->invalidated = false;
    slot->tex_uploaded = false;
    return true;
}

static void bgra8888_row_to_rgba8888(uint8_t * dst, const uint8_t * src, int32_t w)
{
    for(int32_t x = 0; x < w; x++) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = src[3];
        src += 4;
        dst += 4;
    }
}

static void slot_upload_gl(lv_3d_snapshot_slot_t * slot)
{
    if(!slot || !slot->buf || slot->tex_uploaded) return;

    const int32_t w = slot->buf->header.w;
    const int32_t h = slot->buf->header.h;
    if(w <= 0 || h <= 0) return;

    if(slot->tex_id == 0) {
        GL_CALL(glGenTextures(1, &slot->tex_id));
    }

    const uint32_t stride = slot->buf->header.stride;
    const size_t rgba_bytes = (size_t)w * (size_t)h * 4u;
    uint8_t * rgba = lv_malloc(rgba_bytes);
    if(!rgba) return;

    for(int32_t y = 0; y < h; y++) {
        bgra8888_row_to_rgba8888(rgba + (size_t)y * (size_t)w * 4u,
                                 slot->buf->data + (uint32_t)y * stride, w);
    }

    GL_CALL(glBindTexture(GL_TEXTURE_2D, slot->tex_id));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    /* LVGL ARGB8888 draw buf is lv_color32_t (B,G,R,A); upload as canonical GL RGBA. */
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba));
    lv_free(rgba);
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    slot->tex_uploaded = true;
}

lv_3d_snapshot_id_t lv_3d_plane_bake(lv_obj_t * obj, lv_3d_plane_src_t src)
{
    if(!obj) return LV_3D_SNAPSHOT_ID_NONE;

    lv_3d_snapshot_id_t id = slot_alloc(obj, src);
    lv_3d_snapshot_slot_t * slot = slot_by_id(id);
    if(!slot) return LV_3D_SNAPSHOT_ID_NONE;

    slot->src = src;
    if(!slot_refresh(slot)) return LV_3D_SNAPSHOT_ID_NONE;
    return id;
}

void lv_3d_plane_invalidate(lv_obj_t * obj)
{
    lv_3d_snapshot_slot_t * slot = slot_by_source(obj);
    if(!slot) return;
    slot->invalidated = true;
    slot->tex_uploaded = false;
}

void lv_3d_plane_upload_all(void)
{
    for(uint32_t i = 1; i < LV_3D_SNAPSHOT_POOL_SIZE; i++) {
        if(snapshot_pool[i].used && snapshot_pool[i].buf && !snapshot_pool[i].tex_uploaded) {
            slot_upload_gl(&snapshot_pool[i]);
        }
    }
}

unsigned int lv_3d_plane_get_gl_texture(lv_3d_snapshot_id_t id)
{
    lv_3d_snapshot_slot_t * slot = slot_by_id(id);
    if(!slot || !slot->tex_uploaded) return 0;
    return slot->tex_id;
}

bool lv_3d_plane_dump_snapshot_lvgl(lv_3d_snapshot_id_t id, const char * path)
{
    if(!path) return false;
    lv_3d_snapshot_slot_t * slot = slot_by_id(id);
    if(!slot || !slot->buf || !slot->buf->data) return false;

    const int32_t w = slot->buf->header.w;
    const int32_t h = slot->buf->header.h;
    if(w < 1 || h < 1) return false;

    FILE * f = fopen(path, "wb");
    if(!f) return false;

    uint32_t wh[2] = { (uint32_t)w, (uint32_t)h };
    if(fwrite(wh, 1, sizeof(wh), f) != sizeof(wh)) {
        fclose(f);
        return false;
    }

    const uint32_t stride = slot->buf->header.stride;
    for(int32_t y = 0; y < h; y++) {
        const uint8_t * row = slot->buf->data + (uint32_t)y * stride;
        if(fwrite(row, 1, (size_t)w * 4, f) != (size_t)w * 4) {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

#endif /*LV_USE_3D && LV_USE_SNAPSHOT*/
