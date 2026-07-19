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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_FONTCONFIG
# include <fontconfig/fontconfig.h>
#endif

#ifdef HAVE_FREETYPE2
# include <ft2build.h>
# include FT_FREETYPE_H
# include FT_OUTLINE_H
# include FT_BBOX_H
#endif

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_object.h"
#include "xpost_font.h"

#ifdef HAVE_FONTCONFIG
static FcConfig *_xpost_font_fc_config = NULL;
#endif

#ifdef HAVE_FREETYPE2
static FT_Library _xpost_font_ft_library = NULL;
#endif

int
xpost_font_init(void)
{
#ifdef HAVE_FREETYPE2
    FT_Error err_ft;

    err_ft = FT_Init_FreeType(&_xpost_font_ft_library);
    if (!err_ft)
    {
# ifdef HAVE_FONTCONFIG
        FcBool err_fc;

        err_fc = FcInit();
        if (!err_fc)
        {
            FT_Done_FreeType(_xpost_font_ft_library);
            return 0;
        }

        _xpost_font_fc_config = FcInitLoadConfigAndFonts();
        if (_xpost_font_fc_config == NULL)
            XPOST_LOG_ERR("cannot load Fc config and fonts");
# endif

        return 1;
    }

    return 0;
#endif

return 1;
}

void
xpost_font_quit(void)
{
#ifdef HAVE_FONTCONFIG
    FcConfigDestroy(_xpost_font_fc_config);
    FcFini();
#endif

#ifdef HAVE_FREETYPE2
    FT_Done_FreeType(_xpost_font_ft_library);
#endif
}

#ifdef HAVE_FREETYPE2
# ifdef HAVE_FONTCONFIG
/* case- and blank-insensitive name comparison, as fontconfig applies
   to family names */
static int
_fc_name_eq(const char *a, const char *b)
{
    while (*a || *b)
    {
        while (*a == ' ') a++;
        while (*b == ' ') b++;
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        if (*a) a++;
        if (*b) b++;
    }
    return 1;
}

/* does the matched font actually carry the requested name (as family
   or PostScript name)? A fuzzy fontconfig match always returns some
   face, so this distinguishes a real hit from a fallback. */
static int
_fc_match_is_exact(FcPattern *match, const char *name)
{
    char *s;
    int i;

    for (i = 0; FcPatternGetString(match, FC_FAMILY, i, (FcChar8 **)&s) == FcResultMatch; i++)
        if (_fc_name_eq(s, name))
            return 1;
    for (i = 0; FcPatternGetString(match, FC_POSTSCRIPT_NAME, i, (FcChar8 **)&s) == FcResultMatch; i++)
        if (_fc_name_eq(s, name))
            return 1;
    return 0;
}

/* PostScript style-variant suffixes translated to fontconfig style
   flags, so that e.g. Helvetica-Bold reaches the bold face of the
   family Helvetica is aliased to */
static const struct { const char *suffix; const char *flags; } _ps_style_suffix[] = {
    { "-BoldItalic",  ":bold:italic" },
    { "-BoldOblique", ":bold:italic" },
    { "-Bold",        ":bold" },
    { "-Italic",      ":italic" },
    { "-Oblique",     ":italic" },
    { "-Roman",       "" },
    { "-Regular",     "" },
};

static FcPattern *
_fc_match_name(const char *name)
{
    FcPattern *pattern;
    FcPattern *match;
    FcResult result;

    pattern = FcNameParse((const FcChar8 *)name);
    if (!pattern)
        return NULL;

    if (!FcConfigSubstitute (_xpost_font_fc_config, pattern, FcMatchPattern))
    {
        FcPatternDestroy(pattern);
        return NULL;
    }

    FcDefaultSubstitute(pattern);
    match = FcFontMatch(_xpost_font_fc_config, pattern, &result);
    FcPatternDestroy(pattern);
    switch (result) {
        case FcResultMatch: break;
        case FcResultNoMatch: goto destroy_match;
        case FcResultTypeMismatch: break;
        case FcResultNoId: break;
        case FcResultOutOfMemory: goto destroy_match;
    }
    return match;

  destroy_match:
    if (match)
        FcPatternDestroy(match);
    return NULL;
}
# endif

