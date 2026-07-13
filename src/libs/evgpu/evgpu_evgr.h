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

#ifndef EVGPU_EVGR_H
#define EVGPU_EVGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl_public.h"

#if LV_USE_DRAW_EVGPU

#define EVGR_PI 3.14159265358979323846264338327f

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4201)  // nonstandard extension used : nameless struct/union
#endif

typedef struct EVGRcontext EVGRcontext;

union EVGRcolor {
    float rgba[4];
    struct {
        float r, g, b, a;
    } ch;
};
typedef union EVGRcolor EVGRcolor;

struct EVGRpaint {
    float xform[6];
    float extent[2];
    float radius;
    float feather;
    EVGRcolor innerColor;
    EVGRcolor outerColor;
    int image;
    /* LVGL extension: per-pixel image recolor performed in the fragment shader.
     * recolor.rgb is the target color, recolor.a is the recolor opacity
     * (0 = disabled, 1 = full replace). Only applied to image paints. */
    EVGRcolor recolor;
};
typedef struct EVGRpaint EVGRpaint;

enum EVGRwinding {
    EVGR_CCW = 1,            // Winding for solid shapes
    EVGR_CW = 2,             // Winding for holes
};

enum EVGRsolidity {
    EVGR_SOLID = 1,          // CCW
    EVGR_HOLE = 2,           // CW
};

enum EVGRlineCap {
    EVGR_BUTT,
    EVGR_ROUND,
    EVGR_SQUARE,
    EVGR_BEVEL,
    EVGR_MITER,
};

enum EVGRalign {
    // Horizontal align
    EVGR_ALIGN_LEFT      = 1 << 0, // Default, align text horizontally to left.
    EVGR_ALIGN_CENTER    = 1 << 1, // Align text horizontally to center.
    EVGR_ALIGN_RIGHT     = 1 << 2, // Align text horizontally to right.
    // Vertical align
    EVGR_ALIGN_TOP       = 1 << 3, // Align text vertically to top.
    EVGR_ALIGN_MIDDLE    = 1 << 4, // Align text vertically to middle.
    EVGR_ALIGN_BOTTOM    = 1 << 5, // Align text vertically to bottom.
    EVGR_ALIGN_BASELINE  = 1 << 6, // Default, align text vertically to baseline.
};

enum EVGRblendFactor {
    EVGR_ZERO = 1 << 0,
    EVGR_ONE = 1 << 1,
    EVGR_SRC_COLOR = 1 << 2,
    EVGR_ONE_MINUS_SRC_COLOR = 1 << 3,
    EVGR_DST_COLOR = 1 << 4,
    EVGR_ONE_MINUS_DST_COLOR = 1 << 5,
    EVGR_SRC_ALPHA = 1 << 6,
    EVGR_ONE_MINUS_SRC_ALPHA = 1 << 7,
    EVGR_DST_ALPHA = 1 << 8,
    EVGR_ONE_MINUS_DST_ALPHA = 1 << 9,
    EVGR_SRC_ALPHA_SATURATE = 1 << 10,
};

enum EVGRcompositeOperation {
    EVGR_SOURCE_OVER,
    EVGR_SOURCE_IN,
    EVGR_SOURCE_OUT,
    EVGR_ATOP,
    EVGR_DESTINATION_OVER,
    EVGR_DESTINATION_IN,
    EVGR_DESTINATION_OUT,
    EVGR_DESTINATION_ATOP,
    EVGR_LIGHTER,
    EVGR_COPY,
    EVGR_XOR,
};

struct EVGRcompositeOperationState {
    int srcRGB;
    int dstRGB;
    int srcAlpha;
    int dstAlpha;
};
typedef struct EVGRcompositeOperationState EVGRcompositeOperationState;

struct EVGRglyphPosition {
    const char * str;   // Position of the glyph in the input string.
    float x;            // The x-coordinate of the logical glyph position.
    float minx, maxx;   // The bounds of the glyph shape.
};
typedef struct EVGRglyphPosition EVGRglyphPosition;

struct EVGRtextRow {
    const char * start; // Pointer to the input text where the row starts.
    const char * end;   // Pointer to the input text where the row ends (one past the last character).
    const char * next;  // Pointer to the beginning of the next row.
    float width;        // Logical width of the row.
    float minx,
          maxx;   // Actual bounds of the row. Logical with and bounds can differ because of kerning and some parts over extending.
};
typedef struct EVGRtextRow EVGRtextRow;

