/**
 * @file    uart_transport.h
 * @author  IAmWack
 * @brief   Реализация wackos_transport_t поверх UART для slave (ESP32).
 *
 * Единственный файл на стороне slave, который знает про UART.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "wackos_transport.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Настроить UART и вернуть готовый transport для wackos_proto_init().
 * @return  Указатель на статический экземпляр wackos_transport_t.
 */
const wackos_transport_t *uart_transport_init(void);

/**
 * @brief   Самотест второй, "реальной" (master) UART-линии — временная команда.
 *
 * Платы master ещё нет, поэтому GPIO16/17 (UART_NUM_1, см.
 * PIN_UART_TX_REAL/PIN_UART_RX_REAL в board_pins.h) никем не заняты.
 * Функция шлёт тестовый паттерн на TX_REAL и ждёт его обратно на
 * RX_REAL — работает, только если пользователь физически замкнул
 * TX_REAL и RX_REAL перемычкой на макетке (самозамыкание, не путать с
 * протокольным UART0). Первый вызов лениво поднимает драйвер
 * UART_NUM_1, дальше он остаётся установленным.
 *
 * @param   out      Буфер для человекочитаемого результата ("OK ..." /
 *                    "FAIL ...").
 * @param   out_len  Размер out.
 * @return  true, если паттерн пришёл обратно байт-в-байт.
 */
bool uart_transport_uart1_selftest(char *out, size_t out_len);
