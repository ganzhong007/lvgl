//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
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
#ifndef EVGPU_EVGR_GL_H
#define EVGPU_EVGR_GL_H

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

#ifdef __cplusplus
extern "C" {
#endif

// Create flags

enum EVGRcreateFlags {
    // Flag indicating if geometry based anti-aliasing is used (may not be needed when using MSAA).
    EVGR_ANTIALIAS       = 1 << 0,
    // Flag indicating if strokes should be drawn using stencil buffer. The rendering will be a little
    // slower, but path overlaps (i.e. self-intersecting or sharp turns) will be drawn just once.
    EVGR_STENCIL_STROKES = 1 << 1,
    // Flag indicating that additional debug checks are done.
    EVGR_DEBUG           = 1 << 2,
};

#if defined EVGR_GL2_IMPLEMENTATION
#  define EVGR_GL2 1
#  define EVGR_GL_IMPLEMENTATION 1
#  define EVGR_GL_USE_UNIFORMBUFFER 0
#elif defined EVGR_GL3_IMPLEMENTATION
#  define EVGR_GL3 1
#  define EVGR_GL_IMPLEMENTATION 1
#  define EVGR_GL_USE_UNIFORMBUFFER 1
#elif defined EVGR_GLES2_IMPLEMENTATION
#  define EVGR_GLES2 1
#  define EVGR_GL_IMPLEMENTATION 1
#  define EVGR_GL_USE_UNIFORMBUFFER 0
#elif defined EVGR_GLES3_IMPLEMENTATION
#  define EVGR_GLES3 1
#  define EVGR_GL_IMPLEMENTATION 1
#  define EVGR_GL_USE_UNIFORMBUFFER 0
#endif

#define EVGR_GL_USE_STATE_FILTER (1)

// Creates NanoVG contexts for different OpenGL (ES) versions.
// Flags should be combination of the create flags above.

#if defined EVGR_GL2

EVGRcontext * evgrCreateGL2(int flags);
void evgrDeleteGL2(EVGRcontext * ctx);

int evgrlCreateImageFromHandleGL2(EVGRcontext * ctx, GLuint textureId, int w, int h, int flags);
GLuint evgrlImageHandleGL2(EVGRcontext * ctx, int image);

#endif

#if defined EVGR_GL3

EVGRcontext * evgrCreateGL3(int flags);
void evgrDeleteGL3(EVGRcontext * ctx);

int evgrlCreateImageFromHandleGL3(EVGRcontext * ctx, GLuint textureId, int w, int h, int flags);
GLuint evgrlImageHandleGL3(EVGRcontext * ctx, int image);

#endif

#if defined EVGR_GLES2

EVGRcontext * evgrCreateGLES2(int flags);
void evgrDeleteGLES2(EVGRcontext * ctx);

int evgrlCreateImageFromHandleGLES2(EVGRcontext * ctx, GLuint textureId, int w, int h, int flags);
GLuint evgrlImageHandleGLES2(EVGRcontext * ctx, int image);

#endif

#if defined EVGR_GLES3

EVGRcontext * evgrCreateGLES3(int flags);
void evgrDeleteGLES3(EVGRcontext * ctx);

int evgrlCreateImageFromHandleGLES3(EVGRcontext * ctx, GLuint textureId, int w, int h, int flags);
GLuint evgrlImageHandleGLES3(EVGRcontext * ctx, int image);

#endif

// These are additional flags on top of EVGRimageFlags.
enum EVGRimageFlagsGL {
    EVGR_IMAGE_NODELETE          = 1 << 16,  // Do not delete GL texture handle.
};

#ifdef __cplusplus
}
#endif

#endif /* EVGPU_EVGR_GL_H */

#ifdef EVGR_GL_IMPLEMENTATION

#include <math.h>
#include "evgpu_evgr.h"

enum GLEVGRuniformLoc {
    GLEVGR_LOC_VIEWSIZE,
    GLEVGR_LOC_TEX,
    GLEVGR_LOC_FRAG,
    GLEVGR_MAX_LOCS
};

enum GLEVGRshaderType {
    NSVG_SHADER_FILLGRAD,
    NSVG_SHADER_FILLIMG,
    NSVG_SHADER_SIMPLE,
    NSVG_SHADER_IMG
};

#define GLEVGR_SHADER_COUNT 4

#if EVGR_GL_USE_UNIFORMBUFFER
enum GLEVGRuniformBindings {
    GLEVGR_FRAG_BINDING = 0,
};
#endif

struct GLEVGRshader {
    GLuint prog;
    GLuint frag;
    GLuint vert;
    GLint loc[GLEVGR_MAX_LOCS];
};
typedef struct GLEVGRshader GLEVGRshader;

struct GLEVGRtexture {
    int id;
    GLuint tex;
    int width, height;
    int type;
    int flags;
};
typedef struct GLEVGRtexture GLEVGRtexture;

struct GLEVGRblend {
    GLenum srcRGB;
    GLenum dstRGB;
    GLenum srcAlpha;
    GLenum dstAlpha;
};
typedef struct GLEVGRblend GLEVGRblend;

enum GLEVGRcallType {
    GLEVGR_NONE = 0,
    GLEVGR_FILL,
    GLEVGR_CONVEXFILL,
    GLEVGR_STROKE,
    GLEVGR_TRIANGLES,
};

struct GLEVGRcall {
    int type;
    int image;
    int pathOffset;
    int pathCount;
    int triangleOffset;
    int triangleCount;
    int uniformOffset;
    int shaderType;
    GLEVGRblend blendFunc;
};
typedef struct GLEVGRcall GLEVGRcall;

struct GLEVGRpath {
    int fillOffset;
    int fillCount;
    int strokeOffset;
    int strokeCount;
};
typedef struct GLEVGRpath GLEVGRpath;

#if EVGR_GL_USE_UNIFORMBUFFER
struct GLEVGRfragUniforms {
    struct {
        float scissorMat[12]; // matrices are actually 3 vec4s
        float paintMat[12];
        union EVGRcolor innerCol;
        union EVGRcolor outerCol;
        float scissorExt[2];
        float scissorScale[2];
        float extent[2];
        float radius;
        float feather;
        float strokeMult;
        float strokeThr;
        int texType;
        int type;
        union EVGRcolor recolor;
    } s;
};
typedef struct GLEVGRfragUniforms GLEVGRfragUniforms;
#else
// note: after modifying layout or size of uniform array,
// don't forget to also update the fragment shader source!
#define EVGR_GL_UNIFORMARRAY_SIZE 12
union GLEVGRfragUniforms {
    struct {
        float scissorMat[12]; // matrices are actually 3 vec4s
        float paintMat[12];
        union EVGRcolor innerCol;
        union EVGRcolor outerCol;
        float scissorExt[2];
        float scissorScale[2];
        float extent[2];
        float radius;
        float feather;
        float strokeMult;
        float strokeThr;
        float texType;
        float type;
        union EVGRcolor recolor;
    } s;
    float uniformArray[EVGR_GL_UNIFORMARRAY_SIZE][4];
};
typedef union GLEVGRfragUniforms GLEVGRfragUniforms;
#endif

struct GLEVGRcontext {
    GLEVGRshader shaders[GLEVGR_SHADER_COUNT];
    GLEVGRtexture * textures;
    float view[2];
    int ntextures;
    int ctextures;
    int textureId;
    GLuint vertBuf[2];
    int vertBufIndex;
#if defined EVGR_GL3
    GLuint vertArr;
#endif
#if EVGR_GL_USE_UNIFORMBUFFER
    GLuint fragBuf;
#endif
    int fragSize;
    int flags;
    int boundShader;

    // Per frame buffers
    GLEVGRcall * calls;
    int ccalls;
    int ncalls;
    GLEVGRpath * paths;
    int cpaths;
    int npaths;
    struct EVGRvertex * verts;
    int cverts;
    int nverts;
    unsigned char * uniforms;
    int cuniforms;
    int nuniforms;

    // cached state
#if EVGR_GL_USE_STATE_FILTER
    GLuint boundTexture;
    GLuint stencilMask;
    GLenum stencilFunc;
    GLint stencilFuncRef;
    GLuint stencilFuncMask;
    GLEVGRblend blendFunc;
#endif

    int dummyTex;
};
typedef struct GLEVGRcontext GLEVGRcontext;

static int glevgr__maxi(int a, int b)
{
    return a > b ? a : b;
}

