/**
 * @file evgpu_evgr.c
 * @brief EVGPU vector renderer core (EVGR; fork of NanoVG for DrawUnitEVGPU).
 *
 * Kept in libs/evgpu so draw/evgpu does not depend on libs/nanovg.
 * API remains evgr* for minimal churn in draw/evgpu adapters.
 */

//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

#include <math.h>

#include "evgpu_evgr.h"

#ifdef _MSC_VER
    #pragma warning(disable: 4100)  // unreferenced formal parameter
    #pragma warning(disable: 4127)  // conditional expression is constant
    #pragma warning(disable: 4204)  // nonstandard extension used : non-constant aggregate initializer
    #pragma warning(disable: 4706)  // assignment within conditional expression
#endif

#define EVGR_INIT_FONTIMAGE_SIZE  512
#define EVGR_MAX_FONTIMAGE_SIZE   2048
#define EVGR_MAX_FONTIMAGES       4

#define EVGR_INIT_COMMANDS_SIZE 256
#define EVGR_INIT_POINTS_SIZE 128
#define EVGR_INIT_PATHS_SIZE 16
#define EVGR_INIT_VERTS_SIZE 256

#ifndef EVGR_MAX_STATES
    #define EVGR_MAX_STATES 32
#endif

#define EVGR_KAPPA90 0.5522847493f   // Length proportional to radius of a cubic bezier handle for 90deg arcs.

#define EVGR_COUNTOF(arr) (sizeof(arr) / sizeof(0[arr]))

/**
 * This value determines the maximum permissible pixel error when a Bézier curve is subdivided into line segments;
 * a smaller value results in smoother lines but also more vertices.
 */
#define EVGR_TESS_TOL_FACTOR 0.5f

enum EVGRcommands {
    EVGR_MOVETO = 0,
    EVGR_LINETO = 1,
    EVGR_BEZIERTO = 2,
    EVGR_CLOSE = 3,
    EVGR_WINDING = 4,
};

enum EVGRpointFlags {
    EVGR_PT_CORNER = 0x01,
    EVGR_PT_LEFT = 0x02,
    EVGR_PT_BEVEL = 0x04,
    EVGR_PR_INNERBEVEL = 0x08,
};

struct EVGRstate {
    EVGRcompositeOperationState compositeOperation;
    int shapeAntiAlias;
    EVGRpaint fill;
    EVGRpaint stroke;
    float strokeWidth;
    float miterLimit;
    int lineJoin;
    int lineCap;
    float alpha;
    float xform[6];
    EVGRscissor scissor;
    float fontSize;
    float letterSpacing;
    float lineHeight;
    float fontBlur;
    int textAlign;
    int fontId;
};
typedef struct EVGRstate EVGRstate;

struct EVGRpoint {
    float x, y;
    float dx, dy;
    float len;
    float dmx, dmy;
    unsigned char flags;
};
typedef struct EVGRpoint EVGRpoint;

struct EVGRpathCache {
    EVGRpoint * points;
    int npoints;
    int cpoints;
    EVGRpath * paths;
    int npaths;
    int cpaths;
    EVGRvertex * verts;
    int nverts;
    int cverts;
    float bounds[4];
};
typedef struct EVGRpathCache EVGRpathCache;

struct EVGRcontext {
    EVGRparams params;
    float * commands;
    int ccommands;
    int ncommands;
    float commandx, commandy;
    EVGRstate states[EVGR_MAX_STATES];
    int nstates;
    EVGRpathCache * cache;
    float tessTol;
    float distTol;
    float fringeWidth;
    float devicePxRatio;
    struct FONScontext * fs;
    int fontImages[EVGR_MAX_FONTIMAGES];
    int fontImageIdx;
    int drawCallCount;
    int fillTriCount;
    int strokeTriCount;
    int textTriCount;
};

static float evgr__sqrtf(float a)
{
    return sqrtf(a);
}
static float evgr__modf(float a, float b)
{
    return fmodf(a, b);
}
static float evgr__sinf(float a)
{
    return sinf(a);
}
static float evgr__cosf(float a)
{
    return cosf(a);
}
static float evgr__tanf(float a)
{
    return tanf(a);
}
static float evgr__atan2f(float a, float b)
{
    return atan2f(a, b);
}
static float evgr__acosf(float a)
{
    return acosf(a);
}

static int evgr__mini(int a, int b)
{
    return a < b ? a : b;
}
static int evgr__maxi(int a, int b)
{
    return a > b ? a : b;
}
static int evgr__clampi(int a, int mn, int mx)
{
    return a < mn ? mn : (a > mx ? mx : a);
}
static float evgr__minf(float a, float b)
{
    return a < b ? a : b;
}
static float evgr__maxf(float a, float b)
{
    return a > b ? a : b;
}
static float evgr__absf(float a)
{
    return a >= 0.0f ? a : -a;
}
static float evgr__signf(float a)
{
    return a >= 0.0f ? 1.0f : -1.0f;
}
static float evgr__clampf(float a, float mn, float mx)
{
    return a < mn ? mn : (a > mx ? mx : a);
}
static float evgr__cross(float dx0, float dy0, float dx1, float dy1)
{
    return dx1 * dy0 - dx0 * dy1;
}

static float evgr__normalize(float * x, float * y)
{
    float d = evgr__sqrtf((*x) * (*x) + (*y) * (*y));
    if(d > 1e-6f) {
        float id = 1.0f / d;
        *x *= id;
        *y *= id;
    }
    return d;
}


static void evgr__deletePathCache(EVGRpathCache * c)
{
    if(c == NULL) return;
    if(c->points != NULL) lv_free(c->points);
    if(c->paths != NULL) lv_free(c->paths);
    if(c->verts != NULL) lv_free(c->verts);
    lv_free(c);
}

static EVGRpathCache * evgr__allocPathCache(void)
{
    EVGRpathCache * c = (EVGRpathCache *)lv_malloc(sizeof(EVGRpathCache));
    if(c == NULL) goto error;
    lv_memzero(c, sizeof(EVGRpathCache));

    c->points = (EVGRpoint *)lv_malloc(sizeof(EVGRpoint) * EVGR_INIT_POINTS_SIZE);
    if(!c->points) goto error;
    c->npoints = 0;
    c->cpoints = EVGR_INIT_POINTS_SIZE;

    c->paths = (EVGRpath *)lv_malloc(sizeof(EVGRpath) * EVGR_INIT_PATHS_SIZE);
    if(!c->paths) goto error;
    c->npaths = 0;
    c->cpaths = EVGR_INIT_PATHS_SIZE;

    c->verts = (EVGRvertex *)lv_malloc(sizeof(EVGRvertex) * EVGR_INIT_VERTS_SIZE);
    if(!c->verts) goto error;
    c->nverts = 0;
    c->cverts = EVGR_INIT_VERTS_SIZE;

    return c;
error:
    evgr__deletePathCache(c);
    return NULL;
}

static void evgr__setDevicePixelRatio(EVGRcontext * ctx, float ratio)
{
    ctx->tessTol = EVGR_TESS_TOL_FACTOR / ratio;
    ctx->distTol = 0.01f / ratio;
    ctx->fringeWidth = 1.0f / ratio;
    ctx->devicePxRatio = ratio;
}

static EVGRcompositeOperationState evgr__compositeOperationState(int op)
{
    int sfactor, dfactor;

    if(op == EVGR_SOURCE_OVER) {
        sfactor = EVGR_ONE;
        dfactor = EVGR_ONE_MINUS_SRC_ALPHA;
    }
    else if(op == EVGR_SOURCE_IN) {
        sfactor = EVGR_DST_ALPHA;
        dfactor = EVGR_ZERO;
    }
    else if(op == EVGR_SOURCE_OUT) {
        sfactor = EVGR_ONE_MINUS_DST_ALPHA;
        dfactor = EVGR_ZERO;
    }
    else if(op == EVGR_ATOP) {
        sfactor = EVGR_DST_ALPHA;
        dfactor = EVGR_ONE_MINUS_SRC_ALPHA;
    }
    else if(op == EVGR_DESTINATION_OVER) {
        sfactor = EVGR_ONE_MINUS_DST_ALPHA;
        dfactor = EVGR_ONE;
    }
    else if(op == EVGR_DESTINATION_IN) {
        sfactor = EVGR_ZERO;
        dfactor = EVGR_SRC_ALPHA;
    }
    else if(op == EVGR_DESTINATION_OUT) {
        sfactor = EVGR_ZERO;
        dfactor = EVGR_ONE_MINUS_SRC_ALPHA;
    }
    else if(op == EVGR_DESTINATION_ATOP) {
        sfactor = EVGR_ONE_MINUS_DST_ALPHA;
        dfactor = EVGR_SRC_ALPHA;
    }
    else if(op == EVGR_LIGHTER) {
        sfactor = EVGR_ONE;
        dfactor = EVGR_ONE;
    }
    else if(op == EVGR_COPY) {
        sfactor = EVGR_ONE;
        dfactor = EVGR_ZERO;
    }
    else if(op == EVGR_XOR) {
        sfactor = EVGR_ONE_MINUS_DST_ALPHA;
        dfactor = EVGR_ONE_MINUS_SRC_ALPHA;
    }
    else {
        sfactor = EVGR_ONE;
        dfactor = EVGR_ZERO;
    }

    EVGRcompositeOperationState state;
    state.srcRGB = sfactor;
    state.dstRGB = dfactor;
    state.srcAlpha = sfactor;
    state.dstAlpha = dfactor;
    return state;
}

static EVGRstate * evgr__getState(EVGRcontext * ctx)
{
    return &ctx->states[ctx->nstates - 1];
}

EVGRcontext * evgrCreateInternal(EVGRparams * params)
{
    EVGRcontext * ctx = (EVGRcontext *)lv_malloc(sizeof(EVGRcontext));
    int i;
    if(ctx == NULL) goto error;
    lv_memzero(ctx, sizeof(EVGRcontext));

    ctx->params = *params;
    for(i = 0; i < EVGR_MAX_FONTIMAGES; i++)
        ctx->fontImages[i] = 0;

    ctx->commands = (float *)lv_malloc(sizeof(float) * EVGR_INIT_COMMANDS_SIZE);
    if(!ctx->commands) goto error;
    ctx->ncommands = 0;
    ctx->ccommands = EVGR_INIT_COMMANDS_SIZE;

    ctx->cache = evgr__allocPathCache();
    if(ctx->cache == NULL) goto error;

    evgrSave(ctx);
    evgrReset(ctx);

    evgr__setDevicePixelRatio(ctx, 1.0f);

    if(ctx->params.renderCreate(ctx->params.userPtr) == 0) goto error;

    return ctx;

error:
    evgrDeleteInternal(ctx);
    return 0;
}

EVGRparams * evgrInternalParams(EVGRcontext * ctx)
{
    return &ctx->params;
}

