/*
 * Реализация wackos_transport_t поверх UART для slave (ESP32).
 * Единственный файл на стороне slave, который знает про UART.
 */
#pragma once

#include "wackos_transport.h"

const wackos_transport_t *uart_transport_init(void);