#ifdef EVGR_GLES2
static unsigned int glevgr__nearestPow2(unsigned int num)
{
    unsigned n = num > 0 ? num - 1 : 0;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
#endif

static void glevgr__bindTexture(GLEVGRcontext * gl, GLuint tex)
{
#if EVGR_GL_USE_STATE_FILTER
    if(gl->boundTexture != tex) {
        gl->boundTexture = tex;
        glBindTexture(GL_TEXTURE_2D, tex);
    }
#else
    glBindTexture(GL_TEXTURE_2D, tex);
#endif
}

static void glevgr__stencilMask(GLEVGRcontext * gl, GLuint mask)
{
#if EVGR_GL_USE_STATE_FILTER
    if(gl->stencilMask != mask) {
        gl->stencilMask = mask;
        glStencilMask(mask);
    }
#else
    glStencilMask(mask);
#endif
}

static void glevgr__stencilFunc(GLEVGRcontext * gl, GLenum func, GLint ref, GLuint mask)
{
#if EVGR_GL_USE_STATE_FILTER
    if((gl->stencilFunc != func) ||
       (gl->stencilFuncRef != ref) ||
       (gl->stencilFuncMask != mask)) {

        gl->stencilFunc = func;
        gl->stencilFuncRef = ref;
        gl->stencilFuncMask = mask;
        glStencilFunc(func, ref, mask);
    }
#else
    glStencilFunc(func, ref, mask);
#endif
}
static void glevgr__blendFuncSeparate(GLEVGRcontext * gl, const GLEVGRblend * blend)
{
#if EVGR_GL_USE_STATE_FILTER
    if((gl->blendFunc.srcRGB != blend->srcRGB) ||
       (gl->blendFunc.dstRGB != blend->dstRGB) ||
       (gl->blendFunc.srcAlpha != blend->srcAlpha) ||
       (gl->blendFunc.dstAlpha != blend->dstAlpha)) {

        gl->blendFunc = *blend;
        glBlendFuncSeparate(blend->srcRGB, blend->dstRGB, blend->srcAlpha, blend->dstAlpha);
    }
#else
    glBlendFuncSeparate(blend->srcRGB, blend->dstRGB, blend->srcAlpha, blend->dstAlpha);
#endif
}

static GLEVGRtexture * glevgr__allocTexture(GLEVGRcontext * gl)
{
    GLEVGRtexture * tex = NULL;
    int i;

    for(i = 0; i < gl->ntextures; i++) {
        if(gl->textures[i].id == 0) {
            tex = &gl->textures[i];
            break;
        }
    }
    if(tex == NULL) {
        if(gl->ntextures + 1 > gl->ctextures) {
            GLEVGRtexture * textures;
            int ctextures = glevgr__maxi(gl->ntextures + 1, 4) +  gl->ctextures / 2; // 1.5x Overallocate
            textures = (GLEVGRtexture *)lv_realloc(gl->textures, sizeof(GLEVGRtexture) * ctextures);
            if(textures == NULL) return NULL;
            gl->textures = textures;
            gl->ctextures = ctextures;
        }
        tex = &gl->textures[gl->ntextures++];
    }

    lv_memzero(tex, sizeof(*tex));
    tex->id = ++gl->textureId;

    return tex;
}

static GLEVGRtexture * glevgr__findTexture(GLEVGRcontext * gl, int id)
{
    int i;
    for(i = 0; i < gl->ntextures; i++)
        if(gl->textures[i].id == id)
            return &gl->textures[i];
    return NULL;
}

static int glevgr__deleteTexture(GLEVGRcontext * gl, int id)
{
    int i;
    for(i = 0; i < gl->ntextures; i++) {
        if(gl->textures[i].id == id) {
            if(gl->textures[i].tex != 0 && (gl->textures[i].flags & EVGR_IMAGE_NODELETE) == 0)
                glDeleteTextures(1, &gl->textures[i].tex);
            lv_memzero(&gl->textures[i], sizeof(gl->textures[i]));
            return 1;
        }
    }
    return 0;
}

static void glevgr__dumpShaderError(GLuint shader, const char * name, const char * type)
{
    GLchar str[512 + 1];
    GLsizei len = 0;
    glGetShaderInfoLog(shader, 512, &len, str);
    if(len > 512) len = 512;
    str[len] = '\0';
    LV_LOG_ERROR("Shader %s/%s error:\n%s", name, type, str);
}

static void glevgr__dumpProgramError(GLuint prog, const char * name)
{
    GLchar str[512 + 1];
    GLsizei len = 0;
    glGetProgramInfoLog(prog, 512, &len, str);
    if(len > 512) len = 512;
    str[len] = '\0';
    LV_LOG_ERROR("Program %s error:\n%s", name, str);
}

static void glevgr__checkError(GLEVGRcontext * gl, const char * str)
{
    GLenum err;
    if((gl->flags & EVGR_DEBUG) == 0) return;
    err = glGetError();
    if(err != GL_NO_ERROR) {
        LV_LOG_ERROR("Error %08x after %s", err, str);
        return;
    }
}

static int glevgr__createShader(GLEVGRshader * shader, const char * name, const char * header, const char * opts,
                               const char * vshader, const char * fshader)
{
    GLint status;
    GLuint prog, vert, frag;
    const char * str[3];
    str[0] = header;
    str[1] = opts != NULL ? opts : "";

    lv_memzero(shader, sizeof(*shader));

    prog = glCreateProgram();
    vert = glCreateShader(GL_VERTEX_SHADER);
    frag = glCreateShader(GL_FRAGMENT_SHADER);
    str[2] = vshader;
    glShaderSource(vert, 3, str, 0);
    str[2] = fshader;
    glShaderSource(frag, 3, str, 0);

    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE) {
        glevgr__dumpShaderError(vert, name, "vert");
        return 0;
    }

    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE) {
        glevgr__dumpShaderError(frag, name, "frag");
        return 0;
    }

    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    glBindAttribLocation(prog, 0, "vertex");
    glBindAttribLocation(prog, 1, "tcoord");

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if(status != GL_TRUE) {
        glevgr__dumpProgramError(prog, name);
        return 0;
    }

    shader->prog = prog;
    shader->vert = vert;
    shader->frag = frag;

    return 1;
}

static void glevgr__deleteShader(GLEVGRshader * shader)
{
    if(shader->prog != 0)
        glDeleteProgram(shader->prog);
    if(shader->vert != 0)
        glDeleteShader(shader->vert);
    if(shader->frag != 0)
        glDeleteShader(shader->frag);
}

static void glevgr__getUniforms(GLEVGRshader * shader)
{
    shader->loc[GLEVGR_LOC_VIEWSIZE] = glGetUniformLocation(shader->prog, "viewSize");
    shader->loc[GLEVGR_LOC_TEX] = glGetUniformLocation(shader->prog, "tex");

#if EVGR_GL_USE_UNIFORMBUFFER
    shader->loc[GLEVGR_LOC_FRAG] = glGetUniformBlockIndex(shader->prog, "frag");
#else
    shader->loc[GLEVGR_LOC_FRAG] = glGetUniformLocation(shader->prog, "frag");
#endif
}

static int glevgr__renderCreateTexture(void * uptr, int type, int w, int h, int imageFlags, const unsigned char * data);

static int glevgr__renderCreate(void * uptr)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    int align = 4;

    // TODO: mediump float may not be enough for GLES2 in iOS.
    // see the following discussion: https://github.com/memononen/nanovg/issues/46
    static const char * shaderHeader =
#if defined EVGR_GL2
        "#define EVGR_GL2 1\n"
#elif defined EVGR_GL3
        "#version 150 core\n"
        "#define EVGR_GL3 1\n"
#elif defined EVGR_GLES2
        "#version 100\n"
        "#define EVGR_GL2 1\n"
#elif defined EVGR_GLES3
        "#version 300 es\n"
        "#define EVGR_GL3 1\n"
#endif

#if EVGR_GL_USE_UNIFORMBUFFER
        "#define USE_UNIFORMBUFFER 1\n"
#else
        "#define UNIFORMARRAY_SIZE 12\n"
