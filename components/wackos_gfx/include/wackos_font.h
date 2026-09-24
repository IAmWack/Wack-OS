/**
 * @file    wackos_font.h
 * @author  IAmWack
 * @brief   Растровый шрифт Wack-OS: 4 бита альфы на пиксель (16 градаций сглаживания).
 *
 * Таблицы лежат в wackos_font_data.c и генерируются скриптом
 * tools/gen_font.py из обычного TTF/OTF — руками их не правят.
 *
 * Этот слой не знает ни про экран, ни про ESP: только данные глифов.
 * Рисованием занимается wackos_gfx.c.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef struct {
    uint16_t cp;       /* кодовая точка Unicode (ASCII + кириллица) */
    uint32_t offset;   /* смещение битмапа в общем блобе шрифта */
    uint8_t  w;        /* ширина битмапа, px */
    uint8_t  h;        /* высота битмапа, px */
    int16_t  left;     /* сдвиг битмапа от точки вставки по X */
    int16_t  top;      /* сдвиг битмапа от верха строки по Y */
    uint8_t  advance;  /* на сколько сдвинуться до следующего символа */
} wackos_glyph_t;

typedef struct {
    const wackos_glyph_t *glyphs;
    size_t                glyph_count;
    const uint8_t        *bitmaps;   /* 4bpp, по строкам, с выравниванием на байт */
    uint8_t               line_height;
    uint8_t               baseline;  /* расстояние от верха строки до базовой линии */
} wackos_font_t;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/* Три начертания: мелкое (подписи), основное (меню), крупное (заголовок). */
extern const wackos_font_t wackos_font_small;
extern const wackos_font_t wackos_font_body;
extern const wackos_font_t wackos_font_title;

/** @brief  Найти глиф по кодовой точке; NULL, если такого символа в шрифте нет. */
const wackos_glyph_t *wackos_font_glyph(const wackos_font_t *font, uint32_t cp);
