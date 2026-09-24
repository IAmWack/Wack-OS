/**
 * @file    sim_encoder.c
 * @author  IAmWack
 * @brief   Реализация encoder.h для ноутбука — подменяет KY-040 клавиатурой/мышью.
 *
 * Прошивка по-прежнему видит только "события энкодера" — откуда они
 * взялись (квадратурный ISR на KY-040 или клавиатура), слой ui.c не
 * знает и знать не должен. Источников два: локальная очередь от окна
 * (sim_input_push(), см. sim_window.c) и события EVT ENC от живой
 * платы slave, если симулятор подключён по USB (sim_serial.c).
 */
#include "sim.h"

#include <string.h>

#include "encoder.h"
#include "wackos_protocol.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define QUEUE_SIZE 32

/* ============================================================================
 *  Static state
 * ============================================================================ */

static wackos_encoder_event_t s_queue[QUEUE_SIZE];
static int s_head = 0;
static int s_tail = 0;

/* ============================================================================
 *  Public API — приём событий из окна и инициализация
 * ============================================================================ */

/**
 * @brief   Положить событие энкодера в локальную очередь (от окна).
 * @param   ev  Событие для encoder_poll().
 */
void sim_input_push(wackos_encoder_event_t ev)
{
    int next = (s_head + 1) % QUEUE_SIZE;
    if (next == s_tail) {
        return; /* очередь переполнена — теряем событие, как и железо */
    }
    s_queue[s_head] = ev;
    s_head = next;
}

/** @brief Инициализация энкодера (по интерфейсу encoder.h) — очистить очередь. */
void encoder_init(void)
{
    s_head = s_tail = 0;
}

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/**
 * @brief   Забрать одно событие из локальной очереди (клавиатура/мышь).
 * @param   out_event  Куда положить событие.
 * @return  false, если очередь пуста.
 */
static bool take_local(wackos_encoder_event_t *out_event)
{
    if (s_head == s_tail) {
        return false;
    }
    *out_event = s_queue[s_tail];
    s_tail = (s_tail + 1) % QUEUE_SIZE;
    return true;
}

/**
 * @brief   Забрать событие энкодера живой платы slave (по проводу).
 * @param   out_event  Куда положить событие.
 * @return  false, если событий нет.
 *
 * Ручка, подключённая к живой плате slave: её щелчки приезжают кадрами
 * EVT ENC (docs/PROTOCOL.md). Для прошивки это всё равно "события
 * энкодера" — подмена живёт только здесь, в слое железа симулятора.
 */
static bool take_remote(wackos_encoder_event_t *out_event)
{
    if (!sim_serial_active()) {
        return false;
    }
    wackos_frame_t evt;
    if (!wackos_proto_poll_event(&evt, 0)) {
        return false;
    }
    if (strcmp(evt.field, "ENC") != 0) {
        return false; /* какое-то другое событие — не наше дело */
    }
    wackos_encoder_event_t ev = wackos_encoder_event_from_name(evt.payload);
    if (ev == WACKOS_ENC_NONE) {
        return false;
    }

    /* Отрисованная ручка в окне повторяет настоящую. */
    if (ev == WACKOS_ENC_CW) {
        sim_window_note_turn(1);
    } else if (ev == WACKOS_ENC_CCW) {
        sim_window_note_turn(-1);
    } else {
        sim_window_note_press();
    }

    *out_event = ev;
    return true;
}

/* ============================================================================
 *  Public API — опрос энкодера
 * ============================================================================ */

/**
 * @brief   Опрос энкодера (по интерфейсу encoder.h) с тайм-аутом.
 * @param   out_event    Куда положить событие.
 * @param   timeout_ms   Максимальное время ожидания.
 * @return  false, если за timeout_ms ничего не пришло (или окно закрыли).
 */
bool encoder_poll(wackos_encoder_event_t *out_event, uint32_t timeout_ms)
{
    uint32_t deadline = sim_now_ms() + timeout_ms;
    for (;;) {
        if (take_local(out_event) || take_remote(out_event)) {
            return true;
        }
        if (sim_now_ms() >= deadline) {
            return false;
        }
        /* Пока ждём, крутим цикл окна: иначе X-события не разбирались
         * бы и окно выглядело бы зависшим. */
        sim_window_pump(5);
        if (!sim_window_alive()) {
            return false;
        }
    }
}
