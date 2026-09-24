#include "wackos_encoder.h"

#include <string.h>

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

void wackos_quad_reset(wackos_quad_t *q, uint8_t clk, uint8_t dt)
{
    q->prev_state = (uint8_t)((clk << 1) | dt);
    q->accum = 0;
}

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