enum EVGRimageFlags {
    EVGR_IMAGE_GENERATE_MIPMAPS  = 1 << 0,   // Generate mipmaps during creation of the image.
    EVGR_IMAGE_REPEATX           = 1 << 1,   // Repeat image in X direction.
    EVGR_IMAGE_REPEATY           = 1 << 2,   // Repeat image in Y direction.
    EVGR_IMAGE_FLIPY             = 1 << 3,   // Flips (inverses) image in Y direction when rendered.
    EVGR_IMAGE_PREMULTIPLIED     = 1 << 4,   // Image data has premultiplied alpha.
    EVGR_IMAGE_NEAREST           = 1 << 5,   // Image interpolation is Nearest instead Linear
};

// Begin drawing a new frame
// Calls to nanovg drawing API should be wrapped in evgrBeginFrame() & evgrEndFrame()
// evgrBeginFrame() defines the size of the window to render to in relation currently
// set viewport (i.e. glViewport on GL backends). Device pixel ration allows to
// control the rendering on Hi-DPI devices.
// For example, GLFW returns two dimension for an opened window: window size and
// frame buffer size. In that case you would set windowWidth/Height to the window size
// devicePixelRatio to: frameBufferWidth / windowWidth.
void evgrBeginFrame(EVGRcontext * ctx, float windowWidth, float windowHeight, float devicePixelRatio);

// Cancels drawing the current frame.
void evgrCancelFrame(EVGRcontext * ctx);

// Ends drawing flushing remaining render state.
void evgrEndFrame(EVGRcontext * ctx);

//
// Composite operation
//
// The composite operations in NanoVG are modeled after HTML Canvas API, and
// the blend func is based on OpenGL (see corresponding manuals for more info).
// The colors in the blending state have premultiplied alpha.

// Sets the composite operation. The op parameter should be one of EVGRcompositeOperation.
void evgrGlobalCompositeOperation(EVGRcontext * ctx, int op);

// Sets the composite operation with custom pixel arithmetic. The parameters should be one of EVGRblendFactor.
void evgrGlobalCompositeBlendFunc(EVGRcontext * ctx, int sfactor, int dfactor);

// Sets the composite operation with custom pixel arithmetic for RGB and alpha components separately. The parameters should be one of EVGRblendFactor.
void evgrGlobalCompositeBlendFuncSeparate(EVGRcontext * ctx, int srcRGB, int dstRGB, int srcAlpha, int dstAlpha);

//
// Color utils
//
// Colors in NanoVG are stored as unsigned ints in ABGR format.

// Returns a color value from red, green, blue values. Alpha will be set to 255 (1.0f).
EVGRcolor evgrRGB(unsigned char r, unsigned char g, unsigned char b);

// Returns a color value from red, green, blue values. Alpha will be set to 1.0f.
EVGRcolor evgrRGBf(float r, float g, float b);


// Returns a color value from red, green, blue and alpha values.
EVGRcolor evgrRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

// Returns a color value from red, green, blue and alpha values.
EVGRcolor evgrRGBAf(float r, float g, float b, float a);


// Linearly interpolates from color c0 to c1, and returns resulting color value.
EVGRcolor evgrLerpRGBA(EVGRcolor c0, EVGRcolor c1, float u);

// Sets transparency of a color value.
EVGRcolor evgrTransRGBA(EVGRcolor c0, unsigned char a);

// Sets transparency of a color value.
EVGRcolor evgrTransRGBAf(EVGRcolor c0, float a);

// Returns color value specified by hue, saturation and lightness.
// HSL values are all in range [0..1], alpha will be set to 255.
EVGRcolor evgrHSL(float h, float s, float l);

// Returns color value specified by hue, saturation and lightness and alpha.
// HSL values are all in range [0..1], alpha in range [0..255]
EVGRcolor evgrHSLA(float h, float s, float l, unsigned char a);

//
// State Handling
//
// NanoVG contains state which represents how paths will be rendered.
// The state contains transform, fill and stroke styles, text and font styles,
// and scissor clipping.

// Pushes and saves the current render state into a state stack.
// A matching evgrRestore() must be used to restore the state.
void evgrSave(EVGRcontext * ctx);