void evgrDeleteInternal(EVGRcontext * ctx)
{
    int i;
    if(ctx == NULL) return;
    if(ctx->commands != NULL) lv_free(ctx->commands);
    if(ctx->cache != NULL) evgr__deletePathCache(ctx->cache);

    for(i = 0; i < EVGR_MAX_FONTIMAGES; i++) {
        if(ctx->fontImages[i] != 0) {
            evgrDeleteImage(ctx, ctx->fontImages[i]);
            ctx->fontImages[i] = 0;
        }
    }

    if(ctx->params.renderDelete != NULL)
        ctx->params.renderDelete(ctx->params.userPtr);

    lv_free(ctx);
}

void evgrBeginFrame(EVGRcontext * ctx, float windowWidth, float windowHeight, float devicePixelRatio)
{
    /*  printf("Tris: draws:%d  fill:%d  stroke:%d  text:%d  TOT:%d\n",
            ctx->drawCallCount, ctx->fillTriCount, ctx->strokeTriCount, ctx->textTriCount,
            ctx->fillTriCount+ctx->strokeTriCount+ctx->textTriCount);*/

    ctx->nstates = 0;
    evgrSave(ctx);
    evgrReset(ctx);

    evgr__setDevicePixelRatio(ctx, devicePixelRatio);

    ctx->params.renderViewport(ctx->params.userPtr, windowWidth, windowHeight, devicePixelRatio);

    ctx->drawCallCount = 0;
    ctx->fillTriCount = 0;
    ctx->strokeTriCount = 0;
    ctx->textTriCount = 0;
}

void evgrCancelFrame(EVGRcontext * ctx)
{
    ctx->params.renderCancel(ctx->params.userPtr);
}

void evgrEndFrame(EVGRcontext * ctx)
{
    ctx->params.renderFlush(ctx->params.userPtr);
    if(ctx->fontImageIdx != 0) {
        int fontImage = ctx->fontImages[ctx->fontImageIdx];
        ctx->fontImages[ctx->fontImageIdx] = 0;
        int i, j, iw, ih;
        // delete images that smaller than current one
        if(fontImage == 0)
            return;
        evgrImageSize(ctx, fontImage, &iw, &ih);
        for(i = j = 0; i < ctx->fontImageIdx; i++) {
            if(ctx->fontImages[i] != 0) {
                int nw, nh;
                int image = ctx->fontImages[i];
                ctx->fontImages[i] = 0;
                evgrImageSize(ctx, image, &nw, &nh);
                if(nw < iw || nh < ih)
                    evgrDeleteImage(ctx, image);
                else
                    ctx->fontImages[j++] = image;
            }
        }
        // make current font image to first
        ctx->fontImages[j] = ctx->fontImages[0];
        ctx->fontImages[0] = fontImage;
        ctx->fontImageIdx = 0;
    }
}

EVGRcolor evgrRGB(unsigned char r, unsigned char g, unsigned char b)
{
    return evgrRGBA(r, g, b, 255);
}

EVGRcolor evgrRGBf(float r, float g, float b)
{
    return evgrRGBAf(r, g, b, 1.0f);
}

EVGRcolor evgrRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    EVGRcolor color;
    // Use longer initialization to suppress warning.
    color.ch.r = r / 255.0f;
    color.ch.g = g / 255.0f;
    color.ch.b = b / 255.0f;
    color.ch.a = a / 255.0f;
    return color;
}

EVGRcolor evgrRGBAf(float r, float g, float b, float a)
{
    EVGRcolor color;
    // Use longer initialization to suppress warning.
    color.ch.r = r;
    color.ch.g = g;
    color.ch.b = b;
    color.ch.a = a;
    return color;
}

EVGRcolor evgrTransRGBA(EVGRcolor c, unsigned char a)
{
    c.ch.a = a / 255.0f;
    return c;
}

EVGRcolor evgrTransRGBAf(EVGRcolor c, float a)
{
    c.ch.a = a;
    return c;
}

EVGRcolor evgrLerpRGBA(EVGRcolor c0, EVGRcolor c1, float u)
{
    int i;
    float oneminu;
    EVGRcolor cint = { 0 };

    u = evgr__clampf(u, 0.0f, 1.0f);
    oneminu = 1.0f - u;
    for(i = 0; i < 4; i++) {
        cint.rgba[i] = c0.rgba[i] * oneminu + c1.rgba[i] * u;
    }

    return cint;
}

EVGRcolor evgrHSL(float h, float s, float l)
{
    return evgrHSLA(h, s, l, 255);
}

static float evgr__hue(float h, float m1, float m2)
{
    if(h < 0) h += 1;
    if(h > 1) h -= 1;
    if(h < 1.0f / 6.0f)
        return m1 + (m2 - m1) * h * 6.0f;
    else if(h < 3.0f / 6.0f)
        return m2;
    else if(h < 4.0f / 6.0f)
        return m1 + (m2 - m1) * (2.0f / 3.0f - h) * 6.0f;
    return m1;
}

EVGRcolor evgrHSLA(float h, float s, float l, unsigned char a)
{
    float m1, m2;
    EVGRcolor col;
    h = evgr__modf(h, 1.0f);
    if(h < 0.0f) h += 1.0f;
    s = evgr__clampf(s, 0.0f, 1.0f);
    l = evgr__clampf(l, 0.0f, 1.0f);
    m2 = l <= 0.5f ? (l * (1 + s)) : (l + s - l * s);
    m1 = 2 * l - m2;
    col.ch.r = evgr__clampf(evgr__hue(h + 1.0f / 3.0f, m1, m2), 0.0f, 1.0f);
    col.ch.g = evgr__clampf(evgr__hue(h, m1, m2), 0.0f, 1.0f);
    col.ch.b = evgr__clampf(evgr__hue(h - 1.0f / 3.0f, m1, m2), 0.0f, 1.0f);
    col.ch.a = a / 255.0f;
    return col;
}

void evgrTransformIdentity(float * t)
{
    t[0] = 1.0f;
    t[1] = 0.0f;
    t[2] = 0.0f;
    t[3] = 1.0f;
    t[4] = 0.0f;
    t[5] = 0.0f;
}

void evgrTransformTranslate(float * t, float tx, float ty)
{
    t[0] = 1.0f;
    t[1] = 0.0f;
    t[2] = 0.0f;
    t[3] = 1.0f;
    t[4] = tx;
    t[5] = ty;
}

void evgrTransformScale(float * t, float sx, float sy)
{
    t[0] = sx;
    t[1] = 0.0f;
    t[2] = 0.0f;
    t[3] = sy;
    t[4] = 0.0f;
    t[5] = 0.0f;
}

void evgrTransformRotate(float * t, float a)
{
    float cs = evgr__cosf(a), sn = evgr__sinf(a);
    t[0] = cs;
    t[1] = sn;
    t[2] = -sn;
    t[3] = cs;
    t[4] = 0.0f;
    t[5] = 0.0f;
}

void evgrTransformSkewX(float * t, float a)
{
    t[0] = 1.0f;
    t[1] = 0.0f;
    t[2] = evgr__tanf(a);
    t[3] = 1.0f;
    t[4] = 0.0f;
    t[5] = 0.0f;
}

void evgrTransformSkewY(float * t, float a)
{
    t[0] = 1.0f;
    t[1] = evgr__tanf(a);
    t[2] = 0.0f;
    t[3] = 1.0f;
    t[4] = 0.0f;
    t[5] = 0.0f;
}

void evgrTransformMultiply(float * t, const float * s)
{
    float t0 = t[0] * s[0] + t[1] * s[2];
    float t2 = t[2] * s[0] + t[3] * s[2];
    float t4 = t[4] * s[0] + t[5] * s[2] + s[4];
    t[1] = t[0] * s[1] + t[1] * s[3];
    t[3] = t[2] * s[1] + t[3] * s[3];
    t[5] = t[4] * s[1] + t[5] * s[3] + s[5];
    t[0] = t0;
    t[2] = t2;
    t[4] = t4;
}

void evgrTransformPremultiply(float * t, const float * s)
{
    float s2[6];
    lv_memcpy(s2, s, sizeof(float) * 6);
    evgrTransformMultiply(s2, t);
    lv_memcpy(t, s2, sizeof(float) * 6);
}

int evgrTransformInverse(float * inv, const float * t)
{
    double invdet, det = (double)t[0] * t[3] - (double)t[2] * t[1];
    if(det > -1e-6 && det < 1e-6) {
        evgrTransformIdentity(inv);
        return 0;
    }
    invdet = 1.0 / det;
    inv[0] = (float)(t[3] * invdet);
    inv[2] = (float)(-t[2] * invdet);
    inv[4] = (float)(((double)t[2] * t[5] - (double)t[3] * t[4]) * invdet);
    inv[1] = (float)(-t[1] * invdet);
    inv[3] = (float)(t[0] * invdet);
    inv[5] = (float)(((double)t[1] * t[4] - (double)t[0] * t[5]) * invdet);
    return 1;
}

void evgrTransformPoint(float * dx, float * dy, const float * t, float sx, float sy)
{
    *dx = sx * t[0] + sy * t[2] + t[4];
    *dy = sx * t[1] + sy * t[3] + t[5];
}

float evgrDegToRad(float deg)
{
    return deg / 180.0f * EVGR_PI;
}

float evgrRadToDeg(float rad)
{
    return rad / EVGR_PI * 180.0f;
}

static void evgr__setPaintColor(EVGRpaint * p, EVGRcolor color)
{
    lv_memzero(p, sizeof(*p));
    evgrTransformIdentity(p->xform);
    p->radius = 0.0f;
    p->feather = 1.0f;
    p->innerColor = color;
    p->outerColor = color;
}


// State handling
void evgrSave(EVGRcontext * ctx)
{
    if(ctx->nstates >= EVGR_MAX_STATES)
        return;
    if(ctx->nstates > 0)
        lv_memcpy(&ctx->states[ctx->nstates], &ctx->states[ctx->nstates - 1], sizeof(EVGRstate));
    ctx->nstates++;
}

void evgrRestore(EVGRcontext * ctx)
{
    if(ctx->nstates <= 1)
        return;
    ctx->nstates--;
}

void evgrReset(EVGRcontext * ctx)
{
    EVGRstate * state = evgr__getState(ctx);
    lv_memzero(state, sizeof(*state));

    evgr__setPaintColor(&state->fill, evgrRGBA(255, 255, 255, 255));
    evgr__setPaintColor(&state->stroke, evgrRGBA(0, 0, 0, 255));
    state->compositeOperation = evgr__compositeOperationState(EVGR_SOURCE_OVER);
    state->shapeAntiAlias = 1;
    state->strokeWidth = 1.0f;
    state->miterLimit = 10.0f;
    state->lineCap = EVGR_BUTT;
    state->lineJoin = EVGR_MITER;
    state->alpha = 1.0f;
    evgrTransformIdentity(state->xform);

    state->scissor.extent[0] = -1.0f;
    state->scissor.extent[1] = -1.0f;

    state->fontSize = 16.0f;
    state->letterSpacing = 0.0f;
    state->lineHeight = 1.0f;
    state->fontBlur = 0.0f;
    state->textAlign = EVGR_ALIGN_LEFT | EVGR_ALIGN_BASELINE;
    state->fontId = 0;
}

