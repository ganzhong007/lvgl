/**
 * @file lv_3dmesh.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_3dmesh_private.h"
#include "../../lvgl_public.h"

#if LV_USE_3DMESH

#include "../../core/lv_obj_class_private.h"
#include "../../include/lvgl/draw/lv_draw_3d_mesh.h"

#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

/*********************
 *      DEFINES
 *********************/

#define MY_CLASS (&lv_3dmesh_class)

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void lv_3dmesh_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_3dmesh_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void free_geometry(lv_3dmesh_t * mesh);
static bool set_box_geometry(lv_3dmesh_t * mesh, float sx, float sy, float sz);
static bool set_box_phong_geometry(lv_3dmesh_t * mesh, float sx, float sy, float sz);
static void build_model_matrix(const lv_3dmesh_t * mesh, float out[LV_3D_MESH_MODEL_SIZE]);
static void mat4_identity(float m[16]);
static void mat4_translate(float m[16], float tx, float ty, float tz);
static void mat4_scale(float m[16], float sx, float sy, float sz);
static void mat4_rotate_y(float m[16], float radians);
static void mat4_rotate_x(float m[16], float radians);
static void mat4_rotate_z(float m[16], float radians);
static void mat4_mul(float out[16], const float a[16], const float b[16]);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t lv_3dmesh_class = {
    .constructor_cb = lv_3dmesh_constructor,
    .destructor_cb = lv_3dmesh_destructor,
    .width_def = 0,
    .height_def = 0,
    .instance_size = sizeof(lv_3dmesh_t),
    .base_class = &lv_obj_class,
    .name = "3dmesh",
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_3dmesh_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(obj, 1, 1);
    return obj;
}

void lv_3dmesh_set_color(lv_obj_t * obj, lv_color_t color, lv_opa_t opa)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->color = lv_color32_make(color.red, color.green, color.blue, opa);
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dmesh_set_transform(lv_obj_t * obj, float tx, float ty, float tz,
                             float rot_x, float rot_y, float rot_z,
                             float sx, float sy, float sz)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->translation[0] = tx;
    mesh->translation[1] = ty;
    mesh->translation[2] = tz;
    mesh->rotation[0] = rot_x;
    mesh->rotation[1] = rot_y;
    mesh->rotation[2] = rot_z;
    mesh->scale[0] = sx;
    mesh->scale[1] = sy;
    mesh->scale[2] = sz;
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dmesh_set_box(lv_obj_t * obj, float sx, float sy, float sz)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    bool ok = mesh->phong ? set_box_phong_geometry(mesh, sx, sy, sz)
                          : set_box_geometry(mesh, sx, sy, sz);
    if(!ok) return;
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dmesh_set_depth_test(lv_obj_t * obj, bool enable)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    if(enable) mesh->flags |= LV_3D_MESH_FLAG_DEPTH_TEST;
    else mesh->flags &= ~LV_3D_MESH_FLAG_DEPTH_TEST;
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dmesh_set_cull_face(lv_obj_t * obj, bool enable)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    if(enable) mesh->flags |= LV_3D_MESH_FLAG_CULL_FACE;
    else mesh->flags &= ~LV_3D_MESH_FLAG_CULL_FACE;
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dmesh_set_phong(lv_obj_t * obj, bool enable)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->phong = enable;
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dmesh_set_shininess(lv_obj_t * obj, float shininess)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->shininess = shininess;
    lv_obj_invalidate(lv_obj_get_parent(obj));
}

void lv_3dmesh_submit_tree(lv_obj_t * root, lv_layer_t * pass_layer)
{
    if(root == NULL || pass_layer == NULL) return;

    uint32_t i;
    uint32_t cnt = lv_obj_get_child_count(root);
    for(i = 0; i < cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(root, i);
        if(lv_obj_check_type(child, &lv_3dmesh_class)) {
            lv_3dmesh_submit(child, pass_layer);
        }
        lv_3dmesh_submit_tree(child, pass_layer);
    }
}

