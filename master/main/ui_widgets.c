/**
 * @file    ui_widgets.c
 * @author  IAmWack
 * @brief   Общие виджеты интерфейса: карточка, иконки, список, текст, облачко.
 *
 * Смысл в том, чтобы экраны занимались содержанием, а не пикселями:
 * все списки в системе листаются, подсвечиваются и прокручиваются
 * одинаково, потому что это один и тот же код. Публичные функции этого
 * файла объявлены в ui_screen.h.
 */
#include "ui_screen.h"
#include "theme.h"
#include "wackos_gfx.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/** @brief  Плавная пульсация [0..1] с заданным периодом и фазой (синус). */
static float pulse(uint32_t t_ms, float period_ms, float phase)
{
    return 0.5f + 0.5f * sinf((float)t_ms / period_ms * 6.2831853f + phase);
}

/* Плавное приближение a к b (экспоненциальное сглаживание). */
static float approach(float a, float b, float dt_ms, float speed)
{
    return a + (b - a) * (1.0f - expf(-speed * dt_ms / 1000.0f));
}

/* ============================================================================
 *  Public API — карточка и иконки
 * ============================================================================ */

/** @brief  Карточка со скруглениями и тенью — основа всех экранов. */
void ui_card(int x, int y, int w, int h, uint8_t alpha)
{
    gfx_round_rect(x + 1, y + 3, w, h, 16, TH->shadow, 34);
    gfx_round_rect_gradient(x, y, w, h, 16, TH->card,
                            gfx_mix(TH->card, TH->bg_bot, 70), alpha);
    gfx_round_rect_outline(x, y, w, h, 16, 2, TH->card_edge, 200);
}

/**
 * @brief   Иконка размером s с центром в (cx, cy).
 * @param   icon   Какую иконку рисовать (см. ui_icon_t).
 * @param   cx,cy  Центр иконки.
 * @param   s      Размер иконки.
 * @param   color  Цвет.
 * @param   alpha  Непрозрачность 0..255.
 */