// State setting
void evgrShapeAntiAlias(EVGRcontext * ctx, int enabled)
{
    EVGRstate * state = evgr__getState(ctx);
    state->shapeAntiAlias = enabled;
}

void evgrStrokeWidth(EVGRcontext * ctx, float width)
{
    EVGRstate * state = evgr__getState(ctx);
    state->strokeWidth = width;
}

void evgrMiterLimit(EVGRcontext * ctx, float limit)
{
    EVGRstate * state = evgr__getState(ctx);
    state->miterLimit = limit;
}

void evgrLineCap(EVGRcontext * ctx, int cap)
{
    EVGRstate * state = evgr__getState(ctx);
    state->lineCap = cap;
}

void evgrLineJoin(EVGRcontext * ctx, int join)
{
    EVGRstate * state = evgr__getState(ctx);
    state->lineJoin = join;
}

void evgrGlobalAlpha(EVGRcontext * ctx, float alpha)
{
    EVGRstate * state = evgr__getState(ctx);
    state->alpha = alpha;
}

void evgrTransform(EVGRcontext * ctx, float a, float b, float c, float d, float e, float f)
{
    EVGRstate * state = evgr__getState(ctx);
    float t[6] = { a, b, c, d, e, f };
    evgrTransformPremultiply(state->xform, t);
}

void evgrResetTransform(EVGRcontext * ctx)
{
    EVGRstate * state = evgr__getState(ctx);
    evgrTransformIdentity(state->xform);
}

void evgrTranslate(EVGRcontext * ctx, float x, float y)
{
    EVGRstate * state = evgr__getState(ctx);
    float t[6];
    evgrTransformTranslate(t, x, y);
    evgrTransformPremultiply(state->xform, t);
}

void evgrRotate(EVGRcontext * ctx, float angle)
{
    EVGRstate * state = evgr__getState(ctx);
    float t[6];
    evgrTransformRotate(t, angle);
    evgrTransformPremultiply(state->xform, t);
}

void evgrSkewX(EVGRcontext * ctx, float angle)
{
    EVGRstate * state = evgr__getState(ctx);
    float t[6];
    evgrTransformSkewX(t, angle);
    evgrTransformPremultiply(state->xform, t);
}

void evgrSkewY(EVGRcontext * ctx, float angle)
{
    EVGRstate * state = evgr__getState(ctx);
    float t[6];
    evgrTransformSkewY(t, angle);
    evgrTransformPremultiply(state->xform, t);
}

void evgrScale(EVGRcontext * ctx, float x, float y)
{
    EVGRstate * state = evgr__getState(ctx);
    float t[6];
    evgrTransformScale(t, x, y);
    evgrTransformPremultiply(state->xform, t);
}

void evgrCurrentTransform(EVGRcontext * ctx, float * xform)
{
    EVGRstate * state = evgr__getState(ctx);
    if(xform == NULL) return;
    lv_memcpy(xform, state->xform, sizeof(float) * 6);
}

void evgrStrokeColor(EVGRcontext * ctx, EVGRcolor color)
{
    EVGRstate * state = evgr__getState(ctx);
    evgr__setPaintColor(&state->stroke, color);
}

void evgrStrokePaint(EVGRcontext * ctx, EVGRpaint paint)
{
    EVGRstate * state = evgr__getState(ctx);
    state->stroke = paint;
    evgrTransformMultiply(state->stroke.xform, state->xform);
}

void evgrFillColor(EVGRcontext * ctx, EVGRcolor color)
{
    EVGRstate * state = evgr__getState(ctx);
    evgr__setPaintColor(&state->fill, color);
}

void evgrFillPaint(EVGRcontext * ctx, EVGRpaint paint)
{
    EVGRstate * state = evgr__getState(ctx);
    state->fill = paint;
    evgrTransformMultiply(state->fill.xform, state->xform);
}

int evgrCreateImage(EVGRcontext * ctx, int w, int h, int imageFlags, int format, const unsigned char * data)
{
    return ctx->params.renderCreateTexture(ctx->params.userPtr, format, w, h, imageFlags, data);
}

void evgrUpdateImage(EVGRcontext * ctx, int image, const unsigned char * data)
{
    int w, h;
    ctx->params.renderGetTextureSize(ctx->params.userPtr, image, &w, &h);
    ctx->params.renderUpdateTexture(ctx->params.userPtr, image, 0, 0, w, h, data);
}

void evgrImageSize(EVGRcontext * ctx, int image, int * w, int * h)
{
    ctx->params.renderGetTextureSize(ctx->params.userPtr, image, w, h);
}

void evgrDeleteImage(EVGRcontext * ctx, int image)
{
    ctx->params.renderDeleteTexture(ctx->params.userPtr, image);
}

EVGRpaint evgrLinearGradient(EVGRcontext * ctx,
                           float sx, float sy, float ex, float ey,
                           EVGRcolor icol, EVGRcolor ocol)
{
    EVGRpaint p;
    float dx, dy, d;
    const float large = 1e5;
    EVGR_NOTUSED(ctx);
    lv_memzero(&p, sizeof(p));

    // Calculate transform aligned to the line
    dx = ex - sx;
    dy = ey - sy;
    d = sqrtf(dx * dx + dy * dy);
    if(d > 0.0001f) {
        dx /= d;
        dy /= d;
    }
    else {
        dx = 0;
        dy = 1;
    }

    p.xform[0] = dy;
    p.xform[1] = -dx;
    p.xform[2] = dx;
    p.xform[3] = dy;
    p.xform[4] = sx - dx * large;
    p.xform[5] = sy - dy * large;

    p.extent[0] = large;
    p.extent[1] = large + d * 0.5f;

    p.radius = 0.0f;

    p.feather = evgr__maxf(1.0f, d);

    p.innerColor = icol;
    p.outerColor = ocol;

    return p;
}

EVGRpaint evgrRadialGradient(EVGRcontext * ctx,
                           float cx, float cy, float inr, float outr,
                           EVGRcolor icol, EVGRcolor ocol)
{
    EVGRpaint p;
    float r = (inr + outr) * 0.5f;
    float f = (outr - inr);
    EVGR_NOTUSED(ctx);
    lv_memzero(&p, sizeof(p));

    evgrTransformIdentity(p.xform);
    p.xform[4] = cx;
    p.xform[5] = cy;

    p.extent[0] = r;
    p.extent[1] = r;

    p.radius = r;

    p.feather = evgr__maxf(1.0f, f);

    p.innerColor = icol;
    p.outerColor = ocol;

    return p;
}

EVGRpaint evgrBoxGradient(EVGRcontext * ctx,
                        float x, float y, float w, float h, float r, float f,
                        EVGRcolor icol, EVGRcolor ocol)
{
    EVGRpaint p;
    EVGR_NOTUSED(ctx);
    lv_memzero(&p, sizeof(p));

    evgrTransformIdentity(p.xform);
    p.xform[4] = x + w * 0.5f;
    p.xform[5] = y + h * 0.5f;

    p.extent[0] = w * 0.5f;
    p.extent[1] = h * 0.5f;

    p.radius = r;

    p.feather = evgr__maxf(1.0f, f);

    p.innerColor = icol;
    p.outerColor = ocol;

    return p;
}


EVGRpaint evgrImagePattern(EVGRcontext * ctx,
                         float cx, float cy, float w, float h, float angle,
                         int image, float alpha)
{
    EVGRpaint p;
    EVGR_NOTUSED(ctx);
    lv_memzero(&p, sizeof(p));

    evgrTransformRotate(p.xform, angle);
    p.xform[4] = cx;
    p.xform[5] = cy;

    p.extent[0] = w;
    p.extent[1] = h;

    p.image = image;

    p.innerColor = p.outerColor = evgrRGBAf(1, 1, 1, alpha);

    return p;
}

// Scissoring
void evgrScissor(EVGRcontext * ctx, float x, float y, float w, float h)
{
    EVGRstate * state = evgr__getState(ctx);

    w = evgr__maxf(0.0f, w);
    h = evgr__maxf(0.0f, h);

    evgrTransformIdentity(state->scissor.xform);
    state->scissor.xform[4] = x + w * 0.5f;
    state->scissor.xform[5] = y + h * 0.5f;
    evgrTransformMultiply(state->scissor.xform, state->xform);

    state->scissor.extent[0] = w * 0.5f;
    state->scissor.extent[1] = h * 0.5f;
}

static void evgr__isectRects(float * dst,
                            float ax, float ay, float aw, float ah,
                            float bx, float by, float bw, float bh)
{
    float minx = evgr__maxf(ax, bx);
    float miny = evgr__maxf(ay, by);
    float maxx = evgr__minf(ax + aw, bx + bw);
    float maxy = evgr__minf(ay + ah, by + bh);
    dst[0] = minx;
    dst[1] = miny;
    dst[2] = evgr__maxf(0.0f, maxx - minx);
    dst[3] = evgr__maxf(0.0f, maxy - miny);
}

void evgrIntersectScissor(EVGRcontext * ctx, float x, float y, float w, float h)
{
    EVGRstate * state = evgr__getState(ctx);
    float pxform[6], invxorm[6];
    float rect[4];
    float ex, ey, tex, tey;

    // If no previous scissor has been set, set the scissor as current scissor.
    if(state->scissor.extent[0] < 0) {
        evgrScissor(ctx, x, y, w, h);
        return;
    }

    // Transform the current scissor rect into current transform space.
    // If there is difference in rotation, this will be approximation.
    lv_memcpy(pxform, state->scissor.xform, sizeof(float) * 6);
    ex = state->scissor.extent[0];
    ey = state->scissor.extent[1];
    evgrTransformInverse(invxorm, state->xform);
    evgrTransformMultiply(pxform, invxorm);
    tex = ex * evgr__absf(pxform[0]) + ey * evgr__absf(pxform[2]);
    tey = ex * evgr__absf(pxform[1]) + ey * evgr__absf(pxform[3]);

    // Intersect rects.
    evgr__isectRects(rect, pxform[4] - tex, pxform[5] - tey, tex * 2, tey * 2, x, y, w, h);

    evgrScissor(ctx, rect[0], rect[1], rect[2], rect[3]);
}

void evgrResetScissor(EVGRcontext * ctx)
{
    EVGRstate * state = evgr__getState(ctx);
    lv_memzero(state->scissor.xform, sizeof(state->scissor.xform));
    state->scissor.extent[0] = -1.0f;
    state->scissor.extent[1] = -1.0f;
}

// Global composite operation.
void evgrGlobalCompositeOperation(EVGRcontext * ctx, int op)
{
    EVGRstate * state = evgr__getState(ctx);
    state->compositeOperation = evgr__compositeOperationState(op);
}

void evgrGlobalCompositeBlendFunc(EVGRcontext * ctx, int sfactor, int dfactor)
{
    evgrGlobalCompositeBlendFuncSeparate(ctx, sfactor, dfactor, sfactor, dfactor);
}

