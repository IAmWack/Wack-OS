#include "wackos_font.h"

/*
 * Поиск глифа. Таблица отсортирована по кодовой точке (так её пишет
 * tools/gen_font.py), но в ней две несмежные области — латиница и
 * кириллица, — поэтому двоичный поиск, а не индексация по коду.
 */
const wackos_glyph_t *wackos_font_glyph(const wackos_font_t *font, uint32_t cp)
{
    if (!font || font->glyph_count == 0) {
        return NULL;
    }

    size_t lo = 0;
    size_t hi = font->glyph_count - 1;
    while (lo <= hi) {
        size_t mid = (lo + hi) / 2;
        uint32_t mid_cp = font->glyphs[mid].cp;
        if (mid_cp == cp) {
            return &font->glyphs[mid];
        }
        if (mid_cp < cp) {
            lo = mid + 1;
        } else {
            if (mid == 0) {
                break;
            }
            hi = mid - 1;
        }
    }
    return NULL;
}