static char *
_xpost_font_face_filename_and_index_get(const char *name, int *idx)
{
# ifdef HAVE_FONTCONFIG
    FcPattern *match;
    char *file;
    char *filename;
    FcResult result;

    match = _fc_match_name(name);
    if (!match)
        return NULL;

    /* a fallback match for a PostScript style-variant name loses the
       style (fontconfig reads the whole name as a family); requery as
       family plus style flags so the alias family's styled face wins */
    if (!_fc_match_is_exact(match, name))
    {
        size_t i;

        for (i = 0; i < sizeof(_ps_style_suffix)/sizeof(*_ps_style_suffix); i++)
        {
            const char *sfx = _ps_style_suffix[i].suffix;
            size_t nlen = strlen(name), slen = strlen(sfx);

            if (nlen > slen && strcmp(name + nlen - slen, sfx) == 0)
            {
                char *styled = malloc(nlen + strlen(_ps_style_suffix[i].flags) + 1);
                FcPattern *restyled;

                if (!styled)
                    break;
                memcpy(styled, name, nlen - slen);
                strcpy(styled + nlen - slen, _ps_style_suffix[i].flags);
                restyled = _fc_match_name(styled);
                free(styled);
                if (restyled)
                {
                    FcPatternDestroy(match);
                    match = restyled;
                }
                break;
            }
        }
    }

    result = FcPatternGetString(match, FC_FILE, 0, (FcChar8 **)&file);
    if (result != FcResultMatch)
        goto destroy_match;

    XPOST_LOG_INFO("Font %s found in file %s", name, file);

    result = FcPatternGetInteger(match, FC_INDEX, 0, idx);
    if (result != FcResultMatch)
        goto destroy_match;

    XPOST_LOG_INFO("Font %s has index %d", name, *idx);

    filename = strdup(file);

    FcPatternDestroy(match);

    return filename;

  destroy_match:
    FcPatternDestroy(match);
# endif

    return NULL;
}
#endif

void *
xpost_font_face_new_from_name(const char *name)
{
#ifdef HAVE_FREETYPE2
    FT_Face face;
    FT_Error err;
    char *filename;
    int idx;

    filename = _xpost_font_face_filename_and_index_get(name, &idx);
    if (!filename)
        return NULL;

    err = FT_New_Face(_xpost_font_ft_library, filename, idx, &face) ;
    if (err == FT_Err_Unknown_File_Format)
    {
        XPOST_LOG_ERR("Font format unsupported");
        free(filename);
        return NULL;
    }
    else if (err)
    {
        XPOST_LOG_ERR("Font file %s can not be opened or read or is broken", filename);
        free(filename);
        return NULL;
    }

    free(filename);

    return face;
#else
    (void)name;
#endif

    return NULL;
}

void *
xpost_font_face_new_from_memory(const unsigned char *data, size_t len)
{
#ifdef HAVE_FREETYPE2
    FT_Face face;
    FT_Error err;

    err = FT_New_Memory_Face(_xpost_font_ft_library, data, (FT_Long)len, 0, &face);
    if (err)
    {
        XPOST_LOG_ERR("Font program can not be opened or read or is broken (error : %d)", err);
        return NULL;
    }

    return face;
#else
    (void)data;
    (void)len;
#endif

    return NULL;
}

void
xpost_font_face_get_bbox(void *face, Xpost_Object *bboxarray, real em){
#ifdef HAVE_FREETYPE2
    FT_Face f = face;
    real s = 1.0;

    /* FontBBox belongs to character space, whose scale is a convention
       of the font type (1000 units per em for Type 1, one unit for
       Type 42): normalize the face's design units to the em size the
       caller's dictionary declares through its FontMatrix */
    if (f->units_per_EM > 0)
        s = em / f->units_per_EM;
    bboxarray[0] = xpost_real_cons(f->bbox.xMin * s);
    bboxarray[1] = xpost_real_cons(f->bbox.yMin * s);
    bboxarray[2] = xpost_real_cons(f->bbox.xMax * s);
    bboxarray[3] = xpost_real_cons(f->bbox.yMax * s);
#else
    (void)face;
    (void)bboxarray;
    (void)em;
#endif
}

int
xpost_font_face_units(void *face)
{
#ifdef HAVE_FREETYPE2
    FT_Face f = face;

    return f->units_per_EM > 0 ? f->units_per_EM : 0;
#else
    (void)face;
    return 0;
#endif
}