void evgrGlobalCompositeBlendFuncSeparate(EVGRcontext * ctx, int srcRGB, int dstRGB, int srcAlpha, int dstAlpha)
{
    EVGRcompositeOperationState op;
    op.srcRGB = srcRGB;
    op.dstRGB = dstRGB;
    op.srcAlpha = srcAlpha;
    op.dstAlpha = dstAlpha;

    EVGRstate * state = evgr__getState(ctx);
    state->compositeOperation = op;
}

static int evgr__ptEquals(float x1, float y1, float x2, float y2, float tol)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy < tol * tol;
}

static float evgr__distPtSeg(float x, float y, float px, float py, float qx, float qy)
{
    float pqx, pqy, dx, dy, d, t;
    pqx = qx - px;
    pqy = qy - py;
    dx = x - px;
    dy = y - py;
    d = pqx * pqx + pqy * pqy;
    t = pqx * dx + pqy * dy;
    if(d > 0) t /= d;
    if(t < 0) t = 0;
    else if(t > 1) t = 1;
    dx = px + t * pqx - x;
    dy = py + t * pqy - y;
    return dx * dx + dy * dy;
}

static void evgr__appendCommands(EVGRcontext * ctx, float * vals, int nvals)
{
    EVGRstate * state = evgr__getState(ctx);
    int i;

    if(ctx->ncommands + nvals > ctx->ccommands) {
        float * commands;
        int ccommands = ctx->ncommands + nvals + ctx->ccommands / 2;
        commands = (float *)lv_realloc(ctx->commands, sizeof(float) * ccommands);
        if(commands == NULL) return;
        ctx->commands = commands;
        ctx->ccommands = ccommands;
    }

    if((int)vals[0] != EVGR_CLOSE && (int)vals[0] != EVGR_WINDING) {
        ctx->commandx = vals[nvals - 2];
        ctx->commandy = vals[nvals - 1];
    }

    // transform commands
    i = 0;
    while(i < nvals) {
        int cmd = (int)vals[i];
        switch(cmd) {
            case EVGR_MOVETO:
                evgrTransformPoint(&vals[i + 1], &vals[i + 2], state->xform, vals[i + 1], vals[i + 2]);
                i += 3;
                break;
            case EVGR_LINETO:
                evgrTransformPoint(&vals[i + 1], &vals[i + 2], state->xform, vals[i + 1], vals[i + 2]);
                i += 3;
                break;
            case EVGR_BEZIERTO:
                evgrTransformPoint(&vals[i + 1], &vals[i + 2], state->xform, vals[i + 1], vals[i + 2]);
                evgrTransformPoint(&vals[i + 3], &vals[i + 4], state->xform, vals[i + 3], vals[i + 4]);
                evgrTransformPoint(&vals[i + 5], &vals[i + 6], state->xform, vals[i + 5], vals[i + 6]);
                i += 7;
                break;
            case EVGR_CLOSE:
                i++;
                break;
            case EVGR_WINDING:
                i += 2;
                break;
            default:
                i++;
        }
    }

    lv_memcpy(&ctx->commands[ctx->ncommands], vals, nvals * sizeof(float));

    ctx->ncommands += nvals;
}


static void evgr__clearPathCache(EVGRcontext * ctx)
{
    ctx->cache->npoints = 0;
    ctx->cache->npaths = 0;
}

static EVGRpath * evgr__lastPath(EVGRcontext * ctx)
{
    if(ctx->cache->npaths > 0)
        return &ctx->cache->paths[ctx->cache->npaths - 1];
    return NULL;
}

static void evgr__addPath(EVGRcontext * ctx)
{
    EVGRpath * path;
    if(ctx->cache->npaths + 1 > ctx->cache->cpaths) {
        EVGRpath * paths;
        int cpaths = ctx->cache->npaths + 1 + ctx->cache->cpaths / 2;
        paths = (EVGRpath *)lv_realloc(ctx->cache->paths, sizeof(EVGRpath) * cpaths);
        if(paths == NULL) return;
        ctx->cache->paths = paths;
        ctx->cache->cpaths = cpaths;
    }
    path = &ctx->cache->paths[ctx->cache->npaths];
    lv_memzero(path, sizeof(*path));
    path->first = ctx->cache->npoints;
    path->winding = EVGR_CCW;

    ctx->cache->npaths++;
}

static EVGRpoint * evgr__lastPoint(EVGRcontext * ctx)
{
    if(ctx->cache->npoints > 0)
        return &ctx->cache->points[ctx->cache->npoints - 1];
    return NULL;
}

static void evgr__addPoint(EVGRcontext * ctx, float x, float y, int flags)
{
    EVGRpath * path = evgr__lastPath(ctx);
    EVGRpoint * pt;
    if(path == NULL) return;

    if(path->count > 0 && ctx->cache->npoints > 0) {
        pt = evgr__lastPoint(ctx);
        if(evgr__ptEquals(pt->x, pt->y, x, y, ctx->distTol)) {
            pt->flags |= flags;
            return;
        }
    }

    if(ctx->cache->npoints + 1 > ctx->cache->cpoints) {
        EVGRpoint * points;
        int cpoints = ctx->cache->npoints + 1 + ctx->cache->cpoints / 2;
        points = (EVGRpoint *)lv_realloc(ctx->cache->points, sizeof(EVGRpoint) * cpoints);
        if(points == NULL) return;
        ctx->cache->points = points;
        ctx->cache->cpoints = cpoints;
    }

    pt = &ctx->cache->points[ctx->cache->npoints];
    lv_memzero(pt, sizeof(*pt));
    pt->x = x;
    pt->y = y;
    pt->flags = (unsigned char)flags;

    ctx->cache->npoints++;
    path->count++;
}

static void evgr__closePath(EVGRcontext * ctx)
{
    EVGRpath * path = evgr__lastPath(ctx);
    if(path == NULL) return;
    path->closed = 1;
}

static void evgr__pathWinding(EVGRcontext * ctx, int winding)
{
    EVGRpath * path = evgr__lastPath(ctx);
    if(path == NULL) return;
    path->winding = winding;
}

static float evgr__getAverageScale(float * t)
{
    float sx = sqrtf(t[0] * t[0] + t[2] * t[2]);
    float sy = sqrtf(t[1] * t[1] + t[3] * t[3]);
    return (sx + sy) * 0.5f;
}

static EVGRvertex * evgr__allocTempVerts(EVGRcontext * ctx, int nverts)
{
    if(nverts > ctx->cache->cverts) {
        EVGRvertex * verts;
        int cverts = (nverts + 0xff) & ~0xff; // Round up to prevent allocations when things change just slightly.
        verts = (EVGRvertex *)lv_realloc(ctx->cache->verts, sizeof(EVGRvertex) * cverts);
        if(verts == NULL) return NULL;
        ctx->cache->verts = verts;
        ctx->cache->cverts = cverts;
    }

    return ctx->cache->verts;
}

static float evgr__triarea2(float ax, float ay, float bx, float by, float cx, float cy)
{
    float abx = bx - ax;
    float aby = by - ay;
    float acx = cx - ax;
    float acy = cy - ay;
    return acx * aby - abx * acy;
}

static float evgr__polyArea(EVGRpoint * pts, int npts)
{
    int i;
    float area = 0;
    for(i = 2; i < npts; i++) {
        EVGRpoint * a = &pts[0];
        EVGRpoint * b = &pts[i - 1];
        EVGRpoint * c = &pts[i];
        area += evgr__triarea2(a->x, a->y, b->x, b->y, c->x, c->y);
    }
    return area * 0.5f;
}

static void evgr__polyReverse(EVGRpoint * pts, int npts)
{
    EVGRpoint tmp;
    int i = 0, j = npts - 1;
    while(i < j) {
        tmp = pts[i];
        pts[i] = pts[j];
        pts[j] = tmp;
        i++;
        j--;
    }
}


static void evgr__vset(EVGRvertex * vtx, float x, float y, float u, float v)
{
    vtx->x = x;
    vtx->y = y;
    vtx->u = u;
    vtx->v = v;
}

static void evgr__tesselateBezier(EVGRcontext * ctx,
                                 float x1, float y1, float x2, float y2,
                                 float x3, float y3, float x4, float y4,
                                 int level, int type)
{
    float x12, y12, x23, y23, x34, y34, x123, y123, x234, y234, x1234, y1234;
    float dx, dy, d2, d3;

    if(level > 10) return;

    x12 = (x1 + x2) * 0.5f;
    y12 = (y1 + y2) * 0.5f;
    x23 = (x2 + x3) * 0.5f;
    y23 = (y2 + y3) * 0.5f;
    x34 = (x3 + x4) * 0.5f;
    y34 = (y3 + y4) * 0.5f;
    x123 = (x12 + x23) * 0.5f;
    y123 = (y12 + y23) * 0.5f;

    dx = x4 - x1;
    dy = y4 - y1;
    d2 = evgr__absf(((x2 - x4) * dy - (y2 - y4) * dx));
    d3 = evgr__absf(((x3 - x4) * dy - (y3 - y4) * dx));

    if((d2 + d3) * (d2 + d3) < ctx->tessTol * (dx * dx + dy * dy)) {
        evgr__addPoint(ctx, x4, y4, type);
        return;
    }

    /*  if (evgr__absf(x1+x3-x2-x2) + evgr__absf(y1+y3-y2-y2) + evgr__absf(x2+x4-x3-x3) + evgr__absf(y2+y4-y3-y3) < ctx->tessTol) {
            evgr__addPoint(ctx, x4, y4, type);
            return;
        }*/

    x234 = (x23 + x34) * 0.5f;
    y234 = (y23 + y34) * 0.5f;
    x1234 = (x123 + x234) * 0.5f;
    y1234 = (y123 + y234) * 0.5f;

    evgr__tesselateBezier(ctx, x1, y1, x12, y12, x123, y123, x1234, y1234, level + 1, 0);
    evgr__tesselateBezier(ctx, x1234, y1234, x234, y234, x34, y34, x4, y4, level + 1, type);
}

