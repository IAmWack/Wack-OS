/*
 * Слой дисплея. Знает про ST7789/SPI — и больше ни про что.
 *
 * Рисование сюда не входит: весь рендер идёт в кадровый буфер
 * wackos_gfx (components/wackos_gfx), а display отвечает только за то,
 * чтобы готовый кадр оказался на матрице. Ровно по этой границе
 * проходит подмена в симуляторе: sim/sim_display.c реализует те же две
 * функции, но выводит кадр в окно на ноутбуке.
 */
#pragma once

#include "wackos_gfx.h"

#define LCD_WIDTH  GFX_W
#define LCD_HEIGHT GFX_H

void display_init(void);

/*
 * Яркость подсветки, 10..100 %. На плате это ШИМ на PIN_LCD_BL,
 * в симуляторе — затемнение кадра (подсветки-то нет, а ползунок в
 * настройках должен что-то делать).
 */
void display_set_brightness(uint8_t percent);

/* Отправить текущий кадровый буфер gfx на экран целиком. */
void display_flush(void);