void lv_3dmesh_submit(lv_obj_t * obj, lv_layer_t * pass_layer)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    if(mesh->vertices == NULL || mesh->indices == NULL || mesh->index_count == 0) return;

    lv_draw_3d_mesh_dsc_t dsc;
    lv_draw_3d_mesh_dsc_init(&dsc);
    dsc.vertices = mesh->vertices;
    dsc.vertex_count = mesh->vertex_count;
    dsc.indices = mesh->indices;
    dsc.index_count = mesh->index_count;
    dsc.color = mesh->color;
    dsc.flags = mesh->flags;
    dsc.shininess = mesh->shininess;
    dsc.ambient = mesh->ambient;
    if(mesh->phong && mesh->normals != NULL) {
        dsc.flags |= LV_3D_MESH_FLAG_PHONG;
        dsc.normals = mesh->normals;
    }
    build_model_matrix(mesh, dsc.model_matrix);
    lv_draw_3d_mesh(pass_layer, &dsc);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_3dmesh_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_3dmesh_t * mesh = (lv_3dmesh_t *)obj;
    mesh->vertices = NULL;
    mesh->normals = NULL;
    mesh->indices = NULL;
    mesh->vertex_count = 0;
    mesh->index_count = 0;
    mesh->color = lv_color32_make(0xFF, 0xFF, 0xFF, LV_OPA_COVER);
    mesh->translation[0] = mesh->translation[1] = mesh->translation[2] = 0.f;
    mesh->rotation[0] = mesh->rotation[1] = mesh->rotation[2] = 0.f;
    mesh->scale[0] = mesh->scale[1] = mesh->scale[2] = 1.f;
    mesh->flags = LV_3D_MESH_FLAG_DEPTH_TEST | LV_3D_MESH_FLAG_CULL_FACE;
    mesh->phong = false;
    mesh->shininess = 32.f;
    mesh->ambient = 0.15f;
}

static void lv_3dmesh_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    free_geometry((lv_3dmesh_t *)obj);
}

static void free_geometry(lv_3dmesh_t * mesh)
{
    if(mesh->vertices) {
        lv_free(mesh->vertices);
        mesh->vertices = NULL;
    }
    if(mesh->normals) {
        lv_free(mesh->normals);
        mesh->normals = NULL;
    }
    if(mesh->indices) {
        lv_free(mesh->indices);
        mesh->indices = NULL;
    }
    mesh->vertex_count = 0;
    mesh->index_count = 0;
}

static bool set_box_geometry(lv_3dmesh_t * mesh, float sx, float sy, float sz)
{
    static const float unit_verts[8 * 3] = {
        -0.5f, -0.5f, -0.5f,
        0.5f, -0.5f, -0.5f,
        0.5f, 0.5f, -0.5f,
        -0.5f, 0.5f, -0.5f,
        -0.5f, -0.5f, 0.5f,
        0.5f, -0.5f, 0.5f,
        0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f,
    };

    static const uint16_t unit_indices[36] = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        2, 6, 7, 2, 7, 3,
        0, 3, 7, 0, 7, 4,
        1, 5, 6, 1, 6, 2,
    };

    free_geometry(mesh);

    mesh->vertex_count = 8;
    mesh->index_count = 36;
    mesh->vertices = lv_malloc(sizeof(float) * mesh->vertex_count * 3);
    mesh->indices = lv_malloc(sizeof(uint16_t) * mesh->index_count);
    if(mesh->vertices == NULL || mesh->indices == NULL) {
        free_geometry(mesh);
        return false;
    }

    for(uint32_t i = 0; i < mesh->vertex_count; i++) {
        mesh->vertices[i * 3 + 0] = unit_verts[i * 3 + 0] * sx;
        mesh->vertices[i * 3 + 1] = unit_verts[i * 3 + 1] * sy;
        mesh->vertices[i * 3 + 2] = unit_verts[i * 3 + 2] * sz;
    }
    lv_memcpy(mesh->indices, unit_indices, sizeof(unit_indices));
    return true;
}