void
xpost_font_face_free(void *face)
{
#ifdef HAVE_FREETYPE2
    if (!face)
        return;

    FT_Done_Face(face);
#else
    (void)face;
#endif
}

real
xpost_font_face_scale(void *face, real scale)
{
#ifdef HAVE_FREETYPE2
    FT_Face f = face;

    /* Request the scale directly (16.16 pixels per font unit) rather
       than a nominal character size: a nominal request quantizes the
       em to 1/64 pixel, which at the sub-pixel em sizes produced by a
       small user-space size under a modest CTM is a metrics error of
       whole percents.

       FreeType's size machinery is only dependable within a moderate
       band of pixel sizes: a sub-pixel or enormous em makes the
       request fail or degrade silently. Since every glyph is loaded
       unhinted, geometry is linear in the scale, so an extreme
       request is served at a clamped, well-conditioned size instead
       and the caller folds the residual ratio into the face
       transform. Returns the scale actually installed; fixed-size
       faces keep the nominal path. */
    if (f->units_per_EM > 0)
    {
        FT_Size_RequestRec req;
        real base = scale;

        if (base < 8.0)
            base = 8.0;
        else if (base > 2048.0)
            base = 2048.0;

        req.type = FT_SIZE_REQUEST_TYPE_SCALES;
        req.width = req.height =
            (FT_Long)(base * 64.0 * 65536.0 / f->units_per_EM + 0.5);
        req.horiResolution = 0;
        req.vertResolution = 0;
        if (FT_Request_Size(f, &req) == 0)
            return base;
        if (FT_Set_Char_Size(f, 0, (FT_F26Dot6)(base * 64 + 0.5), 72, 72) == 0)
            return base;
    }
    FT_Set_Char_Size(f, 0, (FT_F26Dot6)(scale * 64 + 0.5), 72, 72);
    return scale;
#else
    (void)face;
    (void)scale;
    return scale;
#endif
}

void
xpost_font_face_transform(void *face, float *mat)
{
#ifdef HAVE_FREETYPE2
    FT_Matrix matrix;
    //FT_Vector pen;
    matrix.xx = (FT_Fixed)(mat[0] * 0x10000L);
    matrix.xy = (FT_Fixed)(mat[1] * 0x10000L);
    matrix.yx = (FT_Fixed)(mat[2] * 0x10000L);
    matrix.yy = (FT_Fixed)(mat[3] * 0x10000L);
    //pen.x = (FT_F26Dot6)(mat[4] * 64.0);
    //pen.y = (FT_F26Dot6)(mat[5] * 64.0);
    FT_Set_Transform((FT_Face)face, &matrix, 0);
#else
    (void)face;
    (void)mat;
#endif
}

unsigned int
xpost_font_face_glyph_index_get(void *face, char c)
{
#ifdef HAVE_FREETYPE2
    /* the character code is a byte value: keep 128-255 out of the
       sign extension */
    return FT_Get_Char_Index(face, (unsigned char)c);
#else
    (void)face;
    (void)c;
    return -1;
#endif
}

#ifdef HAVE_FREETYPE2
/* Adobe glyph name -> Unicode for the standard-encoding names, derived from
   ISOLatin1Encoding: over U+0020..U+007E and U+00A0..U+00FF the encoding
   position is the code point (and equals the Adobe glyph list value). Lets a
   named /Encoding select a glyph on a face whose post table stores no names,
   by resolving the name to Unicode and consulting the character map. */
