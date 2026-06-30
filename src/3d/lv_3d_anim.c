/**
 * @file lv_3d_anim.c
 */

#include "../include/lvgl/3d/lv_3d_anim.h"

#if LV_USE_3D && LV_USE_3D_WIDGETS

#include "../widgets/3d/lv_3dmesh_private.h"
#include "../include/lvgl/core/lv_anim.h"

#define LV_ANIM_3D_RANGE 1024

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

typedef struct {
    lv_obj_t * mesh;
    float sx, sy, sz;
    float ex, ey, ez;
} lv_anim_3d_pos_ctx_t;

typedef struct {
    lv_obj_t * mesh;
    float sp, sy, sr;
    float ep, ey, er;
} lv_anim_3d_rot_ctx_t;

typedef struct {
    lv_obj_t * mesh;
    float ssx, ssy, ssz;
    float esx, esy, esz;
} lv_anim_3d_scale_ctx_t;

typedef struct {
    lv_obj_t * mesh;
    lv_opa_t so;
    lv_opa_t eo;
} lv_anim_3d_opa_ctx_t;

static float anim_lerp(float a, float b, int32_t v)
{
    float t = (float)v / (float)LV_ANIM_3D_RANGE;
    return a + (b - a) * t;
}

static void anim_ctx_deleted(lv_anim_t * a)
{
    lv_free(a->var);
}

static void pos_anim_exec(void * var, int32_t v)
{
    lv_anim_3d_pos_ctx_t * ctx = var;
    float x = anim_lerp(ctx->sx, ctx->ex, v);
    float y = anim_lerp(ctx->sy, ctx->ey, v);
    float z = anim_lerp(ctx->sz, ctx->ez, v);
    lv_3dmesh_set_position(ctx->mesh, x, y, z);
}

static void rot_anim_exec(void * var, int32_t v)
{
    lv_anim_3d_rot_ctx_t * ctx = var;
    float p = anim_lerp(ctx->sp, ctx->ep, v);
    float y = anim_lerp(ctx->sy, ctx->ey, v);
    float r = anim_lerp(ctx->sr, ctx->er, v);
    lv_3dmesh_set_rotation(ctx->mesh, p, y, r);
}

static void scale_anim_exec(void * var, int32_t v)
{
    lv_anim_3d_scale_ctx_t * ctx = var;
    float sx = anim_lerp(ctx->ssx, ctx->esx, v);
    float sy = anim_lerp(ctx->ssy, ctx->esy, v);
    float sz = anim_lerp(ctx->ssz, ctx->esz, v);
    lv_3dmesh_set_scale(ctx->mesh, sx, sy, sz);
}

static void opa_anim_exec(void * var, int32_t v)
{
    lv_anim_3d_opa_ctx_t * ctx = var;
    float t = (float)v / (float)LV_ANIM_3D_RANGE;
    lv_opa_t opa = (lv_opa_t)(ctx->so + (int32_t)((int32_t)ctx->eo - (int32_t)ctx->so) * t);
    lv_3dmesh_set_opa(ctx->mesh, opa);
}

static void start_anim(void * ctx, lv_anim_exec_xcb_t exec_cb, uint32_t ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ctx);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, 0, LV_ANIM_3D_RANGE);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_deleted_cb(&a, anim_ctx_deleted);
    lv_anim_start(&a);
}

void lv_anim_3d_position(lv_obj_t * obj, float x, float y, float z, uint32_t ms)
{
    if(!lv_obj_has_class(obj, &lv_3dmesh_class)) return;
    lv_anim_3d_pos_ctx_t * ctx = lv_malloc(sizeof(*ctx));
    if(!ctx) return;
    ctx->mesh = obj;
    lv_3dmesh_get_position(obj, &ctx->sx, &ctx->sy, &ctx->sz);
    ctx->ex = x;
    ctx->ey = y;
    ctx->ez = z;
    start_anim(ctx, pos_anim_exec, ms);
}

void lv_anim_3d_rotation(lv_obj_t * obj, float pitch, float yaw, float roll, uint32_t ms)
{
    if(!lv_obj_has_class(obj, &lv_3dmesh_class)) return;
    lv_anim_3d_rot_ctx_t * ctx = lv_malloc(sizeof(*ctx));
    if(!ctx) return;
    ctx->mesh = obj;
    lv_3dmesh_get_rotation(obj, &ctx->sp, &ctx->sy, &ctx->sr);
    ctx->ep = pitch;
    ctx->ey = yaw;
    ctx->er = roll;
    start_anim(ctx, rot_anim_exec, ms);
}

void lv_anim_3d_scale(lv_obj_t * obj, float sx, float sy, float sz, uint32_t ms)
{
    if(!lv_obj_has_class(obj, &lv_3dmesh_class)) return;
    lv_anim_3d_scale_ctx_t * ctx = lv_malloc(sizeof(*ctx));
    if(!ctx) return;
    ctx->mesh = obj;
    lv_3dmesh_get_scale(obj, &ctx->ssx, &ctx->ssy, &ctx->ssz);
    ctx->esx = sx;
    ctx->esy = sy;
    ctx->esz = sz;
    start_anim(ctx, scale_anim_exec, ms);
}

void lv_anim_3d_opa(lv_obj_t * obj, lv_opa_t opa, uint32_t ms)
{
    if(!lv_obj_has_class(obj, &lv_3dmesh_class)) return;
    lv_anim_3d_opa_ctx_t * ctx = lv_malloc(sizeof(*ctx));
    if(!ctx) return;
    ctx->mesh = obj;
    ctx->so = lv_3dmesh_get_opa(obj);
    ctx->eo = opa;
    start_anim(ctx, opa_anim_exec, ms);
}

#endif /*LV_USE_3D && LV_USE_3D_WIDGETS*/