// Pops and restores current render state.
void evgrRestore(EVGRcontext * ctx);

// Resets current render state to default values. Does not affect the render state stack.
void evgrReset(EVGRcontext * ctx);

//
// Render styles
//
// Fill and stroke render style can be either a solid color or a paint which is a gradient or a pattern.
// Solid color is simply defined as a color value, different kinds of paints can be created
// using evgrLinearGradient(), evgrBoxGradient(), evgrRadialGradient() and evgrImagePattern().
//
// Current render style can be saved and restored using evgrSave() and evgrRestore().

// Sets whether to draw antialias for evgrStroke() and evgrFill(). It's enabled by default.
void evgrShapeAntiAlias(EVGRcontext * ctx, int enabled);

// Sets current stroke style to a solid color.
void evgrStrokeColor(EVGRcontext * ctx, EVGRcolor color);

// Sets current stroke style to a paint, which can be a one of the gradients or a pattern.
void evgrStrokePaint(EVGRcontext * ctx, EVGRpaint paint);

// Sets current fill style to a solid color.
void evgrFillColor(EVGRcontext * ctx, EVGRcolor color);

// Sets current fill style to a paint, which can be a one of the gradients or a pattern.
void evgrFillPaint(EVGRcontext * ctx, EVGRpaint paint);

// Sets the miter limit of the stroke style.
// Miter limit controls when a sharp corner is beveled.
void evgrMiterLimit(EVGRcontext * ctx, float limit);

// Sets the stroke width of the stroke style.
void evgrStrokeWidth(EVGRcontext * ctx, float size);

// Sets how the end of the line (cap) is drawn,
// Can be one of: EVGR_BUTT (default), EVGR_ROUND, EVGR_SQUARE.
void evgrLineCap(EVGRcontext * ctx, int cap);

// Sets how sharp path corners are drawn.
// Can be one of EVGR_MITER (default), EVGR_ROUND, EVGR_BEVEL.
void evgrLineJoin(EVGRcontext * ctx, int join);

// Sets the transparency applied to all rendered shapes.
// Already transparent paths will get proportionally more transparent as well.
void evgrGlobalAlpha(EVGRcontext * ctx, float alpha);

//
// Transforms
//
// The paths, gradients, patterns and scissor region are transformed by an transformation
// matrix at the time when they are passed to the API.
// The current transformation matrix is a affine matrix:
//   [sx kx tx]
//   [ky sy ty]
//   [ 0  0  1]
// Where: sx,sy define scaling, kx,ky skewing, and tx,ty translation.
// The last row is assumed to be 0,0,1 and is not stored.
//
// Apart from evgrResetTransform(), each transformation function first creates
// specific transformation matrix and pre-multiplies the current transformation by it.
//
// Current coordinate system (transformation) can be saved and restored using evgrSave() and evgrRestore().

// Resets current transform to a identity matrix.
void evgrResetTransform(EVGRcontext * ctx);

// Premultiplies current coordinate system by specified matrix.
// The parameters are interpreted as matrix as follows:
//   [a c e]
//   [b d f]
//   [0 0 1]
void evgrTransform(EVGRcontext * ctx, float a, float b, float c, float d, float e, float f);

// Translates current coordinate system.
void evgrTranslate(EVGRcontext * ctx, float x, float y);

// Rotates current coordinate system. Angle is specified in radians.
void evgrRotate(EVGRcontext * ctx, float angle);

// Skews the current coordinate system along X axis. Angle is specified in radians.
void evgrSkewX(EVGRcontext * ctx, float angle);

// Skews the current coordinate system along Y axis. Angle is specified in radians.
void evgrSkewY(EVGRcontext * ctx, float angle);

// Scales the current coordinate system.
void evgrScale(EVGRcontext * ctx, float x, float y);

// Stores the top part (a-f) of the current transformation matrix in to the specified buffer.
//   [a c e]
//   [b d f]
//   [0 0 1]
// There should be space for 6 floats in the return buffer for the values a-f.
void evgrCurrentTransform(EVGRcontext * ctx, float * xform);


// The following functions can be used to make calculations on 2x3 transformation matrices.
// A 2x3 matrix is represented as float[6].

// Sets the transform to identity matrix.
void evgrTransformIdentity(float * dst);

// Sets the transform to translation matrix matrix.
void evgrTransformTranslate(float * dst, float tx, float ty);