static const struct { const char *name; unsigned short cp; } _xpost_glyph_unicode[] = {
    { "space", 0x0020 },
    { "exclam", 0x0021 },
    { "quotedbl", 0x0022 },
    { "numbersign", 0x0023 },
    { "dollar", 0x0024 },
    { "percent", 0x0025 },
    { "ampersand", 0x0026 },
    { "quoteright", 0x0027 },
    { "parenleft", 0x0028 },
    { "parenright", 0x0029 },
    { "asterisk", 0x002A },
    { "plus", 0x002B },
    { "comma", 0x002C },
    { "minus", 0x002D },
    { "period", 0x002E },
    { "slash", 0x002F },
    { "zero", 0x0030 },
    { "one", 0x0031 },
    { "two", 0x0032 },
    { "three", 0x0033 },
    { "four", 0x0034 },
    { "five", 0x0035 },
    { "six", 0x0036 },
    { "seven", 0x0037 },
    { "eight", 0x0038 },
    { "nine", 0x0039 },
    { "colon", 0x003A },
    { "semicolon", 0x003B },
    { "less", 0x003C },
    { "equal", 0x003D },
    { "greater", 0x003E },
    { "question", 0x003F },
    { "at", 0x0040 },
    { "A", 0x0041 },
    { "B", 0x0042 },
    { "C", 0x0043 },
    { "D", 0x0044 },
    { "E", 0x0045 },
    { "F", 0x0046 },
    { "G", 0x0047 },
    { "H", 0x0048 },
    { "I", 0x0049 },
    { "J", 0x004A },
    { "K", 0x004B },
    { "L", 0x004C },
    { "M", 0x004D },
    { "N", 0x004E },
    { "O", 0x004F },
    { "P", 0x0050 },
    { "Q", 0x0051 },
    { "R", 0x0052 },
    { "S", 0x0053 },
    { "T", 0x0054 },
    { "U", 0x0055 },
    { "V", 0x0056 },
    { "W", 0x0057 },
    { "X", 0x0058 },
    { "Y", 0x0059 },
    { "Z", 0x005A },
    { "bracketleft", 0x005B },
    { "backslash", 0x005C },
    { "bracketright", 0x005D },
    { "asciicircum", 0x005E },
    { "underscore", 0x005F },
    { "quoteleft", 0x0060 },
    { "a", 0x0061 },
    { "b", 0x0062 },
    { "c", 0x0063 },
    { "d", 0x0064 },
    { "e", 0x0065 },
    { "f", 0x0066 },
    { "g", 0x0067 },
    { "h", 0x0068 },
    { "i", 0x0069 },
    { "j", 0x006A },
    { "k", 0x006B },
    { "l", 0x006C },
    { "m", 0x006D },
    { "n", 0x006E },
    { "o", 0x006F },
    { "p", 0x0070 },
    { "q", 0x0071 },
    { "r", 0x0072 },
    { "s", 0x0073 },
    { "t", 0x0074 },
    { "u", 0x0075 },
    { "v", 0x0076 },
    { "w", 0x0077 },
    { "x", 0x0078 },
    { "y", 0x0079 },
    { "z", 0x007A },
    { "braceleft", 0x007B },
    { "bar", 0x007C },
    { "braceright", 0x007D },
    { "asciitilde", 0x007E },
    { "exclamdown", 0x00A1 },
    { "cent", 0x00A2 },
    { "sterling", 0x00A3 },
    { "currency", 0x00A4 },
    { "yen", 0x00A5 },
    { "brokenbar", 0x00A6 },
    { "section", 0x00A7 },
    { "dieresis", 0x00A8 },
    { "copyright", 0x00A9 },
    { "ordfeminine", 0x00AA },
    { "guillemotleft", 0x00AB },
    { "logicalnot", 0x00AC },
    { "hyphen", 0x00AD },
    { "registered", 0x00AE },
    { "macron", 0x00AF },
    { "degree", 0x00B0 },
    { "plusminus", 0x00B1 },
    { "twosuperior", 0x00B2 },
    { "threesuperior", 0x00B3 },
    { "acute", 0x00B4 },
    { "mu", 0x00B5 },
    { "paragraph", 0x00B6 },
    { "periodcentered", 0x00B7 },
    { "cedilla", 0x00B8 },
    { "onesuperior", 0x00B9 },
    { "ordmasculine", 0x00BA },
    { "guillemotright", 0x00BB },
    { "onequarter", 0x00BC },
    { "onehalf", 0x00BD },
    { "threequarters", 0x00BE },
    { "questiondown", 0x00BF },
    { "Agrave", 0x00C0 },
    { "Aacute", 0x00C1 },
    { "Acircumflex", 0x00C2 },
    { "Atilde", 0x00C3 },
    { "Adieresis", 0x00C4 },
    { "Aring", 0x00C5 },
    { "AE", 0x00C6 },
    { "Ccedilla", 0x00C7 },
    { "Egrave", 0x00C8 },
    { "Eacute", 0x00C9 },
    { "Ecircumflex", 0x00CA },
    { "Edieresis", 0x00CB },
    { "Igrave", 0x00CC },
    { "Iacute", 0x00CD },
    { "Icircumflex", 0x00CE },
    { "Idieresis", 0x00CF },
    { "Eth", 0x00D0 },
    { "Ntilde", 0x00D1 },
    { "Ograve", 0x00D2 },
    { "Oacute", 0x00D3 },
    { "Ocircumflex", 0x00D4 },
    { "Otilde", 0x00D5 },
    { "Odieresis", 0x00D6 },
    { "multiply", 0x00D7 },
    { "Oslash", 0x00D8 },
    { "Ugrave", 0x00D9 },
    { "Uacute", 0x00DA },
    { "Ucircumflex", 0x00DB },
    { "Udieresis", 0x00DC },
    { "Yacute", 0x00DD },
    { "Thorn", 0x00DE },
    { "germandbls", 0x00DF },
    { "agrave", 0x00E0 },
    { "aacute", 0x00E1 },
    { "acircumflex", 0x00E2 },
    { "atilde", 0x00E3 },
    { "adieresis", 0x00E4 },
    { "aring", 0x00E5 },
    { "ae", 0x00E6 },
    { "ccedilla", 0x00E7 },
    { "egrave", 0x00E8 },
    { "eacute", 0x00E9 },
    { "ecircumflex", 0x00EA },
    { "edieresis", 0x00EB },
    { "igrave", 0x00EC },
    { "iacute", 0x00ED },
    { "icircumflex", 0x00EE },
    { "idieresis", 0x00EF },
    { "eth", 0x00F0 },
    { "ntilde", 0x00F1 },
    { "ograve", 0x00F2 },
    { "oacute", 0x00F3 },
    { "ocircumflex", 0x00F4 },
    { "otilde", 0x00F5 },
    { "odieresis", 0x00F6 },
    { "divide", 0x00F7 },
    { "oslash", 0x00F8 },
    { "ugrave", 0x00F9 },
    { "uacute", 0x00FA },
    { "ucircumflex", 0x00FB },
    { "udieresis", 0x00FC },
    { "yacute", 0x00FD },
    { "thorn", 0x00FE },
    { "ydieresis", 0x00FF },
};