static void evgr__flattenPaths(EVGRcontext * ctx)
{
    EVGRpathCache * cache = ctx->cache;
    //  EVGRstate* state = evgr__getState(ctx);
    EVGRpoint * last;
    EVGRpoint * p0;
    EVGRpoint * p1;
    EVGRpoint * pts;
    EVGRpath * path;
    int i, j;
    float * cp1;
    float * cp2;
    float * p;
    float area;

    if(cache->npaths > 0)
        return;

    // Flatten
    i = 0;
    while(i < ctx->ncommands) {
        int cmd = (int)ctx->commands[i];
        switch(cmd) {
            case EVGR_MOVETO:
                evgr__addPath(ctx);
                p = &ctx->commands[i + 1];
                evgr__addPoint(ctx, p[0], p[1], EVGR_PT_CORNER);
                i += 3;
                break;
            case EVGR_LINETO:
                p = &ctx->commands[i + 1];
                evgr__addPoint(ctx, p[0], p[1], EVGR_PT_CORNER);
                i += 3;
                break;
            case EVGR_BEZIERTO:
                last = evgr__lastPoint(ctx);
                if(last != NULL) {
                    cp1 = &ctx->commands[i + 1];
                    cp2 = &ctx->commands[i + 3];
                    p = &ctx->commands[i + 5];
                    evgr__tesselateBezier(ctx, last->x, last->y, cp1[0], cp1[1], cp2[0], cp2[1], p[0], p[1], 0, EVGR_PT_CORNER);
                }
                i += 7;
                break;
            case EVGR_CLOSE:
                evgr__closePath(ctx);
                i++;
                break;
            case EVGR_WINDING:
                evgr__pathWinding(ctx, (int)ctx->commands[i + 1]);
                i += 2;
                break;
            default:
                i++;
        }
    }

    cache->bounds[0] = cache->bounds[1] = 1e6f;
    cache->bounds[2] = cache->bounds[3] = -1e6f;

    // Calculate the direction and length of line segments.
    for(j = 0; j < cache->npaths; j++) {
        path = &cache->paths[j];
        pts = &cache->points[path->first];

        // If the first and last points are the same, remove the last, mark as closed path.
        p0 = &pts[path->count - 1];
        p1 = &pts[0];
        if(evgr__ptEquals(p0->x, p0->y, p1->x, p1->y, ctx->distTol)) {
            path->count--;
            p0 = &pts[path->count - 1];
            path->closed = 1;
        }

        // Enforce winding.
        if(path->count > 2) {
            area = evgr__polyArea(pts, path->count);
            if(path->winding == EVGR_CCW && area < 0.0f)
                evgr__polyReverse(pts, path->count);
            if(path->winding == EVGR_CW && area > 0.0f)
                evgr__polyReverse(pts, path->count);
        }

        for(i = 0; i < path->count; i++) {
            // Calculate segment direction and length
            p0->dx = p1->x - p0->x;
            p0->dy = p1->y - p0->y;
            p0->len = evgr__normalize(&p0->dx, &p0->dy);
            // Update bounds
            cache->bounds[0] = evgr__minf(cache->bounds[0], p0->x);
            cache->bounds[1] = evgr__minf(cache->bounds[1], p0->y);
            cache->bounds[2] = evgr__maxf(cache->bounds[2], p0->x);
            cache->bounds[3] = evgr__maxf(cache->bounds[3], p0->y);
            // Advance
            p0 = p1++;
        }
    }
}

static int evgr__curveDivs(float r, float arc, float tol)
{
    float da = acosf(r / (r + tol)) * 2.0f;
    return evgr__maxi(2, (int)ceilf(arc / da));
}

static void evgr__chooseBevel(int bevel, EVGRpoint * p0, EVGRpoint * p1, float w,
                             float * x0, float * y0, float * x1, float * y1)
{
    if(bevel) {
        *x0 = p1->x + p0->dy * w;
        *y0 = p1->y - p0->dx * w;
        *x1 = p1->x + p1->dy * w;
        *y1 = p1->y - p1->dx * w;
    }
    else {
        *x0 = p1->x + p1->dmx * w;
        *y0 = p1->y + p1->dmy * w;
        *x1 = p1->x + p1->dmx * w;
        *y1 = p1->y + p1->dmy * w;
    }
}

static EVGRvertex * evgr__roundJoin(EVGRvertex * dst, EVGRpoint * p0, EVGRpoint * p1,
                                  float lw, float rw, float lu, float ru, int ncap,
                                  float fringe)
{
    int i, n;
    float dlx0 = p0->dy;
    float dly0 = -p0->dx;
    float dlx1 = p1->dy;
    float dly1 = -p1->dx;
    EVGR_NOTUSED(fringe);

    if(p1->flags & EVGR_PT_LEFT) {
        float lx0, ly0, lx1, ly1, a0, a1;
        evgr__chooseBevel(p1->flags & EVGR_PR_INNERBEVEL, p0, p1, lw, &lx0, &ly0, &lx1, &ly1);
        a0 = atan2f(-dly0, -dlx0);
        a1 = atan2f(-dly1, -dlx1);
        if(a1 > a0) a1 -= EVGR_PI * 2;

        evgr__vset(dst, lx0, ly0, lu, 1);
        dst++;
        evgr__vset(dst, p1->x - dlx0 * rw, p1->y - dly0 * rw, ru, 1);
        dst++;

        n = evgr__clampi((int)ceilf(((a0 - a1) / EVGR_PI) * ncap), 2, ncap);
        for(i = 0; i < n; i++) {
            float u = i / (float)(n - 1);
            float a = a0 + u * (a1 - a0);
            float rx = p1->x + cosf(a) * rw;
            float ry = p1->y + sinf(a) * rw;
            evgr__vset(dst, p1->x, p1->y, 0.5f, 1);
            dst++;
            evgr__vset(dst, rx, ry, ru, 1);
            dst++;
        }

        evgr__vset(dst, lx1, ly1, lu, 1);
        dst++;
        evgr__vset(dst, p1->x - dlx1 * rw, p1->y - dly1 * rw, ru, 1);
        dst++;

    }
    else {
        float rx0, ry0, rx1, ry1, a0, a1;
        evgr__chooseBevel(p1->flags & EVGR_PR_INNERBEVEL, p0, p1, -rw, &rx0, &ry0, &rx1, &ry1);
        a0 = atan2f(dly0, dlx0);
        a1 = atan2f(dly1, dlx1);
        if(a1 < a0) a1 += EVGR_PI * 2;

        evgr__vset(dst, p1->x + dlx0 * rw, p1->y + dly0 * rw, lu, 1);
        dst++;
        evgr__vset(dst, rx0, ry0, ru, 1);
        dst++;

        n = evgr__clampi((int)ceilf(((a1 - a0) / EVGR_PI) * ncap), 2, ncap);
        for(i = 0; i < n; i++) {
            float u = i / (float)(n - 1);
            float a = a0 + u * (a1 - a0);
            float lx = p1->x + cosf(a) * lw;
            float ly = p1->y + sinf(a) * lw;
            evgr__vset(dst, lx, ly, lu, 1);
            dst++;
            evgr__vset(dst, p1->x, p1->y, 0.5f, 1);
            dst++;
        }

        evgr__vset(dst, p1->x + dlx1 * rw, p1->y + dly1 * rw, lu, 1);
        dst++;
        evgr__vset(dst, rx1, ry1, ru, 1);
        dst++;

    }
    return dst;
}

static EVGRvertex * evgr__bevelJoin(EVGRvertex * dst, EVGRpoint * p0, EVGRpoint * p1,
                                  float lw, float rw, float lu, float ru, float fringe)
{
    float rx0, ry0, rx1, ry1;
    float lx0, ly0, lx1, ly1;
    float dlx0 = p0->dy;
    float dly0 = -p0->dx;
    float dlx1 = p1->dy;
    float dly1 = -p1->dx;
    EVGR_NOTUSED(fringe);

    if(p1->flags & EVGR_PT_LEFT) {
        evgr__chooseBevel(p1->flags & EVGR_PR_INNERBEVEL, p0, p1, lw, &lx0, &ly0, &lx1, &ly1);

        evgr__vset(dst, lx0, ly0, lu, 1);
        dst++;
        evgr__vset(dst, p1->x - dlx0 * rw, p1->y - dly0 * rw, ru, 1);
        dst++;

        if(p1->flags & EVGR_PT_BEVEL) {
            evgr__vset(dst, lx0, ly0, lu, 1);
            dst++;
            evgr__vset(dst, p1->x - dlx0 * rw, p1->y - dly0 * rw, ru, 1);
            dst++;

            evgr__vset(dst, lx1, ly1, lu, 1);
            dst++;
            evgr__vset(dst, p1->x - dlx1 * rw, p1->y - dly1 * rw, ru, 1);
            dst++;
        }
        else {
            rx0 = p1->x - p1->dmx * rw;
            ry0 = p1->y - p1->dmy * rw;

            evgr__vset(dst, p1->x, p1->y, 0.5f, 1);
            dst++;
            evgr__vset(dst, p1->x - dlx0 * rw, p1->y - dly0 * rw, ru, 1);
            dst++;

            evgr__vset(dst, rx0, ry0, ru, 1);
            dst++;
            evgr__vset(dst, rx0, ry0, ru, 1);
            dst++;

            evgr__vset(dst, p1->x, p1->y, 0.5f, 1);
            dst++;
            evgr__vset(dst, p1->x - dlx1 * rw, p1->y - dly1 * rw, ru, 1);
            dst++;
        }

        evgr__vset(dst, lx1, ly1, lu, 1);
        dst++;
        evgr__vset(dst, p1->x - dlx1 * rw, p1->y - dly1 * rw, ru, 1);
        dst++;

    }
    else {
        evgr__chooseBevel(p1->flags & EVGR_PR_INNERBEVEL, p0, p1, -rw, &rx0, &ry0, &rx1, &ry1);

        evgr__vset(dst, p1->x + dlx0 * lw, p1->y + dly0 * lw, lu, 1);
        dst++;
        evgr__vset(dst, rx0, ry0, ru, 1);
        dst++;

        if(p1->flags & EVGR_PT_BEVEL) {
            evgr__vset(dst, p1->x + dlx0 * lw, p1->y + dly0 * lw, lu, 1);
            dst++;
            evgr__vset(dst, rx0, ry0, ru, 1);
            dst++;

            evgr__vset(dst, p1->x + dlx1 * lw, p1->y + dly1 * lw, lu, 1);
            dst++;
            evgr__vset(dst, rx1, ry1, ru, 1);
            dst++;
        }
        else {
            lx0 = p1->x + p1->dmx * lw;
            ly0 = p1->y + p1->dmy * lw;

            evgr__vset(dst, p1->x + dlx0 * lw, p1->y + dly0 * lw, lu, 1);
            dst++;
            evgr__vset(dst, p1->x, p1->y, 0.5f, 1);
            dst++;

            evgr__vset(dst, lx0, ly0, lu, 1);
            dst++;
            evgr__vset(dst, lx0, ly0, lu, 1);
            dst++;

            evgr__vset(dst, p1->x + dlx1 * lw, p1->y + dly1 * lw, lu, 1);
            dst++;
            evgr__vset(dst, p1->x, p1->y, 0.5f, 1);
            dst++;
        }

        evgr__vset(dst, p1->x + dlx1 * lw, p1->y + dly1 * lw, lu, 1);
        dst++;
        evgr__vset(dst, rx1, ry1, ru, 1);
        dst++;
    }

    return dst;
}

static EVGRvertex * evgr__buttCapStart(EVGRvertex * dst, EVGRpoint * p,
                                     float dx, float dy, float w, float d,
                                     float aa, float u0, float u1)
{
    float px = p->x - dx * d;
    float py = p->y - dy * d;
    float dlx = dy;
    float dly = -dx;
    evgr__vset(dst, px + dlx * w - dx * aa, py + dly * w - dy * aa, u0, 0);
    dst++;
    evgr__vset(dst, px - dlx * w - dx * aa, py - dly * w - dy * aa, u1, 0);
    dst++;
    evgr__vset(dst, px + dlx * w, py + dly * w, u0, 1);
    dst++;
    evgr__vset(dst, px - dlx * w, py - dly * w, u1, 1);
    dst++;
    return dst;
}