#endif
        "\n";

    static const char * fillVertShader =
        "#ifdef GL_ES\n"
        "#if defined(EVGR_GL3)\n"
        " precision highp float;\n"
        "#else\n"
        " precision mediump float;\n"
        "#endif\n"
        "#endif\n"
        "#ifdef EVGR_GL3\n"
        "	uniform vec2 viewSize;\n"
        "	in vec2 vertex;\n"
        "	in vec2 tcoord;\n"
        "	out vec2 ftcoord;\n"
        "	out vec2 fpos;\n"
        "	out vec2 v_scissorPos;\n"
        "	out vec2 v_paintPos;\n"
        "#else\n"
        "	uniform vec2 viewSize;\n"
        "	attribute vec2 vertex;\n"
        "	attribute vec2 tcoord;\n"
        "	varying vec2 ftcoord;\n"
        "	varying vec2 fpos;\n"
        "	varying vec2 v_scissorPos;\n"
        "	varying vec2 v_paintPos;\n"
        "#endif\n"
        "#ifdef EVGR_GL3\n"
        "#ifdef USE_UNIFORMBUFFER\n"
        "	layout(std140) uniform frag {\n"
        "		mat3 scissorMat;\n"
        "		mat3 paintMat;\n"
        "		vec4 innerCol;\n"
        "		vec4 outerCol;\n"
        "		vec2 scissorExt;\n"
        "		vec2 scissorScale;\n"
        "		vec2 extent;\n"
        "		float radius;\n"
        "		float feather;\n"
        "		float strokeMult;\n"
        "		float strokeThr;\n"
        "		int texType;\n"
        "		int type;\n"
        "		vec4 recolor;\n"
        "	};\n"
        "#else\n"
        "	uniform vec4 frag[UNIFORMARRAY_SIZE];\n"
        "#endif\n"
        "#else\n"
        "	uniform vec4 frag[UNIFORMARRAY_SIZE];\n"
        "#endif\n"
        "#ifndef USE_UNIFORMBUFFER\n"
        "	#define scissorMat mat3(frag[0].xyz, frag[1].xyz, frag[2].xyz)\n"
        "	#define paintMat mat3(frag[3].xyz, frag[4].xyz, frag[5].xyz)\n"
        "#endif\n"
        "void main(void) {\n"
        "	ftcoord = tcoord;\n"
        "	fpos = vertex;\n"
        "	gl_Position = vec4(2.0*vertex.x/viewSize.x - 1.0, 1.0 - 2.0*vertex.y/viewSize.y, 0, 1);\n"
        "	#if SHADER_TYPE != 2\n" // Not SIMPLE
        "		v_scissorPos = (scissorMat * vec3(vertex, 1.0)).xy;\n"
        "	#endif\n"
        "	#if SHADER_TYPE == 0 || SHADER_TYPE == 1\n" // FILLGRAD or FILLIMG
        "		v_paintPos = (paintMat * vec3(vertex, 1.0)).xy;\n"
        "	#endif\n"
        "}\n";

    static const char * fillFragShader =
        "#ifdef GL_ES\n"
        "#if defined(EVGR_GL3)\n"
        " precision highp float;\n"
        "#else\n"
        " precision mediump float;\n"
        "#endif\n"
        "#endif\n"
        "#ifdef EVGR_GL3\n"
        "#ifdef USE_UNIFORMBUFFER\n"
        "	layout(std140) uniform frag {\n"
        "		mat3 scissorMat;\n"
        "		mat3 paintMat;\n"
        "		vec4 innerCol;\n"
        "		vec4 outerCol;\n"
        "		vec2 scissorExt;\n"
        "		vec2 scissorScale;\n"
        "		vec2 extent;\n"
        "		float radius;\n"
        "		float feather;\n"
        "		float strokeMult;\n"
        "		float strokeThr;\n"
        "		int texType;\n"
        "		int type;\n"
        "		vec4 recolor;\n"
        "	};\n"
        "#else\n" // EVGR_GL3 && !USE_UNIFORMBUFFER
        "	uniform vec4 frag[UNIFORMARRAY_SIZE];\n"
        "#endif\n"
        "	uniform sampler2D tex;\n"
        "	in vec2 ftcoord;\n"
        "	in vec2 fpos;\n"
        "	in vec2 v_scissorPos;\n"
        "	in vec2 v_paintPos;\n"
        "	out vec4 outColor;\n"
        "#else\n" // !EVGR_GL3
        "	uniform vec4 frag[UNIFORMARRAY_SIZE];\n"
        "	uniform sampler2D tex;\n"
        "	varying vec2 ftcoord;\n"
        "	varying vec2 fpos;\n"
        "	varying vec2 v_scissorPos;\n"
        "	varying vec2 v_paintPos;\n"
        "#endif\n"
        "#ifndef USE_UNIFORMBUFFER\n"
        "	#define scissorMat mat3(frag[0].xyz, frag[1].xyz, frag[2].xyz)\n"
        "	#define paintMat mat3(frag[3].xyz, frag[4].xyz, frag[5].xyz)\n"
        "	#define innerCol frag[6]\n"
        "	#define outerCol frag[7]\n"
        "	#define scissorExt frag[8].xy\n"
        "	#define scissorScale frag[8].zw\n"
        "	#define extent frag[9].xy\n"
        "	#define radius frag[9].z\n"
        "	#define feather frag[9].w\n"
        "	#define strokeMult frag[10].x\n"
        "	#define strokeThr frag[10].y\n"
        "	#define texType int(frag[10].z)\n"
        "	#define type int(frag[10].w)\n"
        "	#define recolor frag[11]\n"
        "#endif\n"
        "\n"
        "float sdroundrect(vec2 pt, vec2 ext, float rad) {\n"
        "	vec2 ext2 = ext - vec2(rad,rad);\n"
        "	vec2 d = abs(pt) - ext2;\n"
        "	return min(max(d.x,d.y),0.0) + length(max(d,0.0)) - rad;\n"
        "}\n"
        "\n"
        "// Scissoring\n"
        "float scissorMask(vec2 p) {\n"
        "	vec2 sc = (abs(p) - scissorExt);\n"
        "	sc = vec2(0.5,0.5) - sc * scissorScale;\n"
        "	return clamp(sc.x,0.0,1.0) * clamp(sc.y,0.0,1.0);\n"
        "}\n"
        "#ifdef EDGE_AA\n"
        "// Stroke - from [0..1] to clipped pyramid, where the slope is 1px.\n"
        "float strokeMask() {\n"
        "	return min(1.0, (1.0-abs(ftcoord.x*2.0-1.0))*strokeMult) * min(1.0, ftcoord.y);\n"
        "}\n"
        "#endif\n"
        "\n"
        "void main(void) {\n"
        "   vec4 result;\n"
        "	#if SHADER_TYPE != 2\n" // Not SIMPLE
        "		float scissor = scissorMask(v_scissorPos);\n"
        "	#endif\n"
        "#ifdef EDGE_AA\n"
        "	float strokeAlpha = strokeMask();\n"
        "	// if (strokeAlpha < strokeThr) discard;\n"
        "#else\n"
        "	float strokeAlpha = 1.0;\n"
        "#endif\n"
        "	#if SHADER_TYPE == 0\n"           // Gradient
        "		// Calculate gradient color using box gradient\n"
        "		vec2 pt = v_paintPos;\n"
        "		float d = clamp((sdroundrect(pt, extent, radius) + feather*0.5) / feather, 0.0, 1.0);\n"
        "		vec4 color = mix(innerCol,outerCol,d);\n"
        "		// Combine alpha\n"
        "		color *= strokeAlpha * scissor;\n"
        "		result = color;\n"
        "	#elif SHADER_TYPE == 1\n"     // Image
        "		// Calculate color fron texture\n"
        "		vec2 pt = v_paintPos / extent;\n"
        "#ifdef EVGR_GL3\n"
        "		vec4 color = texture(tex, pt);\n"
        "#else\n"
        "		vec4 color = texture2D(tex, pt);\n"
        "#endif\n"
        "		if (texType == 1) color = vec4(color.xyz*color.w,color.w);"
        "		else if (texType == 2) color = vec4(color.x);"
        "		else if (texType == 3) color.rgb = color.bgr;"  // BGR -> RGB swizzle (premultiplied)
        "		else if (texType == 4) color = vec4(color.bgr, 1.0);"  // BGRX -> RGB with alpha=1
        "		else if (texType == 5) color = vec4(color.bgr*color.a, color.a);"  // BGR swizzle + premultiply
        "		// Apply image recolor (premultiplied-aware) before tint/opacity.\n"
        "		if (recolor.a > 0.0) color.rgb = mix(color.rgb, recolor.rgb*color.a, recolor.a);\n"
        "		// Apply color tint and alpha.\n"
        "		color *= innerCol;\n"
        "		// Combine alpha\n"
        "		color *= strokeAlpha * scissor;\n"
        "		result = color;\n"
        "	#elif SHADER_TYPE == 2\n"     // Stencil fill
        "		result = vec4(1,1,1,1);\n"
        "	#elif SHADER_TYPE == 3\n"     // Textured tris
        "#ifdef EVGR_GL3\n"
        "		vec4 color = texture(tex, ftcoord);\n"
        "#else\n"
        "		vec4 color = texture2D(tex, ftcoord);\n"
        "#endif\n"
        "		if (texType == 1) color = vec4(color.xyz*color.w,color.w);"
        "		else if (texType == 2) color = vec4(color.x);"
        "		else if (texType == 3) color.rgb = color.bgr;"  // BGR -> RGB swizzle (premultiplied)
        "		else if (texType == 4) color = vec4(color.bgr, 1.0);"  // BGRX -> RGB with alpha=1
        "		else if (texType == 5) color = vec4(color.bgr*color.a, color.a);"  // BGR swizzle + premultiply
        "		color *= scissor;\n"
        "		result = color * innerCol;\n"
        "	#endif\n"
        "#ifdef EVGR_GL3\n"
        "	outColor = result;\n"
        "#else\n"
        "	gl_FragColor = result;\n"
        "#endif\n"
        "}\n";

    glevgr__checkError(gl, "init");

    int i;
    char opts[64];
    for(i = 0; i < GLEVGR_SHADER_COUNT; i++) {
        lv_snprintf(opts, sizeof(opts), "#define SHADER_TYPE %d\n%s", i,
                    (gl->flags & EVGR_ANTIALIAS) ? "#define EDGE_AA 1\n" : "");
        if(glevgr__createShader(&gl->shaders[i], "shader", shaderHeader, opts, fillVertShader, fillFragShader) == 0)
            return 0;
        glevgr__checkError(gl, "uniform locations");
        glevgr__getUniforms(&gl->shaders[i]);
    }

    // Create dynamic vertex array
