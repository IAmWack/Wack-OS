/**
 * @file    wackos_font.c
 * @author  IAmWack
 * @brief   Поиск глифа растрового шрифта по кодовой точке Unicode.
 *
 * Часть components/wackos_gfx. Сами таблицы глифов и битмапов лежат
 * в сгенерированном wackos_font_data.c — здесь только логика поиска.
 */
#include "wackos_font.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Найти глиф в шрифте по кодовой точке.
 * @param   font  Шрифт (wackos_font_small/body/title).
 * @param   cp    Кодовая точка Unicode.
 * @return  Указатель на глиф либо NULL, если такого символа в шрифте нет.
 *
 * Таблица отсортирована по кодовой точке (так её пишет
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
