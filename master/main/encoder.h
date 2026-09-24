/*
 * Энкодер KY-040 на стороне master. Отдаёт наружу только события —
 * ui.c не знает ни про GPIO, ни про ISR, ни про квадратурный декодер.
 *
 * Сам декодер живёт в общем компоненте wackos_encoder: та же логика
 * щелчков и длинного нажатия используется на slave, пока ручка
 * подключена к нему (см. docs/PROTOCOL.md, событие ENC).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "wackos_encoder.h"

void encoder_init(void);

/* Неблокирующе (с тайм-аутом) получить следующее событие энкодера.
 * Возвращает true, если событие было получено за timeout_ms. */
bool encoder_poll(wackos_encoder_event_t *out_event, uint32_t timeout_ms);
