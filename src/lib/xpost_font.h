/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
 * Copyright (C) 2013, Vincent Torri
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XPOST_FONT_H
#define XPOST_FONT_H

#ifndef XPOST_OBJECT_H
# error MUST #include "xpost_object.h" before this file
#endif

/**
 * @file xpost_font.h
 * @brief Font manipuation functions
 */

/**
 * @typedef Xpost_Font_Pixel_Mode
 * Describe the format of pixels in a given bitmap.
 *
 * corresponds directly to FreeType's FT_Pixel_Mode enum.
 * http://www.freetype.org/freetype2/docs/reference/ft2-basic_types.html#FT_Pixel_Mode
 */
typedef enum
{
    XPOST_FONT_PIXEL_MODE_NONE = 0, /**< Reserved */
    XPOST_FONT_PIXEL_MODE_MONO,     /**< Monochrome bitmap */
    XPOST_FONT_PIXEL_MODE_GRAY,     /**< 8-bit per pixel bitmap */
    XPOST_FONT_PIXEL_MODE_GRAY2,    /**< 2-bit per pixel bitmap */
    XPOST_FONT_PIXEL_MODE_GRAY4,    /**< 4-bit per pixel bitmap */
    XPOST_FONT_PIXEL_MODE_LCD,      /**< 8-bit per pixel bitmap (for LCD displays) */
    XPOST_FONT_PIXEL_MODE_LCD_V,    /**< 8-bit per pixel bitmap (for LCD displays) */
    XPOST_FONT_PIXEL_MODE_BGRA,     /**< 8-bit per pixel bitmap for colored fonts  with alpha channel */
    XPOST_FONT_PIXEL_MODE_MAX       /**< Reserved */

} Xpost_Font_Pixel_Mode;

/**
 * @brief Initialize the font module.
 *
 * @return 1 on success, 0 otherwise.
 *
 * This function initializes the font module. It is called by
 * xpost_init().
 *
 * @see xpost_font_quit()
 * @see xpost_init()
 */
int xpost_font_init(void);

/**
 * @brief Shut down the font module.
 *
 * This function shuts down the font module. It is called by
 * xpost_quit().
 *
 * @see xpost_font_init()
 * @see xpost_quit()
 */
void xpost_font_quit(void);

/**
 * @brief Return the font face from the given font name.
 *
 * @param[in] name The font name.
 * @return The font face.
 *
 * This function returns the font face of the font named @p name. On
 * error, it returs @c NULL.
 *
 * @see xpost_font_face_free()
 */
void *xpost_font_face_new_from_name(const char *name);

/**
 * @brief Return a font face from a font program held in memory.
 *
 * @param[in] data The font program bytes (TrueType/OpenType sfnt).
 * @param[in] len The number of bytes.
 * @return The font face, or @c NULL on error.
 *
 * The buffer must remain valid for the lifetime of the face; the
 * caller retains ownership.
 *
 * @see xpost_font_face_new_from_name()
 */
void *xpost_font_face_new_from_memory(const unsigned char *data, size_t len);

/**
 * @brief Return bounding box from a font face.
 *
 */
void xpost_font_face_get_bbox(void *face, Xpost_Object *bboxarray, real em);

/**
 * @brief Free the given font.
 *
 * @param[in,out] face The font face.
 *
 * This function frees the memory stored by @p face.
 *
 * @see xpost_font_face_new_from_name()
 */
int xpost_font_face_units(void *face);
int xpost_font_face_is_truetype(void *face);
const char *xpost_font_face_last_file(void);
void xpost_font_face_free(void *face);

/**
 * @brief Scale the given font.
 * @param[in] face The font face.
 * @param[in] scale The scale factor in point.
 *
 * This function scales the font @p face to size @p scale in point
 * unit.
 */
real xpost_font_face_scale(void *face, real scale);

/**
 * @brief Transform the given font.
 * @param[in] face The font face.
 * @param[in] mat The matrix values.
 *
 * This function applies a linear transformation matrix to the font,
 * which may effect any combination of scaling/rotation/skew.
 */
void xpost_font_face_transform(void *face, float *mat);

/**
 * @brief Return the glyph index of the given char  from the given
 * font.
 *
 * @param[in] face The font face.
 * @param[in] c The character.
 * @return The glyph index.
 *
 * This function returns the glygh index of the character @p c in
 * font @p face. If Freetype is not available, it returns -1,
 * otherwise the glyph index.
 *
 * see xpost_font_face_glyph_render()
 */
