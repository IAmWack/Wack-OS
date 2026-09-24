/*
 * Тема интерфейса: палитра (переключается в настройках на лету) и
 * геометрия экрана (фиксированная — она про раскладку 172x320, а не
 * про вкус).
 *
 * Цвета живут в структуре, а не в #define, именно ради экрана настроек:
 * пользователь крутит энкодер — тема меняется в следующем же кадре, без
 * перезапуска и без перерисовки чего-то вручную.
 */
#pragma once

#include <stddef.h>

#include "wackos_gfx.h"

typedef struct {
    const char *name;

    /* обои */
    uint16_t bg_top, bg_mid, bg_bot;
    uint16_t deco;        /* летящие сердечки фона */

    /* карточки и текст */
    uint16_t card, card_edge;
    uint16_t text, text_dim, text_on_sel;
    uint16_t shadow;

    /* выделение */
    uint16_t sel_top, sel_bot, sel_glow;

    /* акценты */
    uint16_t accent;      /* сердечки, бантики, курсор */
    uint16_t accent_soft;
    uint16_t ok;          /* «на связи» */
    uint16_t alert;       /* «offline» / ошибка */
} wackos_theme_t;

/* Текущая тема. Весь UI читает цвета только отсюда. */
extern const wackos_theme_t *TH;

size_t      theme_count(void);
size_t      theme_index(void);
void        theme_set(size_t index);
const char *theme_name(size_t index);

/* ---- Геометрия экрана ---- */

#define EAR_H          15
#define HEADER_Y       16   /* шапка сдвинута вниз: над ней живут ушки */
#define HEADER_H       34
#define CONTENT_Y      56
#define CONTENT_H      (HINT_Y - CONTENT_Y - 4)
#define HINT_Y         (GFX_H - 22)   /* строка подсказок по кнопкам */

/* Список пунктов (общий для всех экранов-списков). */
#define LIST_X         8
#define LIST_W         (GFX_W - 2 * LIST_X - 8) /* -8 — полоска прокрутки */
#define ROW_H          28
#define ROW_GAP        4
#define ROW_PITCH      (ROW_H + ROW_GAP)