// Sets the transform to scale matrix.
void evgrTransformScale(float * dst, float sx, float sy);

// Sets the transform to rotate matrix. Angle is specified in radians.
void evgrTransformRotate(float * dst, float a);

// Sets the transform to skew-x matrix. Angle is specified in radians.
void evgrTransformSkewX(float * dst, float a);

// Sets the transform to skew-y matrix. Angle is specified in radians.
void evgrTransformSkewY(float * dst, float a);

// Sets the transform to the result of multiplication of two transforms, of A = A*B.
void evgrTransformMultiply(float * dst, const float * src);

// Sets the transform to the result of multiplication of two transforms, of A = B*A.
void evgrTransformPremultiply(float * dst, const float * src);

// Sets the destination to inverse of specified transform.
// Returns 1 if the inverse could be calculated, else 0.
int evgrTransformInverse(float * dst, const float * src);

// Transform a point by given transform.
void evgrTransformPoint(float * dstx, float * dsty, const float * xform, float srcx, float srcy);

// Converts degrees to radians and vice versa.
float evgrDegToRad(float deg);
float evgrRadToDeg(float rad);

//
// Images
//
// NanoVG allows you to load jpg, png, psd, tga, pic and gif files to be used for rendering.
// In addition you can upload your own image. The image loading is provided by stb_image.
// The parameter imageFlags is combination of flags defined in EVGRimageFlags.

// Creates image from specified image data with custom format.
// format: see EVGRtexture.
// Returns handle to the image.
int evgrCreateImage(EVGRcontext * ctx, int w, int h, int imageFlags, int format, const unsigned char * data);

// Updates image data specified by image handle.
void evgrUpdateImage(EVGRcontext * ctx, int image, const unsigned char * data);

// Returns the dimensions of a created image.
void evgrImageSize(EVGRcontext * ctx, int image, int * w, int * h);

// Deletes created image.
void evgrDeleteImage(EVGRcontext * ctx, int image);

//
// Paints
//
// NanoVG supports four types of paints: linear gradient, box gradient, radial gradient and image pattern.
// These can be used as paints for strokes and fills.

// Creates and returns a linear gradient. Parameters (sx,sy)-(ex,ey) specify the start and end coordinates
// of the linear gradient, icol specifies the start color and ocol the end color.
// The gradient is transformed by the current transform when it is passed to evgrFillPaint() or evgrStrokePaint().
EVGRpaint evgrLinearGradient(EVGRcontext * ctx, float sx, float sy, float ex, float ey,
                           EVGRcolor icol, EVGRcolor ocol);

// Creates and returns a box gradient. Box gradient is a feathered rounded rectangle, it is useful for rendering
// drop shadows or highlights for boxes. Parameters (x,y) define the top-left corner of the rectangle,
// (w,h) define the size of the rectangle, r defines the corner radius, and f feather. Feather defines how blurry
// the border of the rectangle is. Parameter icol specifies the inner color and ocol the outer color of the gradient.
// The gradient is transformed by the current transform when it is passed to evgrFillPaint() or evgrStrokePaint().
EVGRpaint evgrBoxGradient(EVGRcontext * ctx, float x, float y, float w, float h,
                        float r, float f, EVGRcolor icol, EVGRcolor ocol);

// Creates and returns a radial gradient. Parameters (cx,cy) specify the center, inr and outr specify
// the inner and outer radius of the gradient, icol specifies the start color and ocol the end color.
// The gradient is transformed by the current transform when it is passed to evgrFillPaint() or evgrStrokePaint().
EVGRpaint evgrRadialGradient(EVGRcontext * ctx, float cx, float cy, float inr, float outr,
                           EVGRcolor icol, EVGRcolor ocol);

// Creates and returns an image pattern. Parameters (ox,oy) specify the left-top location of the image pattern,
// (ex,ey) the size of one image, angle rotation around the top-left corner, image is handle to the image to render.
// The gradient is transformed by the current transform when it is passed to evgrFillPaint() or evgrStrokePaint().
EVGRpaint evgrImagePattern(EVGRcontext * ctx, float ox, float oy, float ex, float ey,
                         float angle, int image, float alpha);

//
// Scissoring
//
// Scissoring allows you to clip the rendering into a rectangle. This is useful for various
// user interface cases like rendering a text edit or a timeline.

