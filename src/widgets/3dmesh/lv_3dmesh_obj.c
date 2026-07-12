/**
 * @file lv_3dmesh_obj.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_3dmesh_private.h"
#include "../../lvgl_public.h"

#if LV_USE_3DMESH

#include "../../include/lvgl/fs/lv_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/

#define OBJ_MAX_LINE 256
#define OBJ_GROW 64

/**********************
 *  STATIC PROTOTYPES
 **********************/

static bool parse_face_token(const char * tok, int32_t * vi, int32_t * ni);
static bool push_vertex(float ** verts, float ** norms, uint32_t * count, uint32_t * cap,
                        float x, float y, float z, float nx, float ny, float nz);
static bool push_index(uint16_t ** indices, uint32_t * count, uint32_t * cap, uint16_t idx);

/**********************
 *  STATIC VARIABLES
 **********************/

static bool s_obj_loader_ready;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_result_t lv_3dmesh_load_obj(lv_obj_t * obj, const char * path)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return LV_RESULT_INVALID);
    if(path == NULL) return LV_RESULT_INVALID;

    lv_fs_file_t f;
    if(lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) {
        LV_LOG_WARN("OBJ open failed: %s", path);
        return LV_RESULT_INVALID;
    }

    float * src_v = NULL;
    float * src_n = NULL;
    uint32_t src_v_cnt = 0;
    uint32_t src_v_cap = 0;
    uint32_t src_n_cnt = 0;
    uint32_t src_n_cap = 0;

    float * out_v = NULL;
    float * out_n = NULL;
    uint16_t * out_i = NULL;
    uint32_t out_v_cnt = 0;
    uint32_t out_v_cap = 0;
    uint32_t out_i_cnt = 0;
    uint32_t out_i_cap = 0;

    char line[OBJ_MAX_LINE];
    uint32_t line_pos = 0;
    char ch;
    lv_result_t res = LV_RESULT_OK;

    while(res == LV_RESULT_OK && lv_fs_read(&f, &ch, 1, NULL) == LV_FS_RES_OK) {
        if(ch == '\r') continue;
        if(ch != '\n') {
            if(line_pos + 1 < OBJ_MAX_LINE) line[line_pos++] = ch;
            continue;
        }
        line[line_pos] = '\0';
        line_pos = 0;

        if(line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if(sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) {
                if(src_v_cnt >= src_v_cap) {
                    src_v_cap = src_v_cap ? src_v_cap + OBJ_GROW : OBJ_GROW;
                    float * nv = lv_realloc(src_v, src_v_cap * 3 * sizeof(float));
                    if(nv == NULL) { res = LV_RESULT_INVALID; break; }
                    src_v = nv;
                }
                src_v[src_v_cnt * 3 + 0] = x;
                src_v[src_v_cnt * 3 + 1] = y;
                src_v[src_v_cnt * 3 + 2] = z;
                src_v_cnt++;
            }
        }
        else if(line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
            float x, y, z;
            if(sscanf(line + 3, "%f %f %f", &x, &y, &z) == 3) {
                if(src_n_cnt >= src_n_cap) {
                    src_n_cap = src_n_cap ? src_n_cap + OBJ_GROW : OBJ_GROW;
                    float * nn = lv_realloc(src_n, src_n_cap * 3 * sizeof(float));
                    if(nn == NULL) { res = LV_RESULT_INVALID; break; }
                    src_n = nn;
                }
                src_n[src_n_cnt * 3 + 0] = x;
                src_n[src_n_cnt * 3 + 1] = y;
                src_n[src_n_cnt * 3 + 2] = z;
                src_n_cnt++;
            }
        }
        else if(line[0] == 'f' && line[1] == ' ') {
            int32_t face_v[16];
            int32_t face_n[16];
            uint32_t face_cnt = 0;
            const char * p = line + 2;

            while(*p && face_cnt < 16) {
                while(*p == ' ') p++;
                if(*p == '\0') break;
                char tok[64];
                uint32_t ti = 0;
                while(*p && *p != ' ' && ti + 1 < sizeof(tok)) tok[ti++] = *p++;
                tok[ti] = '\0';
                if(!parse_face_token(tok, &face_v[face_cnt], &face_n[face_cnt])) break;
                face_cnt++;
            }

            for(uint32_t t = 1; t + 1 < face_cnt; t++) {
                int32_t ids[3] = { face_v[0], face_v[t], face_v[t + 1] };
                int32_t nids[3] = { face_n[0], face_n[t], face_n[t + 1] };
                for(uint32_t k = 0; k < 3; k++) {
                    if(ids[k] <= 0 || ids[k] > (int32_t)src_v_cnt) { res = LV_RESULT_INVALID; break; }
                    float * sv = &src_v[(ids[k] - 1) * 3];
                    float nx = 0.f, ny = 1.f, nz = 0.f;
                    if(nids[k] > 0 && nids[k] <= (int32_t)src_n_cnt) {
                        float * sn = &src_n[(nids[k] - 1) * 3];
                        nx = sn[0]; ny = sn[1]; nz = sn[2];
                    }
                    if(!push_vertex(&out_v, &out_n, &out_v_cnt, &out_v_cap, sv[0], sv[1], sv[2], nx, ny, nz)) {
                        res = LV_RESULT_INVALID;
                        break;
                    }
                    if(!push_index(&out_i, &out_i_cnt, &out_i_cap, (uint16_t)(out_v_cnt - 1))) {
                        res = LV_RESULT_INVALID;
                        break;
                    }
                }
            }
        }
    }

    lv_fs_close(&f);
    lv_free(src_v);
    lv_free(src_n);

    if(res != LV_RESULT_OK || out_v_cnt == 0 || out_i_cnt == 0) {
        lv_free(out_v);
        lv_free(out_n);
        lv_free(out_i);
        return LV_RESULT_INVALID;
    }

    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    free_geometry(mesh);
    mesh->vertices = out_v;
    mesh->normals = out_n;
    mesh->indices = out_i;
    mesh->vertex_count = out_v_cnt;
    mesh->index_count = out_i_cnt;
    mesh->phong = true;
    mesh->pickable = true;

    if(!s_obj_loader_ready) {
        s_obj_loader_ready = true;
        LV_LOG_INFO("G100 OBJ loader ready (wavefront subset)");
    }

    lv_obj_invalidate(lv_obj_get_parent(obj));
    return LV_RESULT_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool parse_face_token(const char * tok, int32_t * vi, int32_t * ni)
{
    *vi = (int32_t)strtol(tok, NULL, 10);
    *ni = 0;
    const char * slash = strchr(tok, '/');
    if(slash) {
        slash++;
        if(*slash && *slash != '/') slash = strchr(slash, '/');
        if(slash && slash[1]) *ni = (int32_t)strtol(slash + 1, NULL, 10);
    }
    return *vi > 0;
}