static EVGRvertex * evgr__buttCapEnd(EVGRvertex * dst, EVGRpoint * p,
                                   float dx, float dy, float w, float d,
                                   float aa, float u0, float u1)
{
    float px = p->x + dx * d;
    float py = p->y + dy * d;
    float dlx = dy;
    float dly = -dx;
    evgr__vset(dst, px + dlx * w, py + dly * w, u0, 1);
    dst++;
    evgr__vset(dst, px - dlx * w, py - dly * w, u1, 1);
    dst++;
    evgr__vset(dst, px + dlx * w + dx * aa, py + dly * w + dy * aa, u0, 0);
    dst++;
    evgr__vset(dst, px - dlx * w + dx * aa, py - dly * w + dy * aa, u1, 0);
    dst++;
    return dst;
}


static EVGRvertex * evgr__roundCapStart(EVGRvertex * dst, EVGRpoint * p,
                                      float dx, float dy, float w, int ncap,
                                      float aa, float u0, float u1)
{
    int i;
    float px = p->x;
    float py = p->y;
    float dlx = dy;
    float dly = -dx;
    EVGR_NOTUSED(aa);
    for(i = 0; i < ncap; i++) {
        float a = i / (float)(ncap - 1) * EVGR_PI;
        float ax = cosf(a) * w, ay = sinf(a) * w;
        evgr__vset(dst, px - dlx * ax - dx * ay, py - dly * ax - dy * ay, u0, 1);
        dst++;
        evgr__vset(dst, px, py, 0.5f, 1);
        dst++;
    }
    evgr__vset(dst, px + dlx * w, py + dly * w, u0, 1);
    dst++;
    evgr__vset(dst, px - dlx * w, py - dly * w, u1, 1);
    dst++;
    return dst;
}

static EVGRvertex * evgr__roundCapEnd(EVGRvertex * dst, EVGRpoint * p,
                                    float dx, float dy, float w, int ncap,
                                    float aa, float u0, float u1)
{
    int i;
    float px = p->x;
    float py = p->y;
    float dlx = dy;
    float dly = -dx;
    EVGR_NOTUSED(aa);
    evgr__vset(dst, px + dlx * w, py + dly * w, u0, 1);
    dst++;
    evgr__vset(dst, px - dlx * w, py - dly * w, u1, 1);
    dst++;
    for(i = 0; i < ncap; i++) {
        float a = i / (float)(ncap - 1) * EVGR_PI;
        float ax = cosf(a) * w, ay = sinf(a) * w;
        evgr__vset(dst, px, py, 0.5f, 1);
        dst++;
        evgr__vset(dst, px - dlx * ax + dx * ay, py - dly * ax + dy * ay, u0, 1);
        dst++;
    }
    return dst;
}


static void evgr__calculateJoins(EVGRcontext * ctx, float w, int lineJoin, float miterLimit)
{
    EVGRpathCache * cache = ctx->cache;
    int i, j;
    float iw = 0.0f;

    if(w > 0.0f) iw = 1.0f / w;

    // Calculate which joins needs extra vertices to append, and gather vertex count.
    for(i = 0; i < cache->npaths; i++) {
        EVGRpath * path = &cache->paths[i];
        EVGRpoint * pts = &cache->points[path->first];
        EVGRpoint * p0 = &pts[path->count - 1];
        EVGRpoint * p1 = &pts[0];
        int nleft = 0;

        path->nbevel = 0;

        for(j = 0; j < path->count; j++) {
            float dlx0, dly0, dlx1, dly1, dmr2, cross, limit;
            dlx0 = p0->dy;
            dly0 = -p0->dx;
            dlx1 = p1->dy;
            dly1 = -p1->dx;
            // Calculate extrusions
            p1->dmx = (dlx0 + dlx1) * 0.5f;
            p1->dmy = (dly0 + dly1) * 0.5f;
            dmr2 = p1->dmx * p1->dmx + p1->dmy * p1->dmy;
            if(dmr2 > 0.000001f) {
                float scale = 1.0f / dmr2;
                if(scale > 600.0f) {
                    scale = 600.0f;
                }
                p1->dmx *= scale;
                p1->dmy *= scale;
            }

            // Clear flags, but keep the corner.
            p1->flags = (p1->flags & EVGR_PT_CORNER) ? EVGR_PT_CORNER : 0;

            // Keep track of left turns.
            cross = p1->dx * p0->dy - p0->dx * p1->dy;
            if(cross > 0.0f) {
                nleft++;
                p1->flags |= EVGR_PT_LEFT;
            }

            // Calculate if we should use bevel or miter for inner join.
            limit = evgr__maxf(1.01f, evgr__minf(p0->len, p1->len) * iw);
            if((dmr2 * limit * limit) < 1.0f)
                p1->flags |= EVGR_PR_INNERBEVEL;

            // Check to see if the corner needs to be beveled.
            if(p1->flags & EVGR_PT_CORNER) {
                if((dmr2 * miterLimit * miterLimit) < 1.0f || lineJoin == EVGR_BEVEL || lineJoin == EVGR_ROUND) {
                    p1->flags |= EVGR_PT_BEVEL;
                }
            }

            if((p1->flags & (EVGR_PT_BEVEL | EVGR_PR_INNERBEVEL)) != 0)
                path->nbevel++;

            p0 = p1++;
        }

        path->convex = (nleft == path->count) ? 1 : 0;
    }
}


static int evgr__expandStroke(EVGRcontext * ctx, float w, float fringe, int lineCap, int lineJoin, float miterLimit)
{
    EVGRpathCache * cache = ctx->cache;
    EVGRvertex * verts;
    EVGRvertex * dst;
    int cverts, i, j;
    float aa = fringe;//ctx->fringeWidth;
    float u0 = 0.0f, u1 = 1.0f;
    int ncap = evgr__curveDivs(w, EVGR_PI, ctx->tessTol); // Calculate divisions per half circle.

    w += aa * 0.5f;

    // Disable the gradient used for antialiasing when antialiasing is not used.
    if(aa == 0.0f) {
        u0 = 0.5f;
        u1 = 0.5f;
    }

    evgr__calculateJoins(ctx, w, lineJoin, miterLimit);

    // Calculate max vertex usage.
    cverts = 0;
    for(i = 0; i < cache->npaths; i++) {
        EVGRpath * path = &cache->paths[i];
        int loop = (path->closed == 0) ? 0 : 1;
        if(lineJoin == EVGR_ROUND)
            cverts += (path->count + path->nbevel * (ncap + 2) + 1) * 2; // plus one for loop
        else
            cverts += (path->count + path->nbevel * 5 + 1) * 2; // plus one for loop
        if(loop == 0) {
            // space for caps
            if(lineCap == EVGR_ROUND) {
                cverts += (ncap * 2 + 2) * 2;
            }
            else {
                cverts += (3 + 3) * 2;
            }
        }
    }

    verts = evgr__allocTempVerts(ctx, cverts);
    if(verts == NULL) return 0;

    for(i = 0; i < cache->npaths; i++) {
        EVGRpath * path = &cache->paths[i];
        EVGRpoint * pts = &cache->points[path->first];
        EVGRpoint * p0;
        EVGRpoint * p1;
        int s, e, loop;
        float dx, dy;

        path->fill = 0;
        path->nfill = 0;

        // Calculate fringe or stroke
        loop = (path->closed == 0) ? 0 : 1;
        dst = verts;
        path->stroke = dst;

        if(loop) {
            // Looping
            p0 = &pts[path->count - 1];
            p1 = &pts[0];
            s = 0;
            e = path->count;
        }
        else {
            // Add cap
            p0 = &pts[0];
            p1 = &pts[1];
            s = 1;
            e = path->count - 1;
        }

        if(loop == 0) {
            // Add cap
            dx = p1->x - p0->x;
            dy = p1->y - p0->y;
            evgr__normalize(&dx, &dy);
            if(lineCap == EVGR_BUTT)
                dst = evgr__buttCapStart(dst, p0, dx, dy, w, -aa * 0.5f, aa, u0, u1);
            else if(lineCap == EVGR_BUTT || lineCap == EVGR_SQUARE)
                dst = evgr__buttCapStart(dst, p0, dx, dy, w, w - aa, aa, u0, u1);
            else if(lineCap == EVGR_ROUND)
                dst = evgr__roundCapStart(dst, p0, dx, dy, w, ncap, aa, u0, u1);
        }

        for(j = s; j < e; ++j) {
            if((p1->flags & (EVGR_PT_BEVEL | EVGR_PR_INNERBEVEL)) != 0) {
                if(lineJoin == EVGR_ROUND) {
                    dst = evgr__roundJoin(dst, p0, p1, w, w, u0, u1, ncap, aa);
                }
                else {
                    dst = evgr__bevelJoin(dst, p0, p1, w, w, u0, u1, aa);
                }
            }
            else {
                evgr__vset(dst, p1->x + (p1->dmx * w), p1->y + (p1->dmy * w), u0, 1);
                dst++;
                evgr__vset(dst, p1->x - (p1->dmx * w), p1->y - (p1->dmy * w), u1, 1);
                dst++;
            }
            p0 = p1++;
        }

        if(loop) {
            // Loop it
            evgr__vset(dst, verts[0].x, verts[0].y, u0, 1);
            dst++;
            evgr__vset(dst, verts[1].x, verts[1].y, u1, 1);
            dst++;
        }
        else {
            // Add cap
            dx = p1->x - p0->x;
            dy = p1->y - p0->y;
            evgr__normalize(&dx, &dy);
            if(lineCap == EVGR_BUTT)
                dst = evgr__buttCapEnd(dst, p1, dx, dy, w, -aa * 0.5f, aa, u0, u1);
            else if(lineCap == EVGR_BUTT || lineCap == EVGR_SQUARE)
                dst = evgr__buttCapEnd(dst, p1, dx, dy, w, w - aa, aa, u0, u1);
            else if(lineCap == EVGR_ROUND)
                dst = evgr__roundCapEnd(dst, p1, dx, dy, w, ncap, aa, u0, u1);
        }

        path->nstroke = (int)(dst - verts);

        verts = dst;
    }

    return 1;
}

