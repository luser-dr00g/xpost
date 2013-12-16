/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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
 * @brief Free the given font.
 *
 * @param[inout] face The font face.
 *
 * This function frees the memory stored by @p face.
 *
 * @see xpost_font_face_new_from_name()
 */
void xpost_font_face_free(void *face);

/**
 * @brief Scale the given font.
 * @param[in] face The font face.
 * @param[in] scale The scale factor in point.
 *
 * This function scales the font @p face to size @p scale in point
 * unit.
 */
void xpost_font_face_scale(void *face, real scale);

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

void xpost_font_face_glyph_buffer_get(void *face, unsigned char **buffer, int *rows, int *width, int *pitch, char *pixel_mode, int *left, int *top, long *advance_x, long *advance_y);

/**
 * @brief Check if the given font has kerning feature.
 *
 * @param[in] face The font face.
 * @return 1 if the font has kerning feature, 0 otherwise.
 *
 * This function checks if @p face has kerning feature. It returns 1
 * if so, 0 otherwise.
 *
 * @see xpost_font_face_kerning_delta_get()
 */
int xpost_font_face_kerning_has(void *face);

/**
 * @brief Retrieve the kerning vector of the given glyph.
 *
 * @param[in] face The font face.
 * @param[in] glyph_previous The previous glyph.
 * @param[in] glyph_index The current glyph.
 * @param[out] delta_x The horizontal component of the kerning vector.
 * @param[out] delta_y The vertical component of the kerning vector.
 * @return 1 on success, 0 otherwise.
 *
 * This function stores the kerning vector of the glyph
 * @p glyph_index computed from @p glyph_previous  (in font @p face)
 * in @p delta_x and @p delta_y. It returns 1 on success, 0 otherwise.
 *
 * @see xpost_font_face_kerning_has()
 */
int xpost_font_face_kerning_delta_get(void *face, unsigned int glyph_previous, unsigned int glyph_index, long *delta_x, long *delta_y);

#endif