static bool push_vertex(float ** verts, float ** norms, uint32_t * count, uint32_t * cap,
                        float x, float y, float z, float nx, float ny, float nz)
{
    if(*count >= *cap) {
        uint32_t new_cap = (*cap == 0) ? OBJ_GROW : (*cap + OBJ_GROW);
        float * nv = lv_realloc(*verts, new_cap * 3 * sizeof(float));
        float * nn = lv_realloc(*norms, new_cap * 3 * sizeof(float));
        if(nv == NULL || nn == NULL) return false;
        *verts = nv;
        *norms = nn;
        *cap = new_cap;
    }
    (*verts)[(*count) * 3 + 0] = x;
    (*verts)[(*count) * 3 + 1] = y;
    (*verts)[(*count) * 3 + 2] = z;
    (*norms)[(*count) * 3 + 0] = nx;
    (*norms)[(*count) * 3 + 1] = ny;
    (*norms)[(*count) * 3 + 2] = nz;
    (*count)++;
    return true;
}

static bool push_index(uint16_t ** indices, uint32_t * count, uint32_t * cap, uint16_t idx)
{
    if(*count >= *cap) {
        uint32_t new_cap = (*cap == 0) ? OBJ_GROW : (*cap + OBJ_GROW);
        uint16_t * ni = lv_realloc(*indices, new_cap * sizeof(uint16_t));
        if(ni == NULL) return false;
        *indices = ni;
        *cap = new_cap;
    }
    (*indices)[(*count)++] = idx;
    return true;
}

#endif /*LV_USE_3DMESH*/
