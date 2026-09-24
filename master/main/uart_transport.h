/**
 * @file    uart_transport.h
 * @author  IAmWack
 * @brief   Публичный интерфейс UART-транспорта master.
 *
 * Это единственный файл на стороне master, который знает про UART.
 * Слой протокола (components/wackos_protocol) про него не знает вообще.
 */
#pragma once

#include "wackos_transport.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Настроить UART по board_pins.h и вернуть транспорт для протокола.
 * @return  Указатель на статический wackos_transport_t для wackos_proto_init().
 */
const wackos_transport_t *uart_transport_init(void);
