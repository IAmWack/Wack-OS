/**
 * @file    encoder.h
 * @author  IAmWack
 * @brief   Публичный интерфейс драйвера энкодера KY-040 на master.
 *
 * Отдаёт наружу только события — ui.c не знает ни про GPIO, ни про
 * ISR, ни про квадратурный декодер. Сам декодер живёт в общем
 * компоненте wackos_encoder: та же логика щелчков и длинного нажатия
 * используется на slave, пока ручка подключена к нему (см.
 * docs/PROTOCOL.md, событие ENC).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "wackos_encoder.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Настроить GPIO энкодера, повесить обработчики прерываний. */
void encoder_init(void);

/**
 * @brief   Неблокирующе (с тайм-аутом) получить следующее событие энкодера.
 * @param   out_event   Куда положить событие.
 * @param   timeout_ms  Сколько ждать событие, миллисекунды.
 * @return  true, если событие было получено за timeout_ms.
 */
bool encoder_poll(wackos_encoder_event_t *out_event, uint32_t timeout_ms);