// Sets the current scissor rectangle.
// The scissor rectangle is transformed by the current transform.
void evgrScissor(EVGRcontext * ctx, float x, float y, float w, float h);

// Intersects current scissor rectangle with the specified rectangle.
// The scissor rectangle is transformed by the current transform.
// Note: in case the rotation of previous scissor rect differs from
// the current one, the intersection will be done between the specified
// rectangle and the previous scissor rectangle transformed in the current
// transform space. The resulting shape is always rectangle.
void evgrIntersectScissor(EVGRcontext * ctx, float x, float y, float w, float h);

// Reset and disables scissoring.
void evgrResetScissor(EVGRcontext * ctx);

//
// Paths
//
// Drawing a new shape starts with evgrBeginPath(), it clears all the currently defined paths.
// Then you define one or more paths and sub-paths which describe the shape. The are functions
// to draw common shapes like rectangles and circles, and lower level step-by-step functions,
// which allow to define a path curve by curve.
//
// NanoVG uses even-odd fill rule to draw the shapes. Solid shapes should have counter clockwise
// winding and holes should have counter clockwise order. To specify winding of a path you can
// call evgrPathWinding(). This is useful especially for the common shapes, which are drawn CCW.
//
// Finally you can fill the path using current fill style by calling evgrFill(), and stroke it
// with current stroke style by calling evgrStroke().
//
// The curve segments and sub-paths are transformed by the current transform.

// Clears the current path and sub-paths.
void evgrBeginPath(EVGRcontext * ctx);

// Starts new sub-path with specified point as first point.
void evgrMoveTo(EVGRcontext * ctx, float x, float y);

// Adds line segment from the last point in the path to the specified point.
void evgrLineTo(EVGRcontext * ctx, float x, float y);

// Adds cubic bezier segment from last point in the path via two control points to the specified point.
void evgrBezierTo(EVGRcontext * ctx, float c1x, float c1y, float c2x, float c2y, float x, float y);

// Adds quadratic bezier segment from last point in the path via a control point to the specified point.
void evgrQuadTo(EVGRcontext * ctx, float cx, float cy, float x, float y);

// Adds an arc segment at the corner defined by the last path point, and two specified points.
void evgrArcTo(EVGRcontext * ctx, float x1, float y1, float x2, float y2, float radius);

// Closes current sub-path with a line segment.
void evgrClosePath(EVGRcontext * ctx);

// Sets the current sub-path winding, see EVGRwinding and EVGRsolidity.
void evgrPathWinding(EVGRcontext * ctx, int dir);

// Creates new circle arc shaped sub-path. The arc center is at cx,cy, the arc radius is r,
// and the arc is drawn from angle a0 to a1, and swept in direction dir (EVGR_CCW, or EVGR_CW).
// Angles are specified in radians.
void evgrArc(EVGRcontext * ctx, float cx, float cy, float r, float a0, float a1, int dir);

// Creates new rectangle shaped sub-path.
void evgrRect(EVGRcontext * ctx, float x, float y, float w, float h);

// Creates new rounded rectangle shaped sub-path.
void evgrRoundedRect(EVGRcontext * ctx, float x, float y, float w, float h, float r);

// Creates new rounded rectangle shaped sub-path with varying radii for each corner.
void evgrRoundedRectVarying(EVGRcontext * ctx, float x, float y, float w, float h, float radTopLeft, float radTopRight,
                           float radBottomRight, float radBottomLeft);

// Creates new ellipse shaped sub-path.
void evgrEllipse(EVGRcontext * ctx, float cx, float cy, float rx, float ry);

// Creates new circle shaped sub-path.
void evgrCircle(EVGRcontext * ctx, float cx, float cy, float r);

// Fills the current path with current fill style.
void evgrFill(EVGRcontext * ctx);

// Fills the current path with current stroke style.
void evgrStroke(EVGRcontext * ctx);


