/*
 * Астольфо — не картинка, а состояние системы.
 *
 * Рисуется кодом из примитивов wackos_gfx: нет ни PNG, ни SD-карты, ни
 * декодера — на любой размер отрисуется чисто, и в прошивке это стоит
 * ноль байт ассетов. Настроение переключает логика по событиям
 * протокола (ответил slave -> happy, молчит -> sad, идёт запрос ->
 * thinking), а простой сам уводит его в sleep.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ASTOLFO_IDLE = 0,  /* дышит, моргает */
    ASTOLFO_HAPPY,     /* получилось */
    ASTOLFO_SAD,       /* slave не ответил / ошибка */
    ASTOLFO_THINKING,  /* идёт запрос */
    ASTOLFO_SLEEP,     /* долго ничего не происходит */
} astolfo_mood_t;

void           astolfo_set_mood(astolfo_mood_t mood);
astolfo_mood_t astolfo_mood(void);

/* Пользователь что-то сделал — сбросить таймер засыпания. */
void astolfo_note_activity(void);

/*
 * Раз в кадр: возвращает happy/sad в idle через пару секунд и уводит в
 * sleep после долгого простоя.
 */
void astolfo_tick(uint32_t now_ms);

/*
 * Нарисовать бюст: h — высота головы, всё остальное (волосы, коса,
 * плечи, плащ) считается от неё. Целиком фигура занимает примерно
 * ASTOLFO_BUST_H(h) по вертикали от y вниз.
 *
 * Один и тот же код рисует и маленького Астольфо в углу меню, и
 * большого на экране загрузки — разницу делает только h.
 */
#define ASTOLFO_BUST_H(h) ((h) * 23 / 10)

void astolfo_draw(int cx, int y, int h, uint32_t t);

/* Реплика персонажа под текущее настроение — для облачка в меню. */
const char *astolfo_line(void);
