/**
 * @file    wackos_encoder.h
 * @author  IAmWack
 * @brief   Декодер энкодера KY-040 (квадратура + кнопка) без единой строчки про железо.
 *
 * Зачем отдельным компонентом: энкодер по задумке висит на master, но
 * пока платы master нет, он подключён к slave и шлёт события по
 * протоколу. Логика "четыре перехода = один щелчок" и "держал дольше
 * 0.6 с = это «назад»" должна быть при этом ОДНА, иначе на железе
 * ручка будет вести себя не так, как в симуляторе.
 *
 * Здесь только чистые функции над состоянием: кто читает ноги (ISR на
 * ESP32, опрос, что угодно) — дело вызывающего.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    WACKOS_ENC_NONE = 0,
    WACKOS_ENC_CW,       /* поворот по часовой */
    WACKOS_ENC_CCW,      /* против часовой */
    WACKOS_ENC_BTN,      /* короткое нажатие SW */
    WACKOS_ENC_BTN_LONG, /* долгое нажатие — "назад" по всему интерфейсу */
} wackos_encoder_event_t;

/* Дольше этого удержание считается длинным нажатием. */
#define WACKOS_ENC_LONG_PRESS_US 600000
/* Программный дебаунс кнопки. */
#define WACKOS_ENC_DEBOUNCE_US    30000
/* Сколько переходов состояния приходится на один щелчок KY-040. */
#define WACKOS_ENC_DETENT         4

typedef struct {
    uint8_t prev_state; /* (CLK<<1 | DT) на прошлом шаге */
    int8_t  accum;      /* накопленные переходы до целого щелчка */
} wackos_quad_t;

typedef struct {
    int64_t last_edge_us; /* для дебаунса */
    int64_t press_us;     /* когда кнопку нажали */
    bool    pressed;
} wackos_btn_t;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Запомнить текущее положение ног, ничего не выдавая наружу. */
void wackos_quad_reset(wackos_quad_t *q, uint8_t clk, uint8_t dt);

/**
 * @brief   Скормить новое состояние ног квадратурному декодеру.
 * @return  CW/CCW на границе щелчка, иначе NONE. Дребезг и недо-переходы
 *          отсекаются таблицей переходов.
 */
wackos_encoder_event_t wackos_quad_update(wackos_quad_t *q, uint8_t clk, uint8_t dt);

/**
 * @brief   Скормить состояние кнопки (true = нажата).
 * @return  Событие отдаётся в момент ОТПУСКАНИЯ: только тогда известно,
 *          коротким было нажатие или длинным.
 */
wackos_encoder_event_t wackos_btn_update(wackos_btn_t *b, bool pressed, int64_t now_us);

/* Имена событий для протокола (payload кадра EVT ENC) и обратный разбор. */
const char            *wackos_encoder_event_name(wackos_encoder_event_t ev);
wackos_encoder_event_t wackos_encoder_event_from_name(const char *name);