#if defined EVGR_GL3
    glGenVertexArrays(1, &gl->vertArr);
#endif
    glGenBuffers(2, gl->vertBuf);

#if EVGR_GL_USE_UNIFORMBUFFER
    // Create UBOs
    glUniformBlockBinding(gl->shaders[0].prog, gl->shaders[0].loc[GLEVGR_LOC_FRAG], GLEVGR_FRAG_BINDING);
    glGenBuffers(1, &gl->fragBuf);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
#endif
    gl->fragSize = sizeof(GLEVGRfragUniforms) + align - sizeof(GLEVGRfragUniforms) % align;

    // Some platforms does not allow to have samples to unset textures.
    // Create empty one which is bound when there's no texture specified.
    gl->dummyTex = glevgr__renderCreateTexture(gl, EVGR_TEXTURE_ALPHA, 1, 1, 0, NULL);

    glevgr__checkError(gl, "create done");

    glFinish();

    return 1;
}

static int glevgr__renderCreateTexture(void * uptr, int type, int w, int h, int imageFlags, const unsigned char * data)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    GLEVGRtexture * tex = glevgr__allocTexture(gl);

    if(tex == NULL) return 0;

#ifdef EVGR_GLES2
    // Check for non-power of 2.
    if(glevgr__nearestPow2(w) != (unsigned int)w || glevgr__nearestPow2(h) != (unsigned int)h) {
        // No repeat
        if((imageFlags & EVGR_IMAGE_REPEATX) != 0 || (imageFlags & EVGR_IMAGE_REPEATY) != 0) {
            LV_LOG_WARN("Repeat X/Y is not supported for non power-of-two textures (%d x %d)", w, h);
            imageFlags &= ~(EVGR_IMAGE_REPEATX | EVGR_IMAGE_REPEATY);
        }
        // No mips.
        if(imageFlags & EVGR_IMAGE_GENERATE_MIPMAPS) {
            LV_LOG_WARN("Mip-maps is not support for non power-of-two textures (%d x %d)", w, h);
            imageFlags &= ~EVGR_IMAGE_GENERATE_MIPMAPS;
        }
    }
#endif

    glGenTextures(1, &tex->tex);
    tex->width = w;
    tex->height = h;
    tex->type = type;
    tex->flags = imageFlags;
    glevgr__bindTexture(gl, tex->tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#ifndef EVGR_GLES2
    glPixelStorei(GL_UNPACK_ROW_LENGTH, tex->width);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
#endif

#if defined (EVGR_GL2)
    // GL 1.4 and later has support for generating mipmaps using a tex parameter.
    if(imageFlags & EVGR_IMAGE_GENERATE_MIPMAPS) {
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    }
#endif

    if(type == EVGR_TEXTURE_BGRA || type == EVGR_TEXTURE_BGRX)
        /* BGRA/BGRX: upload as RGBA, shader will swizzle BGR->RGB */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    else if(type == EVGR_TEXTURE_RGBA)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    else if(type == EVGR_TEXTURE_BGR)
        /* BGR888: upload as RGB, shader will swizzle BGR->RGB */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    else if(type == EVGR_TEXTURE_RGB565)
        /* RGB565: directly compatible with GL */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
    else
#if defined(EVGR_GLES2) || defined (EVGR_GL2)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
#elif defined(EVGR_GLES3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
#else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
#endif

    if(imageFlags & EVGR_IMAGE_GENERATE_MIPMAPS) {
        if(imageFlags & EVGR_IMAGE_NEAREST) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        }
        else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }
    }
    else {
        if(imageFlags & EVGR_IMAGE_NEAREST) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
        else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    }

    if(imageFlags & EVGR_IMAGE_NEAREST) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if(imageFlags & EVGR_IMAGE_REPEATX)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

    if(imageFlags & EVGR_IMAGE_REPEATY)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#ifndef EVGR_GLES2
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
#endif

    // The new way to build mipmaps on GLES and GL3
#if !defined(EVGR_GL2)
    if(imageFlags & EVGR_IMAGE_GENERATE_MIPMAPS) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
#endif

    glevgr__checkError(gl, "create tex");
    glevgr__bindTexture(gl, 0);

    return tex->id;
}


static int glevgr__renderDeleteTexture(void * uptr, int image)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    return glevgr__deleteTexture(gl, image);
}

static int glevgr__renderUpdateTexture(void * uptr, int image, int x, int y, int w, int h, const unsigned char * data)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    GLEVGRtexture * tex = glevgr__findTexture(gl, image);

    if(tex == NULL) return 0;
    glevgr__bindTexture(gl, tex->tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

#ifndef EVGR_GLES2
    glPixelStorei(GL_UNPACK_ROW_LENGTH, tex->width);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, y);
#else
    // No support for all of skip, need to update a whole row at a time.
    if(tex->type == EVGR_TEXTURE_BGRA || tex->type == EVGR_TEXTURE_RGBA || tex->type == EVGR_TEXTURE_BGRX)
        data += y * tex->width * 4;
    else if(tex->type == EVGR_TEXTURE_BGR)
        data += y * tex->width * 3;
    else if(tex->type == EVGR_TEXTURE_RGB565)
        data += y * tex->width * 2;
    else
        data += y * tex->width;
    x = 0;
    w = tex->width;
#endif

    if(tex->type == EVGR_TEXTURE_BGRA || tex->type == EVGR_TEXTURE_BGRX)
        /* BGRA/BGRX: upload as RGBA, shader will swizzle BGR->RGB */
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
    else if(tex->type == EVGR_TEXTURE_RGBA)
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
    else if(tex->type == EVGR_TEXTURE_BGR)
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
    else if(tex->type == EVGR_TEXTURE_RGB565)
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
    else
#if defined(EVGR_GLES2) || defined(EVGR_GL2)
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
#else
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RED, GL_UNSIGNED_BYTE, data);
#endif

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#ifndef EVGR_GLES2
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
#endif

    glevgr__bindTexture(gl, 0);

    return 1;
}

static int glevgr__renderGetTextureSize(void * uptr, int image, int * w, int * h)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    GLEVGRtexture * tex = glevgr__findTexture(gl, image);
    if(tex == NULL) return 0;
    *w = tex->width;
    *h = tex->height;
    return 1;
}

static void glevgr__xformToMat3x4(float * m3, float * t)
{
    m3[0] = t[0];
    m3[1] = t[1];
    m3[2] = 0.0f;
    m3[3] = 0.0f;
    m3[4] = t[2];
    m3[5] = t[3];
    m3[6] = 0.0f;
    m3[7] = 0.0f;
    m3[8] = t[4];
    m3[9] = t[5];
    m3[10] = 1.0f;
    m3[11] = 0.0f;
}

static EVGRcolor glevgr__premulColor(EVGRcolor c)
{
    c.ch.r *= c.ch.a;
    c.ch.g *= c.ch.a;
    c.ch.b *= c.ch.a;
    return c;
}

