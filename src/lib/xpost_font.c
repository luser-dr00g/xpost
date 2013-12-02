/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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

#include <stdlib.h>

#ifdef HAVE_FONT
# include <fontconfig/fontconfig.h>

# include <ft2build.h>
# include FT_FREETYPE_H
#endif

#include "xpost_log.h"
#include "xpost_font.h"

struct _Xpost_Font_Face
{
#ifdef HAVE_FONT
    FT_Face face;
#endif
};

#ifdef HAVE_FONT
static FcConfig *_xpost_font_fc_config = NULL;
static FT_Library _xpost_font_ft_library = NULL;
#endif

int
xpost_font_init(void)
{
#ifdef HAVE_FONT
    FT_Error err_ft;
    FcBool err_fc;

    err_ft = FT_Init_FreeType(&_xpost_font_ft_library);
    if (err_ft)
        return 0;

    err_fc = FcInit();
    if (!err_fc)
    {
        FT_Done_FreeType(_xpost_font_ft_library);
        return 0;
    }

    _xpost_font_fc_config = FcInitLoadConfigAndFonts();
#endif
    return 1;
}

void
xpost_font_quit(void)
{
#ifdef HAVE_FONT
    FcConfigDestroy(_xpost_font_fc_config);
    FcFini();
    FT_Done_FreeType(_xpost_font_ft_library);
#endif
}

Xpost_Font_Face *
xpost_font_face_new_from_name(const char *name)
{
#ifdef HAVE_FONT
    char *file;
    FcPattern *pattern;
    FcPattern *match;
    Xpost_Font_Face *face;
    FcResult result;
    FT_Error err;
    int idx;

    face = (Xpost_Font_Face *)malloc(sizeof(Xpost_Font_Face));
    if (!face)
        return NULL;

    /* FIXME: parse name first ? */

    pattern = FcNameParse((const FcChar8 *)name);
    if (!pattern)
        goto free_face;

    if (!FcConfigSubstitute (_xpost_font_fc_config, pattern, FcMatchPattern))
        goto destroy_pattern;

    FcDefaultSubstitute(pattern);
    match = FcFontMatch(_xpost_font_fc_config, pattern, &result);
    if (result != FcResultMatch)
        goto destroy_pattern;

    result = FcPatternGetString(match, FC_FILE, 0, (FcChar8 **)&file);
    if (result != FcResultMatch)
        goto destroy_match;

    XPOST_LOG_INFO("Font %s found in file %s", name, file);

    result = FcPatternGetInteger(match, FC_INDEX, 0, &idx);
    if (result != FcResultMatch)
        goto destroy_match;

    XPOST_LOG_INFO("Font %s has index %d", name, idx);

    err = FT_New_Face(_xpost_font_ft_library, file, idx, &face->face) ;
    if (err == FT_Err_Unknown_File_Format)
    {
        XPOST_LOG_ERR("Font format unsupported");
        goto destroy_match;
    }
    else if (err)
    {
        XPOST_LOG_ERR("Font file %s can not be opened or read or is broken", file);
        goto destroy_match;
    }

    FcPatternDestroy(match);
    FcPatternDestroy(pattern);

    return face;

  destroy_match:
    FcPatternDestroy(match);
  destroy_pattern:
    FcPatternDestroy(pattern);
  free_face:
    free(face);
#else
    (void)name;
#endif

    return NULL;
}

void
xpost_font_face_free(Xpost_Font_Face *face)
{
#ifdef HAVE_FONT
    if (!face)
        return;

    FT_Done_Face(face->face);
    free(face);
#else
    (void)face;
#endif
}