static long
_xpost_glyph_name_to_unicode(const char *name)
{
    size_t i, n;
    char *end;
    long v;

    /* uniXXXX: exactly four hexadecimal digits */
    if (strncmp(name, "uni", 3) == 0 && strlen(name + 3) == 4)
    {
        v = strtol(name + 3, &end, 16);
        if (*end == '\0' && v >= 0)
            return v;
    }
    /* uXXXX .. uXXXXXX: four to six hexadecimal digits */
    if (name[0] == 'u' && name[1] != 'n')
    {
        n = strlen(name + 1);
        if (n >= 4 && n <= 6 && isxdigit((unsigned char)name[1]))
        {
            v = strtol(name + 1, &end, 16);
            if (*end == '\0' && v >= 0 && v <= 0x10FFFF)
                return v;
        }
    }
    for (i = 0; i < sizeof _xpost_glyph_unicode / sizeof _xpost_glyph_unicode[0]; i++)
        if (strcmp(name, _xpost_glyph_unicode[i].name) == 0)
            return _xpost_glyph_unicode[i].cp;
    return -1;
}
#endif /* HAVE_FREETYPE2 */

unsigned int
xpost_font_face_glyph_name_count(void *face)
{
#ifdef HAVE_FREETYPE2
    if (!FT_HAS_GLYPH_NAMES((FT_Face)face))
        return 0;
    return (unsigned int)((FT_Face)face)->num_glyphs;
#else
    (void)face;
    return 0;
#endif
}

int
xpost_font_face_glyph_name_get(void *face, unsigned int gid, char *buf, int len)
{
#ifdef HAVE_FREETYPE2
    if (!FT_HAS_GLYPH_NAMES((FT_Face)face))
        return 0;
    if (FT_Get_Glyph_Name((FT_Face)face, gid, buf, (FT_UInt)len) != 0)
        return 0;
    return buf[0] != '\0';
#else
    (void)face;
    (void)gid;
    (void)buf;
    (void)len;
    return 0;
#endif
}

unsigned int
xpost_font_face_glyph_name_index_get(void *face, const char *name)
{
#ifdef HAVE_FREETYPE2
    unsigned int gi;
    long uni;

    if (FT_HAS_GLYPH_NAMES((FT_Face)face))
    {
        gi = FT_Get_Name_Index((FT_Face)face, (FT_String *)name);
        if (gi)
            return gi;
    }
    /* no post name for this glyph: resolve the Adobe name to Unicode and
       take it through the character map */
    uni = _xpost_glyph_name_to_unicode(name);
    if (uni >= 0)
        return FT_Get_Char_Index((FT_Face)face, (FT_ULong)uni);
    return 0;
#else
    (void)face;
    (void)name;
    return 0;
#endif
}