//
// Text
//
// NanoVG allows you to load .ttf files and use the font to render text.
//
// The appearance of the text can be defined by setting the current text style
// and by specifying the fill color. Common text and font settings such as
// font size, letter spacing and text align are supported. Font blur allows you
// to create simple text effects such as drop shadows.
//
// At render time the font face can be set based on the font handles or name.
//
// Font measure functions return values in local space, the calculations are
// carried in the same resolution as the final rendering. This is done because
// the text glyph positions are snapped to the nearest pixels sharp rendering.
//
// The local space means that values are not rotated or scale as per the current
// transformation. For example if you set font size to 12, which would mean that
// line height is 16, then regardless of the current scaling and rotation, the
// returned line height is always 16. Some measures may vary because of the scaling
// since aforementioned pixel snapping.
//
// While this may sound a little odd, the setup allows you to always render the
// same way regardless of scaling. I.e. following works regardless of scaling:
//
//      const char* txt = "Text me up.";
//      evgrTextBounds(vg, x,y, txt, NULL, bounds);
//      evgrBeginPath(vg);
//      evgrRect(vg, bounds[0],bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1]);
//      evgrFill(vg);
//
// Note: currently only solid color fill is supported for text.

// Creates font by loading it from the disk from specified file name.
// Returns handle to the font.
int evgrCreateFont(EVGRcontext * ctx, const char * name, const char * filename);

// fontIndex specifies which font face to load from a .ttf/.ttc file.
int evgrCreateFontAtIndex(EVGRcontext * ctx, const char * name, const char * filename, const int fontIndex);

// Creates font by loading it from the specified memory chunk.
// Returns handle to the font.
int evgrCreateFontMem(EVGRcontext * ctx, const char * name, unsigned char * data, int ndata, int freeData);

// fontIndex specifies which font face to load from a .ttf/.ttc file.
int evgrCreateFontMemAtIndex(EVGRcontext * ctx, const char * name, unsigned char * data, int ndata, int freeData,
                            const int fontIndex);

// Finds a loaded font of specified name, and returns handle to it, or -1 if the font is not found.
int evgrFindFont(EVGRcontext * ctx, const char * name);

// Adds a fallback font by handle.
int evgrAddFallbackFontId(EVGRcontext * ctx, int baseFont, int fallbackFont);

// Adds a fallback font by name.
int evgrAddFallbackFont(EVGRcontext * ctx, const char * baseFont, const char * fallbackFont);

// Resets fallback fonts by handle.
void evgrResetFallbackFontsId(EVGRcontext * ctx, int baseFont);

// Resets fallback fonts by name.
void evgrResetFallbackFonts(EVGRcontext * ctx, const char * baseFont);

// Sets the font size of current text style.
void evgrFontSize(EVGRcontext * ctx, float size);

// Sets the blur of current text style.
void evgrFontBlur(EVGRcontext * ctx, float blur);

// Sets the letter spacing of current text style.
void evgrTextLetterSpacing(EVGRcontext * ctx, float spacing);

// Sets the proportional line height of current text style. The line height is specified as multiple of font size.
void evgrTextLineHeight(EVGRcontext * ctx, float lineHeight);

// Sets the text align of current text style, see EVGRalign for options.
void evgrTextAlign(EVGRcontext * ctx, int align);

// Sets the font face based on specified id of current text style.
void evgrFontFaceId(EVGRcontext * ctx, int font);

// Sets the font face based on specified name of current text style.
void evgrFontFace(EVGRcontext * ctx, const char * font);

// Draws text string at specified location. If end is specified only the sub-string up to the end is drawn.
float evgrText(EVGRcontext * ctx, float x, float y, const char * string, const char * end);

// Draws multi-line text string at specified location wrapped at the specified width. If end is specified only the sub-string up to the end is drawn.
// White space is stripped at the beginning of the rows, the text is split at word boundaries or when new-line characters are encountered.
// Words longer than the max width are slit at nearest character (i.e. no hyphenation).
void evgrTextBox(EVGRcontext * ctx, float x, float y, float breakRowWidth, const char * string, const char * end);

// Measures the specified text string. Parameter bounds should be a pointer to float[4],
// if the bounding box of the text should be returned. The bounds value are [xmin,ymin, xmax,ymax]
// Returns the horizontal advance of the measured text (i.e. where the next character should drawn).
// Measured values are returned in local coordinate space.
float evgrTextBounds(EVGRcontext * ctx, float x, float y, const char * string, const char * end, float * bounds);

// Measures the specified multi-text string. Parameter bounds should be a pointer to float[4],
// if the bounding box of the text should be returned. The bounds value are [xmin,ymin, xmax,ymax]
// Measured values are returned in local coordinate space.
void evgrTextBoxBounds(EVGRcontext * ctx, float x, float y, float breakRowWidth, const char * string, const char * end,
                      float * bounds);

