/*
 * Реализация wackos_transport_t поверх UART для master (ESP32-S3).
 *
 * Это единственный файл на стороне master, который знает про UART.
 * Слой протокола (components/wackos_protocol) про него не знает вообще.
 */
#pragma once

#include "wackos_transport.h"

/* Настраивает UART по board_pins.h и возвращает транспорт для
 * wackos_proto_init(). */
const wackos_transport_t *uart_transport_init(void);