#ifdef HAVE_FREETYPE2
/* FT_Outline_Decompose adapter: track the current point, divide the
   26.6 fixed-point coordinates out to pixels, and raise quadratic
   segments to the equivalent cubics so the sink sees one curve form.
   The decomposition starts each contour with a moveto and leaves it
   implicitly closed, so a closepath is synthesized before the next
   contour and after the last. */
struct _outline_walk
{
    const Xpost_Font_Outline_Sink *sink;
    double x, y;
    int open;
};

static int
_outline_moveto(const FT_Vector *to, void *user)
{
    struct _outline_walk *w = user;

    if (w->open)
    {
        int ret = w->sink->closepath(w->sink->user);
        if (ret)
            return ret;
    }
    w->open = 1;
    w->x = to->x / 64.0;
    w->y = to->y / 64.0;
    return w->sink->moveto(w->sink->user, w->x, w->y);
}

static int
_outline_lineto(const FT_Vector *to, void *user)
{
    struct _outline_walk *w = user;

    w->x = to->x / 64.0;
    w->y = to->y / 64.0;
    return w->sink->lineto(w->sink->user, w->x, w->y);
}

static int
_outline_conicto(const FT_Vector *control, const FT_Vector *to, void *user)
{
    struct _outline_walk *w = user;
    double cx = control->x / 64.0;
    double cy = control->y / 64.0;
    double ex = to->x / 64.0;
    double ey = to->y / 64.0;
    /* a quadratic's control point pulls each cubic control 2/3 of the
       way from the respective endpoint */
    double c1x = w->x + (cx - w->x) * (2.0 / 3.0);
    double c1y = w->y + (cy - w->y) * (2.0 / 3.0);
    double c2x = ex + (cx - ex) * (2.0 / 3.0);
    double c2y = ey + (cy - ey) * (2.0 / 3.0);

    w->x = ex;
    w->y = ey;
    return w->sink->curveto(w->sink->user, c1x, c1y, c2x, c2y, ex, ey);
}

static int
_outline_cubicto(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user)
{
    struct _outline_walk *w = user;

    w->x = to->x / 64.0;
    w->y = to->y / 64.0;
    return w->sink->curveto(w->sink->user,
                            control1->x / 64.0, control1->y / 64.0,
                            control2->x / 64.0, control2->y / 64.0,
                            w->x, w->y);
}
#endif


#ifdef HAVE_FREETYPE2
/* The hinter rounds slot->advance to whole pixels and the rounding
   accumulates as horizontal drift across a string. Derive the pen
   advance from the unhinted linear width instead, applied through the
   face's current transform (identity when none is set), and report it
   in 16.16 pixels: squeezing through the slot's 26.6 resolution costs
   up to 1/64 pixel per glyph, a whole percent of a sub-pixel em.
   Bitmap-only glyphs carry no linear width; those widen the slot
   advance. */
static void
_glyph_linear_advance(FT_Face face, long *advance_x, long *advance_y)
{
    FT_GlyphSlot slot = face->glyph;
    FT_Fixed lin = slot->linearHoriAdvance;   /* 16.16 pixels */
    FT_Matrix m;

    if (lin == 0)
    {
        *advance_x = slot->advance.x << 10;   /* 26.6 -> 16.16 */
        *advance_y = slot->advance.y << 10;
        return;
    }
    FT_Get_Transform(face, &m, NULL);
    *advance_x = FT_MulFix(m.xx, lin);
    *advance_y = FT_MulFix(m.yx, lin);
}
#endif

