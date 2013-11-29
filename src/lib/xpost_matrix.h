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

#ifndef XPOST_MATRIX_H
#define XPOST_MATRIX_H

/*
  [ xx xy xz ]
  | yx yy yz |
  [ 0  0  1  ]
*/

/**
 * @typedef Xpost_Matrix
 */
typedef struct
{
    real xx;
    real xy;
    real xz;

    real yx;
    real yy;
    real yz;
} Xpost_Matrix;

/**
 * @brief Return the identity matrix.
 *
 * @param[out] m The matrix.
 *
 * This function fills the buffer @p m with the identity matrix.
 */
void xpost_matrix_identity(Xpost_Matrix *m);

/**
 * @brief Return the translation matrix.
 *
 * @param[out] m The matrix.
 * @param[in] tx The translation in x.
 * @param[in] ty The translation in y.
 *
 * This function fills the buffer @p m with the translation matrix
 * with the translation (@p tx, @p ty).
 */
void xpost_matrix_translate(Xpost_Matrix *m, real tx, real ty);

/**
 * @brief Return the scale matrix.
 *
 * @param[out] m The matrix.
 * @param[in] sx The scale in x.
 * @param[in] sy The scale in y.
 *
 * This function fills the buffer @p m with the scale matrix
 * with the scale (@p sx, @p sy).
 */
void xpost_matrix_scale(Xpost_Matrix *m, real sx, real sy);

/**
 * @brief Return the rotation matrix.
 *
 * @param[out] m The matrix.
 * @param[in] rad The angle in rad.
 *
 * This function fills the buffer @p m with the rotation matrix
 * with the angle @p rad (in radian).
 */
void xpost_matrix_rotate(Xpost_Matrix *m, real rad);

/**
 * @brief Return the rotation matrix.
 *
 * @param[in] m1 The first matrix.
 * @param[in] m2 The second matrix.
 * @param[out] m The multiplication matrix.
 *
 * This function fills the buffer @p m with the multiplication of
 * @p m1 and @p m2.
 */
void xpost_matrix_mult(const Xpost_Matrix *m1, const Xpost_Matrix *m2, Xpost_Matrix *m);

#endif
