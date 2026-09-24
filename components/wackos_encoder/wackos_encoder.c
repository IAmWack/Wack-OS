/**
 * @file    wackos_encoder.c
 * @author  IAmWack
 * @brief   Реализация квадратурного декодера KY-040 и дебаунса кнопки SW.
 *
 * Часть компонента components/wackos_encoder (см. wackos_encoder.h) —
 * чистые функции над состоянием, без единой строчки про железо. Кто
 * именно читает ноги (ISR на ESP32, опрос, что угодно) — дело
 * вызывающего кода.
 */
#include "wackos_encoder.h"

#include <string.h>

/* ============================================================================
 *  Static state
 * ============================================================================ */

/*
 * Таблица квадратурного декодера: индекс = (пред_состояние<<2 | тек_состояние),
 * где состояние = (CLK<<1 | DT). Значение — приращение направления
 * (+1/-1), 0 — недопустимый переход (дребезг), игнорируется.
 */
static const int8_t QUAD_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Запомнить текущее положение ног энкодера, ничего не выдавая наружу.
 * @param   q    Состояние квадратурного декодера.
 * @param   clk  Текущий уровень ноги CLK (0/1).
 * @param   dt   Текущий уровень ноги DT (0/1).
 */
void wackos_quad_reset(wackos_quad_t *q, uint8_t clk, uint8_t dt)
{
    q->prev_state = (uint8_t)((clk << 1) | dt);
    q->accum = 0;
}

/**
 * @brief   Скормить декодеру новое состояние ног CLK/DT.
 * @param   q    Состояние квадратурного декодера.
 * @param   clk  Текущий уровень ноги CLK (0/1).
 * @param   dt   Текущий уровень ноги DT (0/1).
 * @return  WACKOS_ENC_CW/WACKOS_ENC_CCW на границе щелчка, иначе
 *          WACKOS_ENC_NONE. Дребезг и недо-переходы отсекаются таблицей
 *          переходов QUAD_TABLE.
 */
wackos_encoder_event_t wackos_quad_update(wackos_quad_t *q, uint8_t clk, uint8_t dt)
{
    uint8_t state = (uint8_t)((clk << 1) | dt);
    uint8_t index = (uint8_t)((q->prev_state << 2) | state);
    q->prev_state = state;

    q->accum = (int8_t)(q->accum + QUAD_TABLE[index]);

    if (q->accum >= WACKOS_ENC_DETENT) {
        q->accum = 0;
        return WACKOS_ENC_CW;
    }
    if (q->accum <= -WACKOS_ENC_DETENT) {
        q->accum = 0;
        return WACKOS_ENC_CCW;
    }
    return WACKOS_ENC_NONE;
}

/**
 * @brief   Скормить дебаунсеру текущее состояние кнопки SW.
 * @param   b       Состояние кнопки.
 * @param   pressed true, если кнопка сейчас нажата.
 * @param   now_us  Текущее время в микросекундах (монотонное).
 * @return  Событие отдаётся в момент ОТПУСКАНИЯ: WACKOS_ENC_BTN для
 *          короткого нажатия, WACKOS_ENC_BTN_LONG для долгого, иначе
 *          WACKOS_ENC_NONE (в т.ч. всё время, пока кнопка ещё зажата).
 */
wackos_encoder_event_t wackos_btn_update(wackos_btn_t *b, bool pressed, int64_t now_us)
{
    if (pressed == b->pressed) {
        return WACKOS_ENC_NONE; /* состояние не менялось */
    }
    if (now_us - b->last_edge_us < WACKOS_ENC_DEBOUNCE_US) {
        return WACKOS_ENC_NONE; /* дребезг контактов */
    }
    b->last_edge_us = now_us;
    b->pressed = pressed;

    if (pressed) {
        b->press_us = now_us;
        return WACKOS_ENC_NONE; /* решение примем на отпускании */
    }

    return (now_us - b->press_us >= WACKOS_ENC_LONG_PRESS_US) ? WACKOS_ENC_BTN_LONG
                                                              : WACKOS_ENC_BTN;
}

/**
 * @brief   Имя события для протокола (payload кадра EVT ENC).
 * @param   ev  Событие энкодера.
 * @return  Строковый литерал ("CW"/"CCW"/"BTN"/"BTN_LONG"/"NONE").
 */
const char *wackos_encoder_event_name(wackos_encoder_event_t ev)
{
    switch (ev) {
        case WACKOS_ENC_CW:       return "CW";
        case WACKOS_ENC_CCW:      return "CCW";
        case WACKOS_ENC_BTN:      return "BTN";
        case WACKOS_ENC_BTN_LONG: return "BTN_LONG";
        default:                  return "NONE";
    }
}

/**
 * @brief   Обратный разбор имени события, полученного по протоколу.
 * @param   name  Строка вида "CW"/"CCW"/"BTN"/"BTN_LONG", либо NULL.
 * @return  Соответствующее значение wackos_encoder_event_t;
 *          WACKOS_ENC_NONE, если имя не распознано или name == NULL.
 */
wackos_encoder_event_t wackos_encoder_event_from_name(const char *name)
{
    if (!name) {
        return WACKOS_ENC_NONE;
    }
    if (strcmp(name, "CW") == 0)       return WACKOS_ENC_CW;
    if (strcmp(name, "CCW") == 0)      return WACKOS_ENC_CCW;
    if (strcmp(name, "BTN") == 0)      return WACKOS_ENC_BTN;
    if (strcmp(name, "BTN_LONG") == 0) return WACKOS_ENC_BTN_LONG;
    return WACKOS_ENC_NONE;
}
