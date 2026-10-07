/**
 * @file    mascot.c
 * @author  IAmWack
 * @brief   Диспетчер маскота текущей темы (см. mascot.h).
 *
 * Ничего не рисует и не хранит сам — только смотрит на TH->mascot
 * (theme.h) и перенаправляет вызов в astolfo.c, razor.c или venti.c,
 * переводя общий mascot_mood_t в mood-enum конкретного персонажа.
 */
#include "mascot.h"
#include "theme.h"
#include "astolfo.h"
#include "razor.h"
#include "venti.h"

#include <stdbool.h>

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Установить настроение маскота текущей темы. */
void mascot_set_mood(mascot_mood_t mood)
{
    switch (TH->mascot) {
        case THEME_MASCOT_RAZOR: razor_set_mood((razor_mood_t)mood); break;
        case THEME_MASCOT_VENTI: venti_set_mood((venti_mood_t)mood); break;
        default:                 astolfo_set_mood((astolfo_mood_t)mood); break;
    }
}

/** @brief  Текущее настроение маскота текущей темы. */
mascot_mood_t mascot_mood(void)
{
    switch (TH->mascot) {
        case THEME_MASCOT_RAZOR: return (mascot_mood_t)razor_mood();
        case THEME_MASCOT_VENTI: return (mascot_mood_t)venti_mood();
        default:                 return (mascot_mood_t)astolfo_mood();
    }
}

/** @brief  Пользователь что-то сделал — сбросить таймер засыпания. */
void mascot_note_activity(void)
{
    switch (TH->mascot) {
        case THEME_MASCOT_RAZOR: razor_note_activity(); break;
        case THEME_MASCOT_VENTI: venti_note_activity(); break;
        default:                 astolfo_note_activity(); break;
    }
}

/**
 * @brief   Раз в кадр: обновить настроение маскота текущей темы.
 * @param   now_ms  Текущее время в миллисекундах (монотонное).
 */
void mascot_tick(uint32_t now_ms)
{
    switch (TH->mascot) {
        case THEME_MASCOT_RAZOR: razor_tick(now_ms); break;
        case THEME_MASCOT_VENTI: venti_tick(now_ms); break;
        default:                 astolfo_tick(now_ms); break;
    }
}

/**
 * @brief   Нарисовать бюст маскота текущей темы.
 * @param   cx  Центр по X.
 * @param   y   Верх фигуры по Y.
 * @param   h   Высота головы — масштаб всей фигуры.
 * @param   t   Время в миллисекундах, для анимации.
 */
void mascot_draw(int cx, int y, int h, uint32_t t)
{
    switch (TH->mascot) {
        case THEME_MASCOT_RAZOR: razor_draw(cx, y, h, t); break;
        case THEME_MASCOT_VENTI: venti_draw(cx, y, h, t); break;
        default:                 astolfo_draw(cx, y, h, t); break;
    }
}

/**
 * @brief   Нарисовать растровый портрет маскота текущей темы.
 * @param   cx, top_y     Центр по X и верх по Y.
 * @param   dst_w, dst_h  Размер на экране.
 */
void mascot_draw_portrait(int cx, int top_y, int dst_w, int dst_h)
{
    switch (TH->mascot) {
        case THEME_MASCOT_RAZOR: razor_draw_portrait(cx, top_y, dst_w, dst_h); break;
        case THEME_MASCOT_VENTI: venti_draw_portrait(cx, top_y, dst_w, dst_h); break;
        default:                 astolfo_draw_portrait(cx, top_y, dst_w, dst_h); break;
    }
}

/** @brief  Реплика маскота текущей темы под текущее настроение. */
const char *mascot_line(void)
{
    switch (TH->mascot) {
        case THEME_MASCOT_RAZOR: return razor_line();
        case THEME_MASCOT_VENTI: return venti_line();
        default:                 return astolfo_line();
    }
}
