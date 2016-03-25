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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define _USE_MATH_DEFINES /* needed for M_PI with Visual Studio */
#include <math.h>

#include "xpost_object.h"
#include "xpost_matrix.h"

#ifdef WANT_LARGE_OBJECT
# define XPOST_ABS(x) fabs(x)
# define XPOST_MOD(x,y) fmod(x, y)
#else
# define XPOST_ABS(x) fabsf(x)
# define XPOST_MOD(x, y) fmodf(x, y)
#endif

/*
 * Fast sinus / cosinus computation :
 * http://devmaster.net/posts/9648/fast-and-accurate-sine-cosine
 */

#define XPOST_EXTRA_PRECISION
static real _sinus(real x)
{
    const real B = (real)(2.0 * M_2_PI);
    const real C = (real)(-M_2_PI * M_2_PI);

    real y = B * x + C * x * XPOST_ABS(x);

#ifdef XPOST_EXTRA_PRECISION
    /* const real Q = 0.775; */
    const real P = 0.225f;

    y = P * (y * XPOST_ABS(y) - y) + y;   /* Q * y + P * y * fabs(y) */
#endif

    return y;
}

static real _cosinus(real x)
{
    x += (real)M_PI_2;

    if (x > M_PI)   /* Original x > pi/2 */
    {
        x -= (real)(2.0 * M_PI);   /* Wrap: cos(x) = cos(x - 2 pi) */
    }

    return _sinus(x);
}

void xpost_matrix_identity(Xpost_Matrix *m)
{
    m->xx = 1.0;
    m->xy = 0.0;
    m->xz = 0.0;

    m->yx = 0.0;
    m->yy = 1.0;
    m->yz = 0.0;
}

void xpost_matrix_translate(Xpost_Matrix *m, real tx, real ty)
{
    m->xx = 1.0;
    m->xy = 0.0;
    m->xz = tx;

    m->yx = 0.0;
    m->yy = 1.0;
    m->yz = ty;
}

void xpost_matrix_scale(Xpost_Matrix *m, real sx, real sy)
{
    m->xx = sx;
    m->xy = 0.0;
    m->xz = 0.0;

    m->yx = 0.0;
    m->yy = sy;
    m->yz = 0.0;
}

void xpost_matrix_rotate(Xpost_Matrix *m, real rad)
{
    real c;
    real s;

    rad = (real)XPOST_MOD(rad + M_PI, 2.0 * M_PI) - M_PI;
    c = _cosinus(rad);
    s = _sinus(rad);

    m->xx = c;
    m->xy = -s;
    m->xz = 0.0;

    m->yx = s;
    m->yy = c;
    m->yz = 0.0;
}

void xpost_matrix_mult(const Xpost_Matrix *m1, const Xpost_Matrix *m2, Xpost_Matrix *m)
{
    m->xx = m1->xx * m2->xx + m1->xy * m2->yx;
    m->xy = m1->xx * m2->xy + m1->xy * m2->yy;
    m->xz = m1->xx * m2->xz + m1->xy * m2->yz + m1->xz;

    m->yx = m1->yx * m2->xx + m1->yy * m2->yx;
    m->yy = m1->yx * m2->xy + m1->yy * m2->yy;
    m->yz = m1->yx * m2->xz + m1->yy * m2->yz + m1->yz;
}
