/**
 * @file    uart_transport.h
 * @author  IAmWack
 * @brief   Реализация wackos_transport_t поверх UART для slave (ESP32).
 *
 * Единственный файл на стороне slave, который знает про UART.
 */
#pragma once

#include "wackos_transport.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Настроить UART и вернуть готовый transport для wackos_proto_init().
 * @return  Указатель на статический экземпляр wackos_transport_t.
 */
const wackos_transport_t *uart_transport_init(void);