static int glevgr__convertPaint(GLEVGRcontext * gl, GLEVGRfragUniforms * frag, EVGRpaint * paint,
                               EVGRscissor * scissor, float width, float fringe, float strokeThr)
{
    GLEVGRtexture * tex = NULL;
    float invxform[6];

    lv_memzero(frag, sizeof(*frag));

    frag->s.innerCol = glevgr__premulColor(paint->innerColor);
    frag->s.outerCol = glevgr__premulColor(paint->outerColor);

    if(scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
        lv_memzero(frag->s.scissorMat, sizeof(frag->s.scissorMat));
        frag->s.scissorExt[0] = 1.0f;
        frag->s.scissorExt[1] = 1.0f;
        frag->s.scissorScale[0] = 1.0f;
        frag->s.scissorScale[1] = 1.0f;
    }
    else {
        evgrTransformInverse(invxform, scissor->xform);
        glevgr__xformToMat3x4(frag->s.scissorMat, invxform);
        frag->s.scissorExt[0] = scissor->extent[0];
        frag->s.scissorExt[1] = scissor->extent[1];
        frag->s.scissorScale[0] = sqrtf(scissor->xform[0] * scissor->xform[0] + scissor->xform[2] * scissor->xform[2]) / fringe;
        frag->s.scissorScale[1] = sqrtf(scissor->xform[1] * scissor->xform[1] + scissor->xform[3] * scissor->xform[3]) / fringe;
    }

    lv_memcpy(frag->s.extent, paint->extent, sizeof(frag->s.extent));
    frag->s.strokeMult = (width * 0.5f + fringe * 0.5f) / fringe;
    frag->s.strokeThr = strokeThr;

    if(paint->image != 0) {
        tex = glevgr__findTexture(gl, paint->image);
        if(tex == NULL) return 0;
        if((tex->flags & EVGR_IMAGE_FLIPY) != 0) {
            float m1[6], m2[6];
            evgrTransformTranslate(m1, 0.0f, frag->s.extent[1] * 0.5f);
            evgrTransformMultiply(m1, paint->xform);
            evgrTransformScale(m2, 1.0f, -1.0f);
            evgrTransformMultiply(m2, m1);
            evgrTransformTranslate(m1, 0.0f, -frag->s.extent[1] * 0.5f);
            evgrTransformMultiply(m1, m2);
            evgrTransformInverse(invxform, m1);
        }
        else {
            evgrTransformInverse(invxform, paint->xform);
        }
        frag->s.type = NSVG_SHADER_FILLIMG;

#if EVGR_GL_USE_UNIFORMBUFFER
        if(tex->type == EVGR_TEXTURE_RGBA)
            frag->s.texType = (tex->flags & EVGR_IMAGE_PREMULTIPLIED) ? 0 : 1;
        else if(tex->type == EVGR_TEXTURE_BGRA)
            frag->s.texType = (tex->flags & EVGR_IMAGE_PREMULTIPLIED) ? 3 : 5;  // BGR swizzle, optionally premultiply
        else if(tex->type == EVGR_TEXTURE_BGR)
            frag->s.texType = 3;  // BGR -> RGB swizzle (no alpha channel)
        else if(tex->type == EVGR_TEXTURE_BGRX)
            frag->s.texType = 4;  // BGRX -> RGB with alpha=1 in shader
        else if(tex->type == EVGR_TEXTURE_RGB565)
            frag->s.texType = 0;  // RGB565 is directly compatible
        else
            frag->s.texType = 2;
#else
        if(tex->type == EVGR_TEXTURE_RGBA)
            frag->s.texType = (tex->flags & EVGR_IMAGE_PREMULTIPLIED) ? 0.0f : 1.0f;
        else if(tex->type == EVGR_TEXTURE_BGRA)
            frag->s.texType = (tex->flags & EVGR_IMAGE_PREMULTIPLIED) ? 3.0f : 5.0f;  // BGR swizzle, optionally premultiply
        else if(tex->type == EVGR_TEXTURE_BGR)
            frag->s.texType = 3.0f;  // BGR -> RGB swizzle (no alpha channel)
        else if(tex->type == EVGR_TEXTURE_BGRX)
            frag->s.texType = 4.0f;  // BGRX -> RGB with alpha=1 in shader
        else if(tex->type == EVGR_TEXTURE_RGB565)
            frag->s.texType = 0.0f;  // RGB565 is directly compatible
        else
            frag->s.texType = 2.0f;
#endif
        //      printf("frag->texType = %d\n", frag->texType);
        frag->s.recolor = paint->recolor;
    }
    else {
        frag->s.type = NSVG_SHADER_FILLGRAD;
        frag->s.radius = paint->radius;
        frag->s.feather = paint->feather;
        evgrTransformInverse(invxform, paint->xform);
    }

    glevgr__xformToMat3x4(frag->s.paintMat, invxform);

    return 1;
}

static GLEVGRfragUniforms * evgr__fragUniformPtr(GLEVGRcontext * gl, int i);

static void glevgr__bindShader(GLEVGRcontext * gl, int shaderType)
{
    if(gl->boundShader != shaderType) {
        gl->boundShader = shaderType;
        glUseProgram(gl->shaders[shaderType].prog);
        glUniform1i(gl->shaders[shaderType].loc[GLEVGR_LOC_TEX], 0);
        glUniform2fv(gl->shaders[shaderType].loc[GLEVGR_LOC_VIEWSIZE], 1, gl->view);
    }
}

static void glevgr__setUniforms(GLEVGRcontext * gl, int uniformOffset, int image, int shaderType)
{
    GLEVGRtexture * tex = NULL;

    glevgr__bindShader(gl, shaderType);

#if EVGR_GL_USE_UNIFORMBUFFER
    glBindBufferRange(GL_UNIFORM_BUFFER, GLEVGR_FRAG_BINDING, gl->fragBuf, uniformOffset, sizeof(GLEVGRfragUniforms));
#else
    // Optimization: NSVG_SHADER_SIMPLE doesn't use any uniforms in the fragment shader,
    // so we can skip uploading them.
    if(shaderType != NSVG_SHADER_SIMPLE) {
        GLEVGRfragUniforms * frag = evgr__fragUniformPtr(gl, uniformOffset);
        glUniform4fv(gl->shaders[shaderType].loc[GLEVGR_LOC_FRAG], EVGR_GL_UNIFORMARRAY_SIZE, &(frag->uniformArray[0][0]));
    }
#endif

    if(image != 0) {
        tex = glevgr__findTexture(gl, image);
    }
    // If no image is set, use empty texture
    if(tex == NULL) {
        tex = glevgr__findTexture(gl, gl->dummyTex);
    }
    glevgr__bindTexture(gl, tex != NULL ? tex->tex : 0);
    glevgr__checkError(gl, "tex paint tex");
}

static void glevgr__renderViewport(void * uptr, float width, float height, float devicePixelRatio)
{
    EVGR_NOTUSED(devicePixelRatio);
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    gl->view[0] = width;
    gl->view[1] = height;
}

