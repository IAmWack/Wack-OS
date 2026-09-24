/**
 * @file    wackos_gfx.h
 * @author  IAmWack
 * @brief   Слой рисования Wack-OS: всё, что умеет класть пиксели, но не знает, куда они потом уедут.
 *
 * Рисуем всегда в кадровый буфер в ОЗУ (172x320 RGB565 = 110 КБ), а
 * "показать" — отдельный шаг, который делает display.c: на ESP32-S3 это
 * esp_lcd_panel_draw_bitmap, в симуляторе на ноутбуке — окно X11.
 * Именно поэтому картинка на ноутбуке пиксель-в-пиксель совпадает с
 * тем, что потом будет на ST7789: весь рендер общий, различается
 * только последний шаг вывода.
 *
 * Здесь нет ни одного #include с железом — это принципиально
 * (docs/ARCHITECTURE.md, слой ниже не знает о слое выше).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "wackos_font.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define GFX_W 172
#define GFX_H 320

/** @brief  Собрать RGB565 из 8-битных r/g/b (нижние биты g/b отбрасываются). */
#define GFX_RGB(r, g, b) \
    ((uint16_t)(((uint16_t)((r) & 0xF8) << 8) | ((uint16_t)((g) & 0xFC) << 3) | ((uint16_t)(b) >> 3)))

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Указатель на кадровый буфер (GFX_W * GFX_H значений RGB565). */
uint16_t *gfx_framebuffer(void);

/* ---- Примитивы ---- */

void gfx_clear(uint16_t color);
void gfx_pixel(int x, int y, uint16_t color, uint8_t alpha);
void gfx_fill_rect(int x, int y, int w, int h, uint16_t color, uint8_t alpha);

/* Вертикальный градиент по всему экрану: цвет top сверху, bottom снизу. */
void gfx_gradient_v(int y0, int y1, uint16_t top, uint16_t bottom);

/* Скруглённый прямоугольник со сглаженными углами. */
void gfx_round_rect(int x, int y, int w, int h, int r, uint16_t color, uint8_t alpha);
/* Он же, но только контур толщиной t (внутрь). */
void gfx_round_rect_outline(int x, int y, int w, int h, int r, int t,
                            uint16_t color, uint8_t alpha);
/* Он же с вертикальным градиентом заливки — для "объёмных" плашек. */
void gfx_round_rect_gradient(int x, int y, int w, int h, int r,
                             uint16_t top, uint16_t bottom, uint8_t alpha);

void gfx_circle(int cx, int cy, int r, uint16_t color, uint8_t alpha);
/* Эллипс — основа всего, из чего собран Астольфо: голова, глаза, пряди. */
void gfx_ellipse(int cx, int cy, int rx, int ry, uint16_t color, uint8_t alpha);
/* Дуга: углы в градусах, 0 — вправо, отсчёт против часовой. Толщина t. */
void gfx_arc(int cx, int cy, int r, int t, int a0, int a1, uint16_t color, uint8_t alpha);
/* Отрезок толщиной w со скруглёнными концами. */
void gfx_line(int x0, int y0, int x1, int y1, int w, uint16_t color, uint8_t alpha);
/* Треугольник по трём вершинам. */
void gfx_triangle(int x0, int y0, int x1, int y1, int x2, int y2,
                  uint16_t color, uint8_t alpha);

/* ---- Милота ---- */

/* Сердечко высотой h с центром в (cx, cy). */
void gfx_heart(int cx, int cy, int h, uint16_t color, uint8_t alpha);
/* Четырёхлучевая искорка/блик радиусом r. */
void gfx_sparkle(int cx, int cy, int r, uint16_t color, uint8_t alpha);
/* Бантик шириной w (две петли + узелок). */
void gfx_bow(int cx, int cy, int w, uint16_t color, uint8_t alpha);
/* Кошачье ушко: треугольник с закруглённой вершиной, внутри — розовое. */
void gfx_cat_ear(int x, int y, int w, int h, bool flip,
                 uint16_t outer, uint16_t inner, uint8_t alpha);

/* ---- Текст (UTF-8) ---- */

void gfx_text(const wackos_font_t *font, int x, int y, const char *utf8,
              uint16_t color, uint8_t alpha);
/* Текст, отцентрованный по горизонтали относительно cx. */
void gfx_text_center(const wackos_font_t *font, int cx, int y, const char *utf8,
                     uint16_t color, uint8_t alpha);
/* Текст с мягкой тенью на 1px вниз — так буквы читаются на пёстром фоне. */
void gfx_text_shadow(const wackos_font_t *font, int x, int y, const char *utf8,
                     uint16_t color, uint16_t shadow, uint8_t alpha);
int  gfx_text_width(const wackos_font_t *font, const char *utf8);

/*
 * Нарисовать текст, обрезав его многоточием по ширине max_w.
 * Возвращает фактическую ширину нарисованного.
 */
int gfx_text_ellipsis(const wackos_font_t *font, int x, int y, const char *utf8,
                      int max_w, uint16_t color, uint8_t alpha);

/*
 * Затемнить весь кадр (level 0..255, 255 — без изменений). На плате
 * яркость крутится подсветкой, а в симуляторе подсветки нет — там этим
 * рисуется то же самое, чтобы ползунок в настройках было видно.
 */
void gfx_dim(uint8_t level);

/* ---- Утилиты цвета ---- */

/* Смешать два цвета: t=0 -> a, t=255 -> b. */
uint16_t gfx_mix(uint16_t a, uint16_t b, uint8_t t);
/* Осветлить/затемнить цвет (amount 0..255). */
uint16_t gfx_lighten(uint16_t c, uint8_t amount);
uint16_t gfx_darken(uint16_t c, uint8_t amount);