static int evgr__expandFill(EVGRcontext * ctx, float w, int lineJoin, float miterLimit)
{
    EVGRpathCache * cache = ctx->cache;
    EVGRvertex * verts;
    EVGRvertex * dst;
    int cverts, convex, i, j;
    float aa = ctx->fringeWidth;
    int fringe = w > 0.0f;

    evgr__calculateJoins(ctx, w, lineJoin, miterLimit);

    // Calculate max vertex usage.
    cverts = 0;
    for(i = 0; i < cache->npaths; i++) {
        EVGRpath * path = &cache->paths[i];
        cverts += path->count + path->nbevel + 1;
        if(fringe)
            cverts += (path->count + path->nbevel * 5 + 1) * 2; // plus one for loop
    }

    verts = evgr__allocTempVerts(ctx, cverts);
    if(verts == NULL) return 0;

    convex = cache->npaths == 1 && cache->paths[0].convex;

    for(i = 0; i < cache->npaths; i++) {
        EVGRpath * path = &cache->paths[i];
        EVGRpoint * pts = &cache->points[path->first];
        EVGRpoint * p0;
        EVGRpoint * p1;
        float rw, lw, woff;
        float ru, lu;

        // Calculate shape vertices.
        woff = 0.5f * aa;
        dst = verts;
        path->fill = dst;

        if(fringe) {
            // Looping
            p0 = &pts[path->count - 1];
            p1 = &pts[0];
            for(j = 0; j < path->count; ++j) {
                if(p1->flags & EVGR_PT_BEVEL) {
                    float dlx0 = p0->dy;
                    float dly0 = -p0->dx;
                    float dlx1 = p1->dy;
                    float dly1 = -p1->dx;
                    if(p1->flags & EVGR_PT_LEFT) {
                        float lx = p1->x + p1->dmx * woff;
                        float ly = p1->y + p1->dmy * woff;
                        evgr__vset(dst, lx, ly, 0.5f, 1);
                        dst++;
                    }
                    else {
                        float lx0 = p1->x + dlx0 * woff;
                        float ly0 = p1->y + dly0 * woff;
                        float lx1 = p1->x + dlx1 * woff;
                        float ly1 = p1->y + dly1 * woff;
                        evgr__vset(dst, lx0, ly0, 0.5f, 1);
                        dst++;
                        evgr__vset(dst, lx1, ly1, 0.5f, 1);
                        dst++;
                    }
                }
                else {
                    evgr__vset(dst, p1->x + (p1->dmx * woff), p1->y + (p1->dmy * woff), 0.5f, 1);
                    dst++;
                }
                p0 = p1++;
            }
        }
        else {
            for(j = 0; j < path->count; ++j) {
                evgr__vset(dst, pts[j].x, pts[j].y, 0.5f, 1);
                dst++;
            }
        }

        path->nfill = (int)(dst - verts);
        verts = dst;

        // Calculate fringe
        if(fringe) {
            lw = w + woff;
            rw = w - woff;
            lu = 0;
            ru = 1;
            dst = verts;
            path->stroke = dst;

            // Create only half a fringe for convex shapes so that
            // the shape can be rendered without stenciling.
            if(convex) {
                lw = woff;  // This should generate the same vertex as fill inset above.
                lu = 0.5f;  // Set outline fade at middle.
            }

            // Looping
            p0 = &pts[path->count - 1];
            p1 = &pts[0];

            for(j = 0; j < path->count; ++j) {
                if((p1->flags & (EVGR_PT_BEVEL | EVGR_PR_INNERBEVEL)) != 0) {
                    dst = evgr__bevelJoin(dst, p0, p1, lw, rw, lu, ru, ctx->fringeWidth);
                }
                else {
                    evgr__vset(dst, p1->x + (p1->dmx * lw), p1->y + (p1->dmy * lw), lu, 1);
                    dst++;
                    evgr__vset(dst, p1->x - (p1->dmx * rw), p1->y - (p1->dmy * rw), ru, 1);
                    dst++;
                }
                p0 = p1++;
            }

            // Loop it
            evgr__vset(dst, verts[0].x, verts[0].y, lu, 1);
            dst++;
            evgr__vset(dst, verts[1].x, verts[1].y, ru, 1);
            dst++;

            path->nstroke = (int)(dst - verts);
            verts = dst;
        }
        else {
            path->stroke = NULL;
            path->nstroke = 0;
        }
    }

    return 1;
}


// Draw
void evgrBeginPath(EVGRcontext * ctx)
{
    ctx->ncommands = 0;
    evgr__clearPathCache(ctx);
}

void evgrMoveTo(EVGRcontext * ctx, float x, float y)
{
    float vals[] = { EVGR_MOVETO, x, y };
    evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
}

void evgrLineTo(EVGRcontext * ctx, float x, float y)
{
    float vals[] = { EVGR_LINETO, x, y };
    evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
}

void evgrBezierTo(EVGRcontext * ctx, float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    float vals[] = { EVGR_BEZIERTO, c1x, c1y, c2x, c2y, x, y };
    evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
}

void evgrQuadTo(EVGRcontext * ctx, float cx, float cy, float x, float y)
{
    float x0 = ctx->commandx;
    float y0 = ctx->commandy;
    float vals[] = { EVGR_BEZIERTO,
                     x0 + 2.0f / 3.0f * (cx - x0), y0 + 2.0f / 3.0f * (cy - y0),
                     x + 2.0f / 3.0f * (cx - x), y + 2.0f / 3.0f * (cy - y),
                     x, y
                   };
    evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
}

void evgrArcTo(EVGRcontext * ctx, float x1, float y1, float x2, float y2, float radius)
{
    float x0 = ctx->commandx;
    float y0 = ctx->commandy;
    float dx0, dy0, dx1, dy1, a, d, cx, cy, a0, a1;
    int dir;

    if(ctx->ncommands == 0) {
        return;
    }

    // Handle degenerate cases.
    if(evgr__ptEquals(x0, y0, x1, y1, ctx->distTol) ||
       evgr__ptEquals(x1, y1, x2, y2, ctx->distTol) ||
       evgr__distPtSeg(x1, y1, x0, y0, x2, y2) < ctx->distTol * ctx->distTol ||
       radius < ctx->distTol) {
        evgrLineTo(ctx, x1, y1);
        return;
    }

    // Calculate tangential circle to lines (x0,y0)-(x1,y1) and (x1,y1)-(x2,y2).
    dx0 = x0 - x1;
    dy0 = y0 - y1;
    dx1 = x2 - x1;
    dy1 = y2 - y1;
    evgr__normalize(&dx0, &dy0);
    evgr__normalize(&dx1, &dy1);
    a = evgr__acosf(dx0 * dx1 + dy0 * dy1);
    d = radius / evgr__tanf(a / 2.0f);

    //  printf("a=%f° d=%f\n", a/EVGR_PI*180.0f, d);

    if(d > 10000.0f) {
        evgrLineTo(ctx, x1, y1);
        return;
    }

    if(evgr__cross(dx0, dy0, dx1, dy1) > 0.0f) {
        cx = x1 + dx0 * d + dy0 * radius;
        cy = y1 + dy0 * d + -dx0 * radius;
        a0 = evgr__atan2f(dx0, -dy0);
        a1 = evgr__atan2f(-dx1, dy1);
        dir = EVGR_CW;
        //      printf("CW c=(%f, %f) a0=%f° a1=%f°\n", cx, cy, a0/EVGR_PI*180.0f, a1/EVGR_PI*180.0f);
    }
    else {
        cx = x1 + dx0 * d + -dy0 * radius;
        cy = y1 + dy0 * d + dx0 * radius;
        a0 = evgr__atan2f(-dx0, dy0);
        a1 = evgr__atan2f(dx1, -dy1);
        dir = EVGR_CCW;
        //      printf("CCW c=(%f, %f) a0=%f° a1=%f°\n", cx, cy, a0/EVGR_PI*180.0f, a1/EVGR_PI*180.0f);
    }

    evgrArc(ctx, cx, cy, radius, a0, a1, dir);
}

void evgrClosePath(EVGRcontext * ctx)
{
    float vals[] = { EVGR_CLOSE };
    evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
}

void evgrPathWinding(EVGRcontext * ctx, int dir)
{
    float vals[] = { EVGR_WINDING, (float)dir };
    evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
}

void evgrArc(EVGRcontext * ctx, float cx, float cy, float r, float a0, float a1, int dir)
{
    float a = 0, da = 0, hda = 0, kappa = 0;
    float dx = 0, dy = 0, x = 0, y = 0, tanx = 0, tany = 0;
    float px = 0, py = 0, ptanx = 0, ptany = 0;
    float vals[3 + 5 * 7 + 100];
    int i, ndivs, nvals;
    int move = ctx->ncommands > 0 ? EVGR_LINETO : EVGR_MOVETO;

    // Clamp angles
    da = a1 - a0;
    if(dir == EVGR_CW) {
        if(evgr__absf(da) >= EVGR_PI * 2) {
            da = EVGR_PI * 2;
        }
        else {
            while(da < 0.0f) da += EVGR_PI * 2;
        }
    }
    else {
        if(evgr__absf(da) >= EVGR_PI * 2) {
            da = -EVGR_PI * 2;
        }
        else {
            while(da > 0.0f) da -= EVGR_PI * 2;
        }
    }

    // Split arc into max 90 degree segments.
    ndivs = evgr__maxi(1, evgr__mini((int)(evgr__absf(da) / (EVGR_PI * 0.5f) + 0.5f), 5));
    hda = (da / (float)ndivs) / 2.0f;
    kappa = evgr__absf(4.0f / 3.0f * (1.0f - evgr__cosf(hda)) / evgr__sinf(hda));

    if(dir == EVGR_CCW)
        kappa = -kappa;

    nvals = 0;
    for(i = 0; i <= ndivs; i++) {
        a = a0 + da * (i / (float)ndivs);
        dx = evgr__cosf(a);
        dy = evgr__sinf(a);
        x = cx + dx * r;
        y = cy + dy * r;
        tanx = -dy * r * kappa;
        tany = dx * r * kappa;

        if(i == 0) {
            vals[nvals++] = (float)move;
            vals[nvals++] = x;
            vals[nvals++] = y;
        }
        else {
            vals[nvals++] = EVGR_BEZIERTO;
            vals[nvals++] = px + ptanx;
            vals[nvals++] = py + ptany;
            vals[nvals++] = x - tanx;
            vals[nvals++] = y - tany;
            vals[nvals++] = x;
            vals[nvals++] = y;
        }
        px = x;
        py = y;
        ptanx = tanx;
        ptany = tany;
    }

    evgr__appendCommands(ctx, vals, nvals);
}

void evgrRect(EVGRcontext * ctx, float x, float y, float w, float h)
{
    float vals[] = {
        EVGR_MOVETO, x, y,
        EVGR_LINETO, x, y + h,
        EVGR_LINETO, x + w, y + h,
        EVGR_LINETO, x + w, y,
        EVGR_CLOSE
    };
    evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
}

void evgrRoundedRect(EVGRcontext * ctx, float x, float y, float w, float h, float r)
{
    evgrRoundedRectVarying(ctx, x, y, w, h, r, r, r, r);
}