int
xpost_font_face_glyph_outline(void *face, unsigned int glyph_index, const Xpost_Font_Outline_Sink *sink, long *advance_x, long *advance_y)
{
#ifdef HAVE_FREETYPE2
    FT_GlyphSlot slot;
    FT_Outline *outline;
    FT_Error err;
    struct _outline_walk w;

    err = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
    if (err)
    {
        XPOST_LOG_ERR("Can not load glyph (error : %d)", err);
        return 0;
    }
    slot = ((FT_Face)face)->glyph;
    _glyph_linear_advance((FT_Face)face, advance_x, advance_y);
    if (slot->format != FT_GLYPH_FORMAT_OUTLINE)
    {
        XPOST_LOG_ERR("glyph has no outline");
        return 0;
    }
    outline = &slot->outline;

    w.sink = sink;
    w.x = 0;
    w.y = 0;
    w.open = 0;
    {
        FT_Outline_Funcs funcs;

        funcs.move_to = _outline_moveto;
        funcs.line_to = _outline_lineto;
        funcs.conic_to = _outline_conicto;
        funcs.cubic_to = _outline_cubicto;
        funcs.shift = 0;
        funcs.delta = 0;
        err = FT_Outline_Decompose(outline, &funcs, &w);
        if (err)
            return 0;
    }
    if (w.open && sink->closepath(sink->user))
        return 0;
    return 1;
#else
    (void)face;
    (void)glyph_index;
    (void)sink;
    (void)advance_x;
    (void)advance_y;
    return 0;
#endif
}

/* The ink extent of a glyph's outline in 26.6 glyph space (y-up around
   the pen), without rasterizing. An empty outline (a space) reports a
   degenerate box; a glyph with no outline at all (a bitmap strike)
   reports failure so the caller can fall back to rendering. */
int
xpost_font_face_glyph_extents(void *face, unsigned int glyph_index,
                              long *xmin, long *ymin, long *xmax, long *ymax,
                              long *advance_x, long *advance_y)
{
#ifdef HAVE_FREETYPE2
    FT_Error err;
    FT_GlyphSlot slot;
    FT_BBox box;

    err = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
    if (err)
    {
        XPOST_LOG_ERR("Can not load glyph (error : %d)", err);
        return 0;
    }
    slot = ((FT_Face)face)->glyph;
    if (slot->format != FT_GLYPH_FORMAT_OUTLINE)
        return 0;
    FT_Outline_Get_BBox(&slot->outline, &box);
    *xmin = box.xMin;
    *ymin = box.yMin;
    *xmax = box.xMax;
    *ymax = box.yMax;
    _glyph_linear_advance((FT_Face)face, advance_x, advance_y);
    return 1;
#else
    (void)face;
    (void)glyph_index;
    (void)xmin;
    (void)ymin;
    (void)xmax;
    (void)ymax;
    (void)advance_x;
    (void)advance_y;
    return 0;
#endif
}

int
xpost_font_face_glyph_render(void *face, unsigned int glyph_index)
{
#ifdef HAVE_FREETYPE2
    FT_Error err;

    err = FT_Load_Glyph(face, glyph_index, FT_LOAD_FORCE_AUTOHINT);
    if (!err)
    {
        if (((FT_Face)face)->glyph->format != FT_GLYPH_FORMAT_BITMAP)
        {
            err = FT_Render_Glyph(((FT_Face)face)->glyph, FT_RENDER_MODE_NORMAL);
            if (!err)
                return 1;
            else
            {
                XPOST_LOG_ERR("Can not render  non bitmap glyph (error : %d)", err);
                return 0;
            }
        }
        else
            return 1;
    }
    else
    {
        XPOST_LOG_ERR("Can not load glyph (error : %d)", err);
        return 0;
    }
#else
    (void)face;
    (void)glyph_index;
    return 0;
#endif

    return 0;
}

void
xpost_font_face_glyph_buffer_get(void *face, unsigned char **buffer, int *rows, int *width, int *pitch, char *pixel_mode, int *left, int *top, long *advance_x, long *advance_y)
{
#ifdef HAVE_FREETYPE2
    *buffer = ((FT_Face)face)->glyph->bitmap.buffer;
    *rows = ((FT_Face)face)->glyph->bitmap.rows;
    *width = ((FT_Face)face)->glyph->bitmap.width;
    *pitch = ((FT_Face)face)->glyph->bitmap.pitch;
    *pixel_mode = ((FT_Face)face)->glyph->bitmap.pixel_mode;
    *left = ((FT_Face)face)->glyph->bitmap_left;
    *top = ((FT_Face)face)->glyph->bitmap_top;
    _glyph_linear_advance((FT_Face)face, advance_x, advance_y);
#else
    (void)face;
    (void)buffer;
    (void)rows;
    (void)width;
    (void)pitch;
    (void)pixel_mode;
    (void)left;
    (void)top;
    (void)advance_x;
    (void)advance_y;
#endif
}


