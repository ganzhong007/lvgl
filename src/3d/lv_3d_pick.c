/**
 * @file lv_3d_pick.c — viewport ray pick against mesh AABBs
 */

#include "lv_3d_internal.h"

#if LV_USE_3D

#include <float.h>
#include <math.h>

static bool ray_aabb_local(float ox, float oy, float oz,
                           float dx, float dy, float dz,
                           float hx, float hy, float hz, float * t_out)
{
    float tmin = 0.0f;
    float tmax = FLT_MAX;
    const float o[3] = { ox, oy, oz };
    const float d[3] = { dx, dy, dz };
    const float bmin[3] = { -hx, -hy, -hz };
    const float bmax[3] = { hx, hy, hz };

    for(int i = 0; i < 3; i++) {
        if(fabsf(d[i]) < 1e-8f) {
            if(o[i] < bmin[i] || o[i] > bmax[i]) return false;
        }
        else {
            float t1 = (bmin[i] - o[i]) / d[i];
            float t2 = (bmax[i] - o[i]) / d[i];
            if(t1 > t2) {
                float tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            if(t1 > tmin) tmin = t1;
            if(t2 < tmax) tmax = t2;
            if(tmin > tmax) return false;
        }
    }

    if(tmax < 0.0f) return false;
    *t_out = tmin >= 0.0f ? tmin : tmax;
    return true;
}

static bool ray_hit_mesh(const lv_3d_draw_item_t * it, lv_vec3_t origin, lv_vec3_t dir, float * t_out)
{
    float inv_world[16];
    if(!lv_3d_mat4_invert(inv_world, it->transform.world)) return false;

    lv_vec3_t lo, ld;
    lv_3d_mat4_transform_point(inv_world, origin, &lo);
    lv_3d_mat4_transform_dir(inv_world, dir, &ld);
    float dl = sqrtf(ld.x * ld.x + ld.y * ld.y + ld.z * ld.z);
    if(dl > 1e-8f) {
        ld.x /= dl;
        ld.y /= dl;
        ld.z /= dl;
    }

    float hx = it->w * 0.5f;
    float hy = it->h * 0.5f;
    float hz = it->d * 0.5f;
    return ray_aabb_local(lo.x, lo.y, lo.z, ld.x, ld.y, ld.z, hx, hy, hz, t_out);
}

lv_obj_t * lv_3d_pick_scene(lv_obj_t * scene, lv_vec3_t origin, lv_vec3_t dir)
{
    if(!scene) return NULL;

    lv_3d_draw_item_t items[LV_3D_MAX_DRAW_ITEMS];
    uint32_t n = lv_3d_scene_collect(scene, items, LV_3D_MAX_DRAW_ITEMS);

    float best_t = FLT_MAX;
    lv_obj_t * best = NULL;
    for(uint32_t i = 0; i < n; i++) {
        float t;
        if(ray_hit_mesh(&items[i], origin, dir, &t) && t >= 0.0f && t < best_t) {
            best_t = t;
            best = items[i].obj;
        }
    }
    return best;
}

#endif /*LV_USE_3D*/