void evgrRoundedRectVarying(EVGRcontext * ctx, float x, float y, float w, float h, float radTopLeft, float radTopRight,
                           float radBottomRight, float radBottomLeft)
{
    if(radTopLeft < 0.1f && radTopRight < 0.1f && radBottomRight < 0.1f && radBottomLeft < 0.1f) {
        evgrRect(ctx, x, y, w, h);
        return;
    }
    else {
        float halfw = evgr__absf(w) * 0.5f;
        float halfh = evgr__absf(h) * 0.5f;
        float rxBL = evgr__minf(radBottomLeft, halfw) * evgr__signf(w), ryBL = evgr__minf(radBottomLeft, halfh) * evgr__signf(h);
        float rxBR = evgr__minf(radBottomRight, halfw) * evgr__signf(w), ryBR = evgr__minf(radBottomRight, halfh) * evgr__signf(h);
        float rxTR = evgr__minf(radTopRight, halfw) * evgr__signf(w), ryTR = evgr__minf(radTopRight, halfh) * evgr__signf(h);
        float rxTL = evgr__minf(radTopLeft, halfw) * evgr__signf(w), ryTL = evgr__minf(radTopLeft, halfh) * evgr__signf(h);
        float vals[] = {
            EVGR_MOVETO, x, y + ryTL,
            EVGR_LINETO, x, y + h - ryBL,
            EVGR_BEZIERTO, x, y + h - ryBL * (1 - EVGR_KAPPA90), x + rxBL * (1 - EVGR_KAPPA90), y + h, x + rxBL, y + h,
            EVGR_LINETO, x + w - rxBR, y + h,
            EVGR_BEZIERTO, x + w - rxBR * (1 - EVGR_KAPPA90), y + h, x + w, y + h - ryBR * (1 - EVGR_KAPPA90), x + w, y + h - ryBR,
            EVGR_LINETO, x + w, y + ryTR,
            EVGR_BEZIERTO, x + w, y + ryTR * (1 - EVGR_KAPPA90), x + w - rxTR * (1 - EVGR_KAPPA90), y, x + w - rxTR, y,
            EVGR_LINETO, x + rxTL, y,
            EVGR_BEZIERTO, x + rxTL * (1 - EVGR_KAPPA90), y, x, y + ryTL * (1 - EVGR_KAPPA90), x, y + ryTL,
            EVGR_CLOSE
        };
        evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
    }
}

void evgrEllipse(EVGRcontext * ctx, float cx, float cy, float rx, float ry)
{
    float vals[] = {
        EVGR_MOVETO, cx - rx, cy,
        EVGR_BEZIERTO, cx - rx, cy + ry * EVGR_KAPPA90, cx - rx * EVGR_KAPPA90, cy + ry, cx, cy + ry,
        EVGR_BEZIERTO, cx + rx * EVGR_KAPPA90, cy + ry, cx + rx, cy + ry * EVGR_KAPPA90, cx + rx, cy,
        EVGR_BEZIERTO, cx + rx, cy - ry * EVGR_KAPPA90, cx + rx * EVGR_KAPPA90, cy - ry, cx, cy - ry,
        EVGR_BEZIERTO, cx - rx * EVGR_KAPPA90, cy - ry, cx - rx, cy - ry * EVGR_KAPPA90, cx - rx, cy,
        EVGR_CLOSE
    };
    evgr__appendCommands(ctx, vals, EVGR_COUNTOF(vals));
}

void evgrCircle(EVGRcontext * ctx, float cx, float cy, float r)
{
    evgrEllipse(ctx, cx, cy, r, r);
}

void evgrDebugDumpPathCache(EVGRcontext * ctx)
{
    const EVGRpath * path;
    int i, j;

    LV_LOG_USER("Dumping %d cached paths", ctx->cache->npaths);
    for(i = 0; i < ctx->cache->npaths; i++) {
        path = &ctx->cache->paths[i];
        LV_LOG_USER(" - Path %d", i);
        if(path->nfill) {
            LV_LOG_USER("   - fill: %d", path->nfill);
            for(j = 0; j < path->nfill; j++)
                LV_LOG_USER("%f\t%f", path->fill[j].x, path->fill[j].y);
        }
        if(path->nstroke) {
            LV_LOG_USER("   - stroke: %d", path->nstroke);
            for(j = 0; j < path->nstroke; j++)
                LV_LOG_USER("%f\t%f", path->stroke[j].x, path->stroke[j].y);
        }
    }
}

void evgrFill(EVGRcontext * ctx)
{
    EVGRstate * state = evgr__getState(ctx);
    const EVGRpath * path;
    EVGRpaint fillPaint = state->fill;
    int i;

    evgr__flattenPaths(ctx);
    if(ctx->params.edgeAntiAlias && state->shapeAntiAlias)
        evgr__expandFill(ctx, ctx->fringeWidth, EVGR_MITER, 2.4f);
    else
        evgr__expandFill(ctx, 0.0f, EVGR_MITER, 2.4f);

    // Apply global alpha
    fillPaint.innerColor.ch.a *= state->alpha;
    fillPaint.outerColor.ch.a *= state->alpha;

    ctx->params.renderFill(ctx->params.userPtr, &fillPaint, state->compositeOperation, &state->scissor, ctx->fringeWidth,
                           ctx->cache->bounds, ctx->cache->paths, ctx->cache->npaths);

    // Count triangles
    for(i = 0; i < ctx->cache->npaths; i++) {
        path = &ctx->cache->paths[i];
        ctx->fillTriCount += path->nfill - 2;
        ctx->fillTriCount += path->nstroke - 2;
        ctx->drawCallCount += 2;
    }
}

void evgrStroke(EVGRcontext * ctx)
{
    EVGRstate * state = evgr__getState(ctx);
    float scale = evgr__getAverageScale(state->xform);
    float strokeWidth = evgr__clampf(state->strokeWidth * scale, 0.0f, 200.0f);
    EVGRpaint strokePaint = state->stroke;
    const EVGRpath * path;
    int i;


    if(strokeWidth < ctx->fringeWidth) {
        // If the stroke width is less than pixel size, use alpha to emulate coverage.
        // Since coverage is area, scale by alpha*alpha.
        float alpha = evgr__clampf(strokeWidth / ctx->fringeWidth, 0.0f, 1.0f);
        strokePaint.innerColor.ch.a *= alpha * alpha;
        strokePaint.outerColor.ch.a *= alpha * alpha;
        strokeWidth = ctx->fringeWidth;
    }

    // Apply global alpha
    strokePaint.innerColor.ch.a *= state->alpha;
    strokePaint.outerColor.ch.a *= state->alpha;

    evgr__flattenPaths(ctx);

    if(ctx->params.edgeAntiAlias && state->shapeAntiAlias)
        evgr__expandStroke(ctx, strokeWidth * 0.5f, ctx->fringeWidth, state->lineCap, state->lineJoin, state->miterLimit);
    else
        evgr__expandStroke(ctx, strokeWidth * 0.5f, 0.0f, state->lineCap, state->lineJoin, state->miterLimit);

    ctx->params.renderStroke(ctx->params.userPtr, &strokePaint, state->compositeOperation, &state->scissor,
                             ctx->fringeWidth,
                             strokeWidth, ctx->cache->paths, ctx->cache->npaths);

    // Count triangles
    for(i = 0; i < ctx->cache->npaths; i++) {
        path = &ctx->cache->paths[i];
        ctx->strokeTriCount += path->nstroke - 2;
        ctx->drawCallCount++;
    }
}

// Add fonts
int evgrCreateFont(EVGRcontext * ctx, const char * name, const char * filename)
{
    (void)ctx;
    (void)name;
    (void)filename;
    return -1;
}

int evgrCreateFontAtIndex(EVGRcontext * ctx, const char * name, const char * filename, const int fontIndex)
{
    (void)ctx;
    (void)name;
    (void)filename;
    (void)fontIndex;
    return -1;
}

int evgrCreateFontMem(EVGRcontext * ctx, const char * name, unsigned char * data, int ndata, int freeData)
{
    (void)ctx;
    (void)name;
    (void)data;
    (void)ndata;
    (void)freeData;
    return -1;
}

int evgrCreateFontMemAtIndex(EVGRcontext * ctx, const char * name, unsigned char * data, int ndata, int freeData,
                            const int fontIndex)
{
    (void)ctx;
    (void)name;
    (void)data;
    (void)ndata;
    (void)freeData;
    (void)fontIndex;
    return -1;
}

int evgrFindFont(EVGRcontext * ctx, const char * name)
{
    (void)ctx;
    (void)name;
    return -1;
}


int evgrAddFallbackFontId(EVGRcontext * ctx, int baseFont, int fallbackFont)
{
    (void)ctx;
    (void)baseFont;
    (void)fallbackFont;
    return 0;
}

int evgrAddFallbackFont(EVGRcontext * ctx, const char * baseFont, const char * fallbackFont)
{
    (void)ctx;
    (void)baseFont;
    (void)fallbackFont;
    return 0;
}

void evgrResetFallbackFontsId(EVGRcontext * ctx, int baseFont)
{
    (void)ctx;
    (void)baseFont;
}

void evgrResetFallbackFonts(EVGRcontext * ctx, const char * baseFont)
{
    (void)ctx;
    (void)baseFont;
}

// State setting
void evgrFontSize(EVGRcontext * ctx, float size)
{
    EVGRstate * state = evgr__getState(ctx);
    state->fontSize = size;
}

void evgrFontBlur(EVGRcontext * ctx, float blur)
{
    EVGRstate * state = evgr__getState(ctx);
    state->fontBlur = blur;
}

void evgrTextLetterSpacing(EVGRcontext * ctx, float spacing)
{
    EVGRstate * state = evgr__getState(ctx);
    state->letterSpacing = spacing;
}

void evgrTextLineHeight(EVGRcontext * ctx, float lineHeight)
{
    EVGRstate * state = evgr__getState(ctx);
    state->lineHeight = lineHeight;
}

void evgrTextAlign(EVGRcontext * ctx, int align)
{
    EVGRstate * state = evgr__getState(ctx);
    state->textAlign = align;
}

void evgrFontFaceId(EVGRcontext * ctx, int font)
{
    EVGRstate * state = evgr__getState(ctx);
    state->fontId = font;
}

void evgrFontFace(EVGRcontext * ctx, const char * font)
{
    (void)ctx;
    (void)font;
}

float evgrText(EVGRcontext * ctx, float x, float y, const char * string, const char * end)
{
    (void)ctx;
    (void)x;
    (void)y;
    (void)string;
    (void)end;
    return 0;
}

void evgrTextBox(EVGRcontext * ctx, float x, float y, float breakRowWidth, const char * string, const char * end)
{
    (void)ctx;
    (void)x;
    (void)y;
    (void)breakRowWidth;
    (void)string;
    (void)end;
}

int evgrTextBreakLines(EVGRcontext * ctx, const char * string, const char * end, float breakRowWidth, EVGRtextRow * rows,
                      int maxRows)
{
    (void)ctx;
    (void)string;
    (void)end;
    (void)breakRowWidth;
    (void)rows;
    (void)maxRows;
    return 0;
}

float evgrTextBounds(EVGRcontext * ctx, float x, float y, const char * string, const char * end, float * bounds)
{
    (void)ctx;
    (void)x;
    (void)y;
    (void)string;
    (void)end;
    (void)bounds;
    return 0;
}

void evgrTextBoxBounds(EVGRcontext * ctx, float x, float y, float breakRowWidth, const char * string, const char * end,
                      float * bounds)
{
    (void)ctx;
    (void)x;
    (void)y;
    (void)breakRowWidth;
    (void)string;
    (void)end;
    (void)bounds;
}

void evgrTextMetrics(EVGRcontext * ctx, float * ascender, float * descender, float * lineh)
{
    (void)ctx;
    (void)ascender;
    (void)descender;
    (void)lineh;
}
// vim: ft=c nu noet ts=4

#endif /* LV_USE_DRAW_EVGPU */
