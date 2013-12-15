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

void *xpost_font_face_new_from_name(const char *name);

void xpost_font_face_free(void *face);

void xpost_font_face_scale(void *face, real scale);

int xpost_font_face_kerning_has(void *face);

int xpost_font_face_kerning_delta_get(void *face, unsigned int previous, unsigned int glyph_index, long *delta_x, long *delta_y);

#endif
