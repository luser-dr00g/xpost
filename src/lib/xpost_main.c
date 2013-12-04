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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "xpost_log.h"
#include "xpost_object.h"
#include "xpost_memory.h"
#include "xpost_font.h"
#include "xpost_main.h"

static int _xpost_init_count = 0;

int
xpost_init(void)
{
    if (++_xpost_init_count != 1)
        return _xpost_init_count;

    if (!xpost_log_init())
        return --_xpost_init_count;

    if (!xpost_memory_init())
        return --_xpost_init_count;

    if (!xpost_font_init())
        return --_xpost_init_count;

    return _xpost_init_count;
}

int
xpost_quit(void)
{
    if (_xpost_init_count <= 0)
    {
        XPOST_LOG_ERR("Init count not greater than 0 in shutdown.");
        return 0;
    }

    if (--_xpost_init_count != 0)
        return _xpost_init_count;

    xpost_font_quit();
    xpost_log_quit();

    return _xpost_init_count;
}

void
xpost_version_get(int *maj, int *min, int *mic)
{
    if (maj) *maj = XPOST_VERSION_MAJ;
    if (min) *min = XPOST_VERSION_MIN;
    if (mic) *mic = XPOST_VERSION_MIC;
}