// Calculates the glyph x positions of the specified text. If end is specified only the sub-string will be used.
// Measured values are returned in local coordinate space.
int evgrTextGlyphPositions(EVGRcontext * ctx, float x, float y, const char * string, const char * end,
                          EVGRglyphPosition * positions, int maxPositions);

// Returns the vertical metrics based on the current text style.
// Measured values are returned in local coordinate space.
void evgrTextMetrics(EVGRcontext * ctx, float * ascender, float * descender, float * lineh);

// Breaks the specified text into lines. If end is specified only the sub-string will be used.
// White space is stripped at the beginning of the rows, the text is split at word boundaries or when new-line characters are encountered.
// Words longer than the max width are slit at nearest character (i.e. no hyphenation).
int evgrTextBreakLines(EVGRcontext * ctx, const char * string, const char * end, float breakRowWidth, EVGRtextRow * rows,
                      int maxRows);

//
// Internal Render API
//
enum EVGRtexture {
    EVGR_TEXTURE_ALPHA = 0x01,
    EVGR_TEXTURE_BGRA = 0x02,   /* ARGB8888 format (memory order: B-G-R-A) */
    EVGR_TEXTURE_RGBA = 0x03,   /* Standard OpenGL RGBA format */
    EVGR_TEXTURE_BGR = 0x04,    /* RGB888 format (memory order: B-G-R) */
    EVGR_TEXTURE_RGB565 = 0x05, /* RGB565 format */
    EVGR_TEXTURE_BGRX = 0x06,   /* XRGB8888 format (memory order: B-G-R-X, X ignored) */
};

struct EVGRscissor {
    float xform[6];
    float extent[2];
};
typedef struct EVGRscissor EVGRscissor;

struct EVGRvertex {
    float x, y, u, v;
};
typedef struct EVGRvertex EVGRvertex;

struct EVGRpath {
    int first;
    int count;
    unsigned char closed;
    int nbevel;
    EVGRvertex * fill;
    int nfill;
    EVGRvertex * stroke;
    int nstroke;
    int winding;
    int convex;
};
typedef struct EVGRpath EVGRpath;

struct EVGRparams {
    void * userPtr;
    int edgeAntiAlias;
    int (*renderCreate)(void * uptr);
    int (*renderCreateTexture)(void * uptr, int type, int w, int h, int imageFlags, const unsigned char * data);
    int (*renderDeleteTexture)(void * uptr, int image);
    int (*renderUpdateTexture)(void * uptr, int image, int x, int y, int w, int h, const unsigned char * data);
    int (*renderGetTextureSize)(void * uptr, int image, int * w, int * h);
    void (*renderViewport)(void * uptr, float width, float height, float devicePixelRatio);
    void (*renderCancel)(void * uptr);
    void (*renderFlush)(void * uptr);
    void (*renderFill)(void * uptr, EVGRpaint * paint, EVGRcompositeOperationState compositeOperation, EVGRscissor * scissor,
                       float fringe, const float * bounds, const EVGRpath * paths, int npaths);
    void (*renderStroke)(void * uptr, EVGRpaint * paint, EVGRcompositeOperationState compositeOperation, EVGRscissor * scissor,
                         float fringe, float strokeWidth, const EVGRpath * paths, int npaths);
    void (*renderTriangles)(void * uptr, EVGRpaint * paint, EVGRcompositeOperationState compositeOperation,
                            EVGRscissor * scissor, const EVGRvertex * verts, int nverts, float fringe);
    void (*renderDelete)(void * uptr);
};
typedef struct EVGRparams EVGRparams;

// Constructor and destructor, called by the render back-end.
EVGRcontext * evgrCreateInternal(EVGRparams * params);
void evgrDeleteInternal(EVGRcontext * ctx);

EVGRparams * evgrInternalParams(EVGRcontext * ctx);

// Debug function to dump cached path data.
void evgrDebugDumpPathCache(EVGRcontext * ctx);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define EVGR_NOTUSED(v) for (;;) { (void)(1 ? (void)0 : ( (void)(v) ) ); break; }

#endif // LV_USE_DRAW_EVGPU

#ifdef __cplusplus
}
#endif

#endif // EVGPU_EVGR_H