static bool set_box_phong_geometry(lv_3dmesh_t * mesh, float sx, float sy, float sz)
{
    static const float face_data[6][4][6] = {
        /* +X */
        { {0.5f, -0.5f, -0.5f, 1, 0, 0}, {0.5f, 0.5f, -0.5f, 1, 0, 0}, {0.5f, 0.5f, 0.5f, 1, 0, 0}, {0.5f, -0.5f, 0.5f, 1, 0, 0} },
        /* -X */
        { {-0.5f, -0.5f, 0.5f, -1, 0, 0}, {-0.5f, 0.5f, 0.5f, -1, 0, 0}, {-0.5f, 0.5f, -0.5f, -1, 0, 0}, {-0.5f, -0.5f, -0.5f, -1, 0, 0} },
        /* +Y */
        { {-0.5f, 0.5f, -0.5f, 0, 1, 0}, {0.5f, 0.5f, -0.5f, 0, 1, 0}, {0.5f, 0.5f, 0.5f, 0, 1, 0}, {-0.5f, 0.5f, 0.5f, 0, 1, 0} },
        /* -Y */
        { {-0.5f, -0.5f, 0.5f, 0, -1, 0}, {0.5f, -0.5f, 0.5f, 0, -1, 0}, {0.5f, -0.5f, -0.5f, 0, -1, 0}, {-0.5f, -0.5f, -0.5f, 0, -1, 0} },
        /* +Z */
        { {-0.5f, -0.5f, 0.5f, 0, 0, 1}, {0.5f, -0.5f, 0.5f, 0, 0, 1}, {0.5f, 0.5f, 0.5f, 0, 0, 1}, {-0.5f, 0.5f, 0.5f, 0, 0, 1} },
        /* -Z */
        { {0.5f, -0.5f, -0.5f, 0, 0, -1}, {-0.5f, -0.5f, -0.5f, 0, 0, -1}, {-0.5f, 0.5f, -0.5f, 0, 0, -1}, {0.5f, 0.5f, -0.5f, 0, 0, -1} },
    };

    free_geometry(mesh);

    mesh->vertex_count = 24;
    mesh->index_count = 36;
    mesh->vertices = lv_malloc(sizeof(float) * mesh->vertex_count * 3);
    mesh->normals = lv_malloc(sizeof(float) * mesh->vertex_count * 3);
    mesh->indices = lv_malloc(sizeof(uint16_t) * mesh->index_count);
    if(mesh->vertices == NULL || mesh->normals == NULL || mesh->indices == NULL) {
        free_geometry(mesh);
        return false;
    }

    uint32_t v = 0;
    uint32_t i = 0;
    for(uint32_t f = 0; f < 6; f++) {
        for(uint32_t c = 0; c < 4; c++) {
            mesh->vertices[v * 3 + 0] = face_data[f][c][0] * sx;
            mesh->vertices[v * 3 + 1] = face_data[f][c][1] * sy;
            mesh->vertices[v * 3 + 2] = face_data[f][c][2] * sz;
            mesh->normals[v * 3 + 0] = face_data[f][c][3];
            mesh->normals[v * 3 + 1] = face_data[f][c][4];
            mesh->normals[v * 3 + 2] = face_data[f][c][5];
            v++;
        }
        uint16_t base = (uint16_t)(v - 4);
        mesh->indices[i++] = base;
        mesh->indices[i++] = base + 1;
        mesh->indices[i++] = base + 2;
        mesh->indices[i++] = base;
        mesh->indices[i++] = base + 2;
        mesh->indices[i++] = base + 3;
    }

    return true;
}

static void build_model_matrix(const lv_3dmesh_t * mesh, float out[LV_3D_MESH_MODEL_SIZE])
{
    float t[16];
    float rx[16];
    float ry[16];
    float rz[16];
    float r[16];
    float s[16];
    float tmp[16];

    mat4_identity(t);
    mat4_translate(t, mesh->translation[0], mesh->translation[1], mesh->translation[2]);

    mat4_identity(rx);
    mat4_rotate_x(rx, mesh->rotation[0]);
    mat4_identity(ry);
    mat4_rotate_y(ry, mesh->rotation[1]);
    mat4_identity(rz);
    mat4_rotate_z(rz, mesh->rotation[2]);

    mat4_mul(r, ry, rx);
    mat4_mul(tmp, r, rz);

    mat4_identity(s);
    mat4_scale(s, mesh->scale[0], mesh->scale[1], mesh->scale[2]);

    mat4_mul(tmp, t, tmp);
    mat4_mul(out, tmp, s);
}

static void mat4_identity(float m[16])
{
    lv_memzero(m, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

static void mat4_translate(float m[16], float tx, float ty, float tz)
{
    mat4_identity(m);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}

static void mat4_scale(float m[16], float sx, float sy, float sz)
{
    mat4_identity(m);
    m[0] = sx;
    m[5] = sy;
    m[10] = sz;
}

static void mat4_rotate_y(float m[16], float radians)
{
    mat4_identity(m);
    float c = cosf(radians);
    float s = sinf(radians);
    m[0] = c;
    m[2] = s;
    m[8] = -s;
    m[10] = c;
}

static void mat4_rotate_x(float m[16], float radians)
{
    mat4_identity(m);
    float c = cosf(radians);
    float s = sinf(radians);
    m[5] = c;
    m[6] = -s;
    m[9] = s;
    m[10] = c;
}

static void mat4_rotate_z(float m[16], float radians)
{
    mat4_identity(m);
    float c = cosf(radians);
    float s = sinf(radians);
    m[0] = c;
    m[1] = -s;
    m[4] = s;
    m[5] = c;
}

static void mat4_mul(float out[16], const float a[16], const float b[16])
{
    float r[16];
    for(int col = 0; col < 4; col++) {
        for(int row = 0; row < 4; row++) {
            r[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0]
                               + a[1 * 4 + row] * b[col * 4 + 1]
                               + a[2 * 4 + row] * b[col * 4 + 2]
                               + a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    lv_memcpy(out, r, sizeof(r));
}

#endif /*LV_USE_3DMESH*/