unsigned int xpost_font_face_glyph_index_get(void *face, char c);

/**
 * @brief Return the glyph index for a glyph name in the given font.
 *
 * @param[in] face The font face.
 * @param[in] name The glyph name (e.g. "zero").
 * @return The glyph index, or 0 when the face has no glyph by that
 * name (or carries no glyph names at all).
 *
 * @see xpost_font_face_glyph_index_get()
 */
unsigned int xpost_font_face_glyph_name_index_get(void *face, const char *name);

/**
 * @brief The number of glyphs in the face, or 0 when the face carries
 * no glyph names to enumerate them by.
 */
unsigned int xpost_font_face_glyph_name_count(void *face);

/**
 * @brief Copy the name of the given glyph into buf (nul-terminated).
 * Returns 0 on a nameless glyph or a face without glyph names.
 */
int xpost_font_face_glyph_name_get(void *face, unsigned int gid, char *buf, int len);

/**
 * @typedef Xpost_Font_Outline_Sink
 * Callbacks receiving a glyph outline decomposed into path segments.
 *
 * Coordinates are in pixels (26.6 fixed point divided out), y-up,
 * relative to the pen position. Quadratic segments are converted so
 * only cubic curves are delivered. Each callback returns 0 to
 * continue, non-zero to abort the decomposition.
 */
typedef struct
{
    int (*moveto)(void *user, double x, double y);
    int (*lineto)(void *user, double x, double y);
    int (*curveto)(void *user, double x1, double y1, double x2, double y2, double x3, double y3);
    int (*closepath)(void *user);
    void *user;
} Xpost_Font_Outline_Sink;

/**
 * @brief Decompose a glyph's outline into path segments.
 *
 * @param[in] face The font face.
 * @param[in] glyph_index The glyph index.
 * @param[in] sink The segment callbacks.
 * @param[out] advance_x The horizontal advance (16.16 fixed point,
 * unhinted linear width through the face transform).
 * @param[out] advance_y The vertical advance (16.16 fixed point).
 * @return 1 on success, 0 otherwise (e.g. a bitmap-only glyph).
 *
 * The glyph is loaded without rendering; the face's size and
 * transform apply to the outline and the advance exactly as they do
 * to the rendered bitmap.
 */
int xpost_font_face_glyph_outline(void *face, unsigned int glyph_index, const Xpost_Font_Outline_Sink *sink, long *advance_x, long *advance_y);

/**
 * @brief render the given glyph of the given face.
 * font.
 *
 * @param[in] face The font face.
 * @param[in] glyph_index The glyph index.
 * @return 1 on success, 0 otherwise.
 *
 * This function renders in an internal buffer the glyph
 * @p glyph_index of font @p face in an internal buffer. It returns 1
 * on success, 0 otherwise.
 *
 * @see xpost_font_face_glyph_index_get()
 */
int xpost_font_face_glyph_render(void *face, unsigned int glyph_index);

/* the glyph cache behind the rendering pair: status and limits for
   the cache operators, and keyed raster entry points for glyphs
   painted by procedure rather than by face */
void xpost_font_cache_status(long *bsize, long *bmax, long *msize,
                             long *mmax, long *csize, long *cmax,
                             long *blimit);
void xpost_font_cache_setlimit(long blimit);
void xpost_font_cache_setparams(long bmax, long lower, long upper);
int xpost_font_cache_lookup_bits(const void *k1, unsigned long k2,
                                 const long m[4], long size,
                                 unsigned char **bits, int *rows, int *width,
                                 int *pitch, int *left, int *top,
                                 long *advance_x, long *advance_y);
int xpost_font_cache_insert_bits(const void *k1, unsigned long k2,
                                 const long m[4], long size,
                                 const unsigned char *bits, int rows,
                                 int width, int pitch, int left, int top,
                                 long advance_x, long advance_y);

/**
 * @brief Report a glyph outline's ink extent and advance without
 * rasterizing, in 26.6 glyph space (y-up around the pen).
 *
 * @return 1 on success; 0 when the glyph cannot load or has no
 * outline (a bitmap strike), in which case render instead.
 */
int xpost_font_face_glyph_extents(void *face, unsigned int glyph_index,
                                  long *xmin, long *ymin, long *xmax, long *ymax,
                                  long *advance_x, long *advance_y);

void xpost_font_face_glyph_buffer_get(void *face, unsigned char **buffer, int *rows, int *width, int *pitch, char *pixel_mode, int *left, int *top, long *advance_x, long *advance_y);


#endif