static void glevgr__fill(GLEVGRcontext * gl, GLEVGRcall * call)
{
    LV_PROFILER_DRAW_BEGIN;
    GLEVGRpath * paths = &gl->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    // Draw shapes
    glEnable(GL_STENCIL_TEST);
    glevgr__stencilMask(gl, 0xff);
    glevgr__stencilFunc(gl, GL_ALWAYS, 0, 0xff);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    // set bindpoint for solid loc
    glevgr__setUniforms(gl, call->uniformOffset, 0, NSVG_SHADER_SIMPLE);
    glevgr__checkError(gl, "fill simple");

    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    glDisable(GL_CULL_FACE);
    for(i = 0; i < npaths; i++)
        glDrawArrays(GL_TRIANGLE_FAN, paths[i].fillOffset, paths[i].fillCount);
    glEnable(GL_CULL_FACE);

    // Draw anti-aliased pixels
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glevgr__setUniforms(gl, call->uniformOffset + gl->fragSize, call->image, call->shaderType);
    glevgr__checkError(gl, "fill fill");

    if(gl->flags & EVGR_ANTIALIAS) {
        glevgr__stencilFunc(gl, GL_EQUAL, 0x00, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        // Draw fringes
        for(i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
    }

    // Draw fill
    glevgr__stencilFunc(gl, GL_NOTEQUAL, 0x0, 0xff);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
    glDrawArrays(GL_TRIANGLE_STRIP, call->triangleOffset, call->triangleCount);

    glDisable(GL_STENCIL_TEST);
    LV_PROFILER_DRAW_END;
}

static void glevgr__convexFill(GLEVGRcontext * gl, GLEVGRcall * call)
{
    LV_PROFILER_DRAW_BEGIN;
    GLEVGRpath * paths = &gl->paths[call->pathOffset];
    int i, npaths = call->pathCount;

    glevgr__setUniforms(gl, call->uniformOffset, call->image, call->shaderType);
    glevgr__checkError(gl, "convex fill");

    for(i = 0; i < npaths; i++) {
        glDrawArrays(GL_TRIANGLE_FAN, paths[i].fillOffset, paths[i].fillCount);
        // Draw fringes
        if(paths[i].strokeCount > 0) {
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
        }
    }
    LV_PROFILER_DRAW_END;
}

static void glevgr__stroke(GLEVGRcontext * gl, GLEVGRcall * call)
{
    LV_PROFILER_DRAW_BEGIN;
    GLEVGRpath * paths = &gl->paths[call->pathOffset];
    int npaths = call->pathCount, i;

    if(gl->flags & EVGR_STENCIL_STROKES) {

        glEnable(GL_STENCIL_TEST);
        glevgr__stencilMask(gl, 0xff);

        // Fill the stroke base without overlap
        glevgr__stencilFunc(gl, GL_EQUAL, 0x0, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
        glevgr__setUniforms(gl, call->uniformOffset + gl->fragSize, call->image, NSVG_SHADER_SIMPLE);
        glevgr__checkError(gl, "stroke fill 0");
        for(i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);

        // Draw anti-aliased pixels.
        glevgr__setUniforms(gl, call->uniformOffset, call->image, call->shaderType);
        glevgr__stencilFunc(gl, GL_EQUAL, 0x00, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        for(i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);

        // Clear stencil buffer.
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glevgr__stencilFunc(gl, GL_ALWAYS, 0x0, 0xff);
        glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
        glevgr__checkError(gl, "stroke fill 1");
        for(i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glDisable(GL_STENCIL_TEST);

        //      glevgr__convertPaint(gl, evgr__fragUniformPtr(gl, call->uniformOffset + gl->fragSize), paint, scissor, strokeWidth, fringe, 1.0f - 0.5f/255.0f);

    }
    else {
        glevgr__setUniforms(gl, call->uniformOffset, call->image, call->shaderType);
        glevgr__checkError(gl, "stroke fill");
        // Draw Strokes
        for(i = 0; i < npaths; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, paths[i].strokeOffset, paths[i].strokeCount);
    }
    LV_PROFILER_DRAW_END;
}

static void glevgr__triangles(GLEVGRcontext * gl, GLEVGRcall * call)
{
    LV_PROFILER_DRAW_BEGIN;
    glevgr__setUniforms(gl, call->uniformOffset, call->image, NSVG_SHADER_IMG);
    glevgr__checkError(gl, "triangles fill");

    glDrawArrays(GL_TRIANGLES, call->triangleOffset, call->triangleCount);
    LV_PROFILER_DRAW_END;
}

static void glevgr__renderCancel(void * uptr)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    gl->nverts = 0;
    gl->npaths = 0;
    gl->ncalls = 0;
    gl->nuniforms = 0;
}

static GLenum glevgr_convertBlendFuncFactor(int factor)
{
    if(factor == EVGR_ZERO)
        return GL_ZERO;
    if(factor == EVGR_ONE)
        return GL_ONE;
    if(factor == EVGR_SRC_COLOR)
        return GL_SRC_COLOR;
    if(factor == EVGR_ONE_MINUS_SRC_COLOR)
        return GL_ONE_MINUS_SRC_COLOR;
    if(factor == EVGR_DST_COLOR)
        return GL_DST_COLOR;
    if(factor == EVGR_ONE_MINUS_DST_COLOR)
        return GL_ONE_MINUS_DST_COLOR;
    if(factor == EVGR_SRC_ALPHA)
        return GL_SRC_ALPHA;
    if(factor == EVGR_ONE_MINUS_SRC_ALPHA)
        return GL_ONE_MINUS_SRC_ALPHA;
    if(factor == EVGR_DST_ALPHA)
        return GL_DST_ALPHA;
    if(factor == EVGR_ONE_MINUS_DST_ALPHA)
        return GL_ONE_MINUS_DST_ALPHA;
    if(factor == EVGR_SRC_ALPHA_SATURATE)
        return GL_SRC_ALPHA_SATURATE;
    return GL_INVALID_ENUM;
}

static GLEVGRblend glevgr__blendCompositeOperation(EVGRcompositeOperationState op)
{
    GLEVGRblend blend;
    blend.srcRGB = glevgr_convertBlendFuncFactor(op.srcRGB);
    blend.dstRGB = glevgr_convertBlendFuncFactor(op.dstRGB);
    blend.srcAlpha = glevgr_convertBlendFuncFactor(op.srcAlpha);
    blend.dstAlpha = glevgr_convertBlendFuncFactor(op.dstAlpha);
    if(blend.srcRGB == GL_INVALID_ENUM || blend.dstRGB == GL_INVALID_ENUM || blend.srcAlpha == GL_INVALID_ENUM ||
       blend.dstAlpha == GL_INVALID_ENUM) {
        blend.srcRGB = GL_ONE;
        blend.dstRGB = GL_ONE_MINUS_SRC_ALPHA;
        blend.srcAlpha = GL_ONE;
        blend.dstAlpha = GL_ONE_MINUS_SRC_ALPHA;
    }
    return blend;
}

static void glevgr__renderFlush(void * uptr)
{
    LV_PROFILER_DRAW_BEGIN;
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    int i;

    if(gl->ncalls > 0) {

        // Setup require GL state.
        LV_PROFILER_DRAW_BEGIN_TAG("setup_gl_state");
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilMask(0xffffffff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
#if EVGR_GL_USE_STATE_FILTER
        gl->boundTexture = 0;
        gl->boundShader = -1;
        gl->stencilMask = 0xffffffff;
        gl->stencilFunc = GL_ALWAYS;
        gl->stencilFuncRef = 0;
        gl->stencilFuncMask = 0xffffffff;
        gl->blendFunc.srcRGB = GL_INVALID_ENUM;
        gl->blendFunc.srcAlpha = GL_INVALID_ENUM;
        gl->blendFunc.dstRGB = GL_INVALID_ENUM;
        gl->blendFunc.dstAlpha = GL_INVALID_ENUM;
#endif
        LV_PROFILER_DRAW_END_TAG("setup_gl_state");

#if EVGR_GL_USE_UNIFORMBUFFER
        // Upload ubo for frag shaders
        LV_PROFILER_DRAW_BEGIN_TAG("glBindBuffer");
        glBindBuffer(GL_UNIFORM_BUFFER, gl->fragBuf);
        LV_PROFILER_DRAW_END_TAG("glBindBuffer");
        LV_PROFILER_DRAW_BEGIN_TAG("glBufferData");
        glBufferData(GL_UNIFORM_BUFFER, gl->nuniforms * gl->fragSize, gl->uniforms, GL_STREAM_DRAW);
        LV_PROFILER_DRAW_END_TAG("glBufferData");
#endif

        // Upload vertex data
#if defined EVGR_GL3
        LV_PROFILER_DRAW_BEGIN_TAG("glBindVertexArray");
        glBindVertexArray(gl->vertArr);
        LV_PROFILER_DRAW_END_TAG("glBindVertexArray");
#endif
        gl->vertBufIndex = (gl->vertBufIndex + 1) % 2;
        LV_PROFILER_DRAW_BEGIN_TAG("glBindBuffer");
        glBindBuffer(GL_ARRAY_BUFFER, gl->vertBuf[gl->vertBufIndex]);
        LV_PROFILER_DRAW_END_TAG("glBindBuffer");
        LV_PROFILER_DRAW_BEGIN_TAG("glBufferData");
        glBufferData(GL_ARRAY_BUFFER, gl->nverts * sizeof(EVGRvertex), gl->verts, GL_STREAM_DRAW);
        LV_PROFILER_DRAW_END_TAG("glBufferData");
        LV_PROFILER_DRAW_BEGIN_TAG("glEnableVertexAttribArray");
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        LV_PROFILER_DRAW_END_TAG("glEnableVertexAttribArray");
        LV_PROFILER_DRAW_BEGIN_TAG("glVertexAttribPointer");
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(EVGRvertex), (const GLvoid *)(size_t)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(EVGRvertex), (const GLvoid *)(0 + 2 * sizeof(float)));
        LV_PROFILER_DRAW_END_TAG("glVertexAttribPointer");

#if EVGR_GL_USE_UNIFORMBUFFER
        LV_PROFILER_DRAW_BEGIN_TAG("glBindBuffer");
        glBindBuffer(GL_UNIFORM_BUFFER, gl->fragBuf);
        LV_PROFILER_DRAW_END_TAG("glBindBuffer");
#endif

        for(i = 0; i < gl->ncalls; i++) {
            GLEVGRcall * call = &gl->calls[i];
            glevgr__blendFuncSeparate(gl, &call->blendFunc);
            if(call->type == GLEVGR_FILL)
                glevgr__fill(gl, call);
            else if(call->type == GLEVGR_CONVEXFILL)
                glevgr__convexFill(gl, call);
            else if(call->type == GLEVGR_STROKE)
                glevgr__stroke(gl, call);
            else if(call->type == GLEVGR_TRIANGLES)
                glevgr__triangles(gl, call);
        }

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
#if defined EVGR_GL3
        glBindVertexArray(0);
#endif
        glDisable(GL_CULL_FACE);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
        glevgr__bindTexture(gl, 0);
    }

    // Reset calls
    gl->nverts = 0;
    gl->npaths = 0;
    gl->ncalls = 0;
    gl->nuniforms = 0;
    LV_PROFILER_DRAW_END;
}

static int glevgr__maxVertCount(const EVGRpath * paths, int npaths)
{
    int i, count = 0;
    for(i = 0; i < npaths; i++) {
        count += paths[i].nfill;
        count += paths[i].nstroke;
    }
    return count;
}

static GLEVGRcall * glevgr__allocCall(GLEVGRcontext * gl)
{
    GLEVGRcall * ret = NULL;
    if(gl->ncalls + 1 > gl->ccalls) {
        GLEVGRcall * calls;
        int ccalls = glevgr__maxi(gl->ncalls + 1, 128) + gl->ccalls / 2; // 1.5x Overallocate
        calls = (GLEVGRcall *)lv_realloc(gl->calls, sizeof(GLEVGRcall) * ccalls);
        if(calls == NULL) return NULL;
        gl->calls = calls;
        gl->ccalls = ccalls;
    }
    ret = &gl->calls[gl->ncalls++];
    lv_memzero(ret, sizeof(GLEVGRcall));
    return ret;
}

static int glevgr__allocPaths(GLEVGRcontext * gl, int n)
{
    int ret = 0;
    if(gl->npaths + n > gl->cpaths) {
        GLEVGRpath * paths;
        int cpaths = glevgr__maxi(gl->npaths + n, 128) + gl->cpaths / 2; // 1.5x Overallocate
        paths = (GLEVGRpath *)lv_realloc(gl->paths, sizeof(GLEVGRpath) * cpaths);
        if(paths == NULL) return -1;
        gl->paths = paths;
        gl->cpaths = cpaths;
    }
    ret = gl->npaths;
    gl->npaths += n;
    return ret;
}

static int glevgr__allocVerts(GLEVGRcontext * gl, int n)
{
    int ret = 0;
    if(gl->nverts + n > gl->cverts) {
        EVGRvertex * verts;
        int cverts = glevgr__maxi(gl->nverts + n, 4096) + gl->cverts / 2; // 1.5x Overallocate
        verts = (EVGRvertex *)lv_realloc(gl->verts, sizeof(EVGRvertex) * cverts);
        if(verts == NULL) return -1;
        gl->verts = verts;
        gl->cverts = cverts;
    }
    ret = gl->nverts;
    gl->nverts += n;
    return ret;
}

static int glevgr__allocFragUniforms(GLEVGRcontext * gl, int n)
{
    int ret = 0, structSize = gl->fragSize;
    if(gl->nuniforms + n > gl->cuniforms) {
        unsigned char * uniforms;
        int cuniforms = glevgr__maxi(gl->nuniforms + n, 128) + gl->cuniforms / 2; // 1.5x Overallocate
        uniforms = (unsigned char *)lv_realloc(gl->uniforms, structSize * cuniforms);
        if(uniforms == NULL) return -1;
        gl->uniforms = uniforms;
        gl->cuniforms = cuniforms;
    }
    ret = gl->nuniforms * structSize;
    gl->nuniforms += n;
    return ret;
}

static GLEVGRfragUniforms * evgr__fragUniformPtr(GLEVGRcontext * gl, int i)
{
    return (GLEVGRfragUniforms *)&gl->uniforms[i];
}

static void glevgr__vset(EVGRvertex * vtx, float x, float y, float u, float v)
{
    vtx->x = x;
    vtx->y = y;
    vtx->u = u;
    vtx->v = v;
}

static void glevgr__renderFill(void * uptr, EVGRpaint * paint, EVGRcompositeOperationState compositeOperation,
                              EVGRscissor * scissor, float fringe,
                              const float * bounds, const EVGRpath * paths, int npaths)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    GLEVGRcall * call = glevgr__allocCall(gl);
    EVGRvertex * quad;
    GLEVGRfragUniforms * frag;
    int i, maxverts, offset;

    if(call == NULL) return;

    call->type = GLEVGR_FILL;
    call->triangleCount = 4;
    call->pathOffset = glevgr__allocPaths(gl, npaths);
    if(call->pathOffset == -1) goto error;
    call->pathCount = npaths;
    call->image = paint->image;
    call->blendFunc = glevgr__blendCompositeOperation(compositeOperation);

    if(npaths == 1 && paths[0].convex) {
        call->type = GLEVGR_CONVEXFILL;
        call->triangleCount = 0;    // Bounding box fill quad not needed for convex fill
    }

    // Allocate vertices for all the paths.
    maxverts = glevgr__maxVertCount(paths, npaths) + call->triangleCount;
    offset = glevgr__allocVerts(gl, maxverts);
    if(offset == -1) goto error;

    for(i = 0; i < npaths; i++) {
        GLEVGRpath * copy = &gl->paths[call->pathOffset + i];
        const EVGRpath * path = &paths[i];
        lv_memzero(copy, sizeof(GLEVGRpath));
        if(path->nfill > 0) {
            copy->fillOffset = offset;
            copy->fillCount = path->nfill;
            lv_memcpy(&gl->verts[offset], path->fill, sizeof(EVGRvertex) * path->nfill);
            offset += path->nfill;
        }
        if(path->nstroke > 0) {
            copy->strokeOffset = offset;
            copy->strokeCount = path->nstroke;
            lv_memcpy(&gl->verts[offset], path->stroke, sizeof(EVGRvertex) * path->nstroke);
            offset += path->nstroke;
        }
    }

    // Setup uniforms for draw calls
    if(call->type == GLEVGR_FILL) {
        // Quad
        call->triangleOffset = offset;
        quad = &gl->verts[call->triangleOffset];
        glevgr__vset(&quad[0], bounds[2], bounds[3], 0.5f, 1.0f);
        glevgr__vset(&quad[1], bounds[2], bounds[1], 0.5f, 1.0f);
        glevgr__vset(&quad[2], bounds[0], bounds[3], 0.5f, 1.0f);
        glevgr__vset(&quad[3], bounds[0], bounds[1], 0.5f, 1.0f);

        call->uniformOffset = glevgr__allocFragUniforms(gl, 2);
        if(call->uniformOffset == -1) goto error;
        // Simple shader for stencil
        frag = evgr__fragUniformPtr(gl, call->uniformOffset);
        lv_memzero(frag, sizeof(*frag));
        frag->s.strokeThr = -1.0f;
        frag->s.type = NSVG_SHADER_SIMPLE;
        // Fill shader
        glevgr__convertPaint(gl, evgr__fragUniformPtr(gl, call->uniformOffset + gl->fragSize), paint, scissor, fringe, fringe,
                            -1.0f);
    }
    else {
        call->uniformOffset = glevgr__allocFragUniforms(gl, 1);
        if(call->uniformOffset == -1) goto error;
        // Fill shader
        glevgr__convertPaint(gl, evgr__fragUniformPtr(gl, call->uniformOffset), paint, scissor, fringe, fringe, -1.0f);
    }

    if(paint->image != 0)
        call->shaderType = NSVG_SHADER_FILLIMG;
    else
        call->shaderType = NSVG_SHADER_FILLGRAD;

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if(gl->ncalls > 0) gl->ncalls--;
}

static void glevgr__renderStroke(void * uptr, EVGRpaint * paint, EVGRcompositeOperationState compositeOperation,
                                EVGRscissor * scissor, float fringe,
                                float strokeWidth, const EVGRpath * paths, int npaths)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    GLEVGRcall * call = glevgr__allocCall(gl);
    int i, maxverts, offset;

    if(call == NULL) return;

    call->type = GLEVGR_STROKE;
    call->pathOffset = glevgr__allocPaths(gl, npaths);
    if(call->pathOffset == -1) goto error;
    call->pathCount = npaths;
    call->image = paint->image;
    call->blendFunc = glevgr__blendCompositeOperation(compositeOperation);

    // Allocate vertices for all the paths.
    maxverts = glevgr__maxVertCount(paths, npaths);
    offset = glevgr__allocVerts(gl, maxverts);
    if(offset == -1) goto error;

    for(i = 0; i < npaths; i++) {
        GLEVGRpath * copy = &gl->paths[call->pathOffset + i];
        const EVGRpath * path = &paths[i];
        lv_memzero(copy, sizeof(GLEVGRpath));
        if(path->nstroke) {
            copy->strokeOffset = offset;
            copy->strokeCount = path->nstroke;
            lv_memcpy(&gl->verts[offset], path->stroke, sizeof(EVGRvertex) * path->nstroke);
            offset += path->nstroke;
        }
    }

    if(gl->flags & EVGR_STENCIL_STROKES) {
        // Fill shader
        call->uniformOffset = glevgr__allocFragUniforms(gl, 2);
        if(call->uniformOffset == -1) goto error;

        glevgr__convertPaint(gl, evgr__fragUniformPtr(gl, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
        glevgr__convertPaint(gl, evgr__fragUniformPtr(gl, call->uniformOffset + gl->fragSize), paint, scissor, strokeWidth,
                            fringe, 1.0f - 0.5f / 255.0f);

    }
    else {
        // Fill shader
        call->uniformOffset = glevgr__allocFragUniforms(gl, 1);
        if(call->uniformOffset == -1) goto error;
        glevgr__convertPaint(gl, evgr__fragUniformPtr(gl, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
    }

    if(paint->image != 0)
        call->shaderType = NSVG_SHADER_FILLIMG;
    else
        call->shaderType = NSVG_SHADER_FILLGRAD;

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if(gl->ncalls > 0) gl->ncalls--;
}

static void glevgr__renderTriangles(void * uptr, EVGRpaint * paint, EVGRcompositeOperationState compositeOperation,
                                   EVGRscissor * scissor,
                                   const EVGRvertex * verts, int nverts, float fringe)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    GLEVGRcall * call = glevgr__allocCall(gl);
    GLEVGRfragUniforms * frag;

    if(call == NULL) return;

    call->type = GLEVGR_TRIANGLES;
    call->image = paint->image;
    call->blendFunc = glevgr__blendCompositeOperation(compositeOperation);

    // Allocate vertices for all the paths.
    call->triangleOffset = glevgr__allocVerts(gl, nverts);
    if(call->triangleOffset == -1) goto error;
    call->triangleCount = nverts;

    lv_memcpy(&gl->verts[call->triangleOffset], verts, sizeof(EVGRvertex) * nverts);

    // Fill shader
    call->uniformOffset = glevgr__allocFragUniforms(gl, 1);
    if(call->uniformOffset == -1) goto error;
    frag = evgr__fragUniformPtr(gl, call->uniformOffset);
    glevgr__convertPaint(gl, frag, paint, scissor, 1.0f, fringe, -1.0f);
    frag->s.type = NSVG_SHADER_IMG;
    call->shaderType = NSVG_SHADER_IMG;

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if(gl->ncalls > 0) gl->ncalls--;
}

static void glevgr__renderDelete(void * uptr)
{
    GLEVGRcontext * gl = (GLEVGRcontext *)uptr;
    int i;
    if(gl == NULL) return;

    for(i = 0; i < GLEVGR_SHADER_COUNT; i++)
        glevgr__deleteShader(&gl->shaders[i]);

#if defined EVGR_GL3
#if EVGR_GL_USE_UNIFORMBUFFER
    if(gl->fragBuf != 0)
        glDeleteBuffers(1, &gl->fragBuf);
#endif
    if(gl->vertArr != 0)
        glDeleteVertexArrays(1, &gl->vertArr);
#endif
    if(gl->vertBuf[0] != 0)
        glDeleteBuffers(2, gl->vertBuf);

    for(i = 0; i < gl->ntextures; i++) {
        if(gl->textures[i].tex != 0 && (gl->textures[i].flags & EVGR_IMAGE_NODELETE) == 0)
            glDeleteTextures(1, &gl->textures[i].tex);
    }
    lv_free(gl->textures);

    lv_free(gl->paths);
    lv_free(gl->verts);
    lv_free(gl->uniforms);
    lv_free(gl->calls);

    lv_free(gl);
}


#if defined EVGR_GL2
    EVGRcontext * evgrCreateGL2(int flags)
#elif defined EVGR_GL3
    EVGRcontext * evgrCreateGL3(int flags)
#elif defined EVGR_GLES2
    EVGRcontext * evgrCreateGLES2(int flags)
#elif defined EVGR_GLES3
    EVGRcontext * evgrCreateGLES3(int flags)
#endif
{
    EVGRparams params;
    EVGRcontext * ctx = NULL;
    GLEVGRcontext * gl = (GLEVGRcontext *)lv_malloc(sizeof(GLEVGRcontext));
    if(gl == NULL) goto error;
    lv_memzero(gl, sizeof(GLEVGRcontext));

    lv_memzero(&params, sizeof(params));
    params.renderCreate = glevgr__renderCreate;
    params.renderCreateTexture = glevgr__renderCreateTexture;
    params.renderDeleteTexture = glevgr__renderDeleteTexture;
    params.renderUpdateTexture = glevgr__renderUpdateTexture;
    params.renderGetTextureSize = glevgr__renderGetTextureSize;
    params.renderViewport = glevgr__renderViewport;
    params.renderCancel = glevgr__renderCancel;
    params.renderFlush = glevgr__renderFlush;
    params.renderFill = glevgr__renderFill;
    params.renderStroke = glevgr__renderStroke;
    params.renderTriangles = glevgr__renderTriangles;
    params.renderDelete = glevgr__renderDelete;
    params.userPtr = gl;
    params.edgeAntiAlias = flags & EVGR_ANTIALIAS ? 1 : 0;

    gl->flags = flags;

    ctx = evgrCreateInternal(&params);
    if(ctx == NULL) goto error;

    return ctx;

error:
    // 'gl' is freed by evgrDeleteInternal.
    if(ctx != NULL) evgrDeleteInternal(ctx);
    return NULL;
}

#if defined EVGR_GL2
    void evgrDeleteGL2(EVGRcontext * ctx)
#elif defined EVGR_GL3
    void evgrDeleteGL3(EVGRcontext * ctx)
#elif defined EVGR_GLES2
    void evgrDeleteGLES2(EVGRcontext * ctx)
#elif defined EVGR_GLES3
    void evgrDeleteGLES3(EVGRcontext * ctx)
#endif
{
    evgrDeleteInternal(ctx);
}

#if defined EVGR_GL2
    int evgrlCreateImageFromHandleGL2(EVGRcontext * ctx, GLuint textureId, int w, int h, int imageFlags)
#elif defined EVGR_GL3
    int evgrlCreateImageFromHandleGL3(EVGRcontext * ctx, GLuint textureId, int w, int h, int imageFlags)
#elif defined EVGR_GLES2
    int evgrlCreateImageFromHandleGLES2(EVGRcontext * ctx, GLuint textureId, int w, int h, int imageFlags)
#elif defined EVGR_GLES3
    int evgrlCreateImageFromHandleGLES3(EVGRcontext * ctx, GLuint textureId, int w, int h, int imageFlags)
#endif
{
    GLEVGRcontext * gl = (GLEVGRcontext *)evgrInternalParams(ctx)->userPtr;
    GLEVGRtexture * tex = glevgr__allocTexture(gl);

    if(tex == NULL) return 0;

    tex->type = EVGR_TEXTURE_RGBA;
    tex->tex = textureId;
    tex->flags = imageFlags;
    tex->width = w;
    tex->height = h;

    return tex->id;
}

#if defined EVGR_GL2
    GLuint evgrlImageHandleGL2(EVGRcontext * ctx, int image)
#elif defined EVGR_GL3
    GLuint evgrlImageHandleGL3(EVGRcontext * ctx, int image)
#elif defined EVGR_GLES2
    GLuint evgrlImageHandleGLES2(EVGRcontext * ctx, int image)
#elif defined EVGR_GLES3
    GLuint evgrlImageHandleGLES3(EVGRcontext * ctx, int image)
#endif
{
    GLEVGRcontext * gl = (GLEVGRcontext *)evgrInternalParams(ctx)->userPtr;
    GLEVGRtexture * tex = glevgr__findTexture(gl, image);
    return tex->tex;
}

#endif /* LV_USE_DRAW_EVGPU */

#endif /* EVGR_GL_IMPLEMENTATION */