void ui_icon(ui_icon_t icon, int cx, int cy, int s, uint16_t color, uint8_t alpha)
{
    switch (icon) {
        case UI_ICON_HEART:
            gfx_heart(cx, cy, s, color, alpha);
            break;
        case UI_ICON_BOW:
            gfx_bow(cx, cy, s + 4, color, alpha);
            break;
        case UI_ICON_SPARK:
            gfx_sparkle(cx, cy, s / 2 + 1, color, alpha);
            break;
        case UI_ICON_GEAR: {
            /* Шестерёнка: кружок с зубчиками по кругу. */
            int r = s / 2;
            for (int i = 0; i < 6; i++) {
                float a = i * 3.14159265f / 3.0f;
                gfx_circle(cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r),
                           r / 3 + 1, color, alpha);
            }
            gfx_circle(cx, cy, r, color, alpha);
            gfx_circle(cx, cy, r / 2, TH->card, alpha);
            break;
        }
        case UI_ICON_PLUG: {
            /* Штекер: корпус со скруглением и два усика. */
            int r = s / 2;
            gfx_round_rect(cx - r, cy - r / 2, r * 2, r + 2, 3, color, alpha);
            gfx_line(cx - r / 2, cy - r - 2, cx - r / 2, cy - r / 2, 2, color, alpha);
            gfx_line(cx + r / 2, cy - r - 2, cx + r / 2, cy - r / 2, 2, color, alpha);
            gfx_line(cx, cy + r / 2, cx, cy + r + 2, 2, color, alpha);
            break;
        }
        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Список                                                              */
/* ------------------------------------------------------------------ */

/** @brief  Инициализировать список: сбросить состояние на count пунктов. */
void ui_list_init(ui_list_t *l, size_t count, int visible_rows, int row_h)
{
    l->count = count;
    l->visible_rows = visible_rows;
    l->row_h = row_h;
    l->editing = false;
    if (l->selected >= count) {
        l->selected = 0;
    }
    l->sel_anim = (float)l->selected;
    l->scroll_anim = 0.0f;
    l->last_ms = 0;
}

/**
 * @brief   Сдвинуть выделение в списке на delta пунктов, по кругу.
 * @param   l      Список.
 * @param   delta  Смещение (может быть отрицательным).
 */
void ui_list_move(ui_list_t *l, int delta)
{
    if (l->count == 0) {
        return;
    }
    long next = (long)l->selected + delta;
    /* По кругу: на семи пунктах это удобнее, чем упираться в края. */
    while (next < 0) {
        next += (long)l->count;
    }
    l->selected = (size_t)(next % (long)l->count);
}

/**
 * @brief   Нарисовать список: выделение, плавная прокрутка, полоска справа.
 * @param   l    Список.
 * @param   y0   Верх области списка по Y.
 * @param   row  Колбэк, заполняющий содержимое строки по индексу.
 * @param   t    Текущее время, для анимаций (пульсация выделения и т.п.).
 */
void ui_list_draw(ui_list_t *l, int y0, ui_row_fn row, uint32_t t)
{
    int pitch = l->row_h + ROW_GAP;

    float dt = (l->last_ms == 0) ? 16.0f : (float)(t - l->last_ms);
    if (dt > 200.0f) {
        dt = 200.0f; /* после долгой паузы не делаем рывок */
    }
    l->last_ms = t;

    l->sel_anim = approach(l->sel_anim, (float)l->selected, dt, 14.0f);

    /* Прокрутка: держим выделение внутри окна видимых строк. */
    float target = l->scroll_anim;
    if ((float)l->selected < target) {
        target = (float)l->selected;
    } else if ((float)l->selected > target + (l->visible_rows - 1)) {
        target = (float)l->selected - (l->visible_rows - 1);
    }
    if ((int)l->count <= l->visible_rows) {
        target = 0.0f;
    }
    l->scroll_anim = approach(l->scroll_anim, target, dt, 12.0f);

    const wackos_font_t *f = &wackos_font_body;
    const wackos_font_t *fv = &wackos_font_small;

    for (size_t i = 0; i < l->count; i++) {
        int y = y0 + (int)(((float)i - l->scroll_anim) * pitch + 0.5f);
        if (y + l->row_h < y0 - 2 || y > y0 + l->visible_rows * pitch) {
            continue; /* уехало за пределы окна списка */
        }

        ui_row_t r = { .icon = UI_ICON_NONE };
        r.title[0] = r.value[0] = '\0';
        row(i, &r);

        bool sel = (i == l->selected);
        int text_y = y + (l->row_h - f->line_height) / 2;
        int val_w = r.value[0] ? gfx_text_width(fv, r.value) : 0;

        if (sel) {
            float glow = pulse(t, 1400.0f, 0.0f);
            gfx_round_rect(LIST_X - 3, y - 3, LIST_W + 6, l->row_h + 6, 17,
                           TH->sel_glow, (uint8_t)(50 + glow * 60));
            gfx_round_rect(LIST_X + 1, y + 3, LIST_W, l->row_h, 15, TH->shadow, 45);
            gfx_round_rect_gradient(LIST_X, y, LIST_W, l->row_h, 15,
                                    TH->sel_top, TH->sel_bot, 255);
            gfx_round_rect_outline(LIST_X, y, LIST_W, l->row_h, 15, 2, TH->card_edge, 230);

            /* Курсор — бантик, а не стрелка. */
            gfx_bow(LIST_X + 16, y + l->row_h / 2, 14 + (int)(glow * 2.0f), TH->card, 255);
            gfx_sparkle(LIST_X + LIST_W - 12, y + 8, 3, TH->card,
                        (uint8_t)(110 + pulse(t, 900.0f, 0.0f) * 145));
        } else {
            gfx_round_rect(LIST_X, y, LIST_W, l->row_h, 15, TH->card, 120);
            gfx_round_rect_outline(LIST_X, y, LIST_W, l->row_h, 15, 1, TH->card_edge, 150);
            if (r.icon != UI_ICON_NONE) {
                ui_icon(r.icon, LIST_X + 16, y + l->row_h / 2, 10, TH->accent_soft, 230);
            } else {
                gfx_heart(LIST_X + 15, y + l->row_h / 2, 8, TH->accent_soft, 220);
            }
        }

        uint16_t tc = sel ? TH->text_on_sel : TH->text;
        int text_x = LIST_X + 30;
        int max_w = LIST_W - 40 - (val_w ? val_w + 10 : 0);
        gfx_text_ellipsis(f, text_x, text_y, r.title, max_w, tc, sel ? 255 : 235);

        if (r.value[0]) {
            int vx = LIST_X + LIST_W - 12 - val_w;
            int vy = y + (l->row_h - fv->line_height) / 2;
            if (sel && l->editing) {
                /* Режим правки: стрелки показывают, что крутилка меняет
                 * значение, а не выбор пункта. */
                float p = pulse(t, 600.0f, 0.0f);
                uint8_t a = (uint8_t)(140 + p * 115);
                gfx_triangle(vx - 12, vy + fv->line_height / 2,
                             vx - 5, vy + 1, vx - 5, vy + fv->line_height - 1,
                             TH->card, a);
                gfx_triangle(vx + val_w + 12, vy + fv->line_height / 2,
                             vx + val_w + 5, vy + 1,
                             vx + val_w + 5, vy + fv->line_height - 1, TH->card, a);
            }
            gfx_text(fv, vx, vy, r.value, sel ? TH->text_on_sel : TH->text_dim, 240);
        }
    }

    /* Полоска прокрутки. */
    if ((int)l->count > l->visible_rows) {
        int track_x = GFX_W - 9;
        int track_h = l->visible_rows * pitch - ROW_GAP;
        gfx_round_rect(track_x, y0, 4, track_h, 2, TH->card, 110);

        int thumb_h = (int)(track_h * l->visible_rows / (int)l->count);
        if (thumb_h < 14) {
            thumb_h = 14;
        }
        float max_scroll = (float)((int)l->count - l->visible_rows);
        float pos = (max_scroll > 0.0f) ? (l->scroll_anim / max_scroll) : 0.0f;
        if (pos < 0.0f) pos = 0.0f;
        if (pos > 1.0f) pos = 1.0f;
        gfx_round_rect(track_x, y0 + (int)((track_h - thumb_h) * pos), 4, thumb_h, 2,
                       TH->accent, 230);
    }
}

/* ------------------------------------------------------------------ */
/* Текст блоком и облачко                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief   Перенос по словам: скопировать в out кусок текста, влезающий в max_w.
 * @param   f         Шрифт.
 * @param   text      Исходный текст.
 * @param   max_w     Доступная ширина в пикселях.
 * @param   out       Буфер для строки, которая влезла.
 * @param   out_size  Размер буфера out.
 * @return  Указатель на остаток text после перенесённой части, либо NULL,
 *          если весь текст поместился.
 */
static const char *wrap_word(const wackos_font_t *f, const char *text, int max_w,
                             char *out, size_t out_size)
{
    size_t best_break = 0;
    size_t fitted = 0;
    int w = 0;
    size_t i = 0;

    while (text[i]) {
        char ch[5] = {0};
        size_t len = 1;
        unsigned char c = (unsigned char)text[i];
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        memcpy(ch, &text[i], len);

        w += gfx_text_width(f, ch);
        if (w > max_w) {
            break;
        }
        i += len;
        fitted = i;
        if (text[i - len] == ' ' || text[i - len] == ',' || text[i - len] == ';') {
            best_break = i;
        }
    }

    if (!text[i]) {
        size_t n = strlen(text);
        if (n >= out_size) {
            n = out_size - 1;
        }
        memcpy(out, text, n);
        out[n] = '\0';
        return NULL;
    }

    size_t cut = best_break ? best_break : fitted;
    if (cut == 0) {
        cut = 1;
    }
    if (cut >= out_size) {
        cut = out_size - 1;
    }
    memcpy(out, text, cut);
    out[cut] = '\0';

    const char *rest = text + cut;
    while (*rest == ' ') {
        rest++;
    }
    return *rest ? rest : NULL;
}

/* ============================================================================
 *  Public API — текст блоком и облачко
 * ============================================================================ */

/**
 * @brief   Текст с переносом по словам.
 * @param   f          Шрифт.
 * @param   x, y       Позиция первой строки.
 * @param   w          Доступная ширина в пикселях.
 * @param   max_lines  Максимум строк.
 * @param   text       Текст.
 * @param   color      Цвет текста.
 * @return  Число нарисованных строк.
 */
int ui_text_block(const wackos_font_t *f, int x, int y, int w, int max_lines,
                  const char *text, uint16_t color)
{
    const char *rest = text;
    int line = 0;
    while (rest && line < max_lines) {
        char buf[72];
        rest = wrap_word(f, rest, w, buf, sizeof(buf));
        gfx_text(f, x, y + line * (f->line_height + 2), buf, color, 255);
        line++;
    }
    return line;
}

/**
 * @brief   Строка "поле ....... значение" внутри карточки.
 * @param   x, y   Позиция строки.
 * @param   w      Доступная ширина.
 * @param   label  Подпись слева.
 * @param   value  Значение справа (обрезается многоточием, если не влезает).
 * @param   color  Цвет значения.
 */
void ui_info_line(int x, int y, int w, const char *label, const char *value,
                  uint16_t color)
{
    const wackos_font_t *f = &wackos_font_small;
    int label_w = gfx_text_width(f, label);
    gfx_text(f, x, y, label, TH->text_dim, 230);

    int avail = w - label_w - 8;
    int value_w = gfx_text_width(f, value);
    if (value_w > avail) {
        gfx_text_ellipsis(f, x + label_w + 8, y, value, avail, color, 245);
    } else {
        gfx_text(f, x + w - value_w, y, value, color, 245);
    }
}

/**
 * @brief   Облачко с репликой (хвостик вниз-влево, к Астольфо).
 * @param   x, y  Верхний левый угол облачка.
 * @param   w     Ширина облачка.
 * @param   text  Текст реплики (переносится по словам, максимум 2 строки).
 */
void ui_bubble(int x, int y, int w, const char *text)
{
    const wackos_font_t *f = &wackos_font_small;
    int h = f->line_height * 2 + 14;

    gfx_round_rect(x + 1, y + 2, w, h, 12, TH->shadow, 30);
    gfx_round_rect(x, y, w, h, 12, TH->card, 235);
    gfx_round_rect_outline(x, y, w, h, 12, 1, TH->card_edge, 220);
    /* Хвостик к персонажу — влево-вниз. */
    gfx_triangle(x + 4, y + h - 10, x + 16, y + h - 4, x - 6, y + h + 6, TH->card, 235);

    ui_text_block(f, x + 9, y + 7, w - 18, 2, text, TH->text);
}
