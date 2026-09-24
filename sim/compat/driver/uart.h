/*
 * Заглушка driver/uart.h — нужна только потому, что board_pins.h
 * (и master, и slave) подтягивает номера портов UART_NUM_*.
 * Настоящего UART в симуляторе нет: master и slave разговаривают
 * через петлю в памяти (sim_transport.c).
 */
#pragma once

typedef int uart_port_t;

#define UART_NUM_0 0
#define UART_NUM_1 1
#define UART_NUM_2 2
