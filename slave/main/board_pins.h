/**
 * @file    board_pins.h
 * @author  IAmWack
 * @brief   Распиновка slave-платы (эта реализация — под ESP32).
 *
 * Единственное место с номерами пинов на стороне slave. Слово "slave"
 * в протоколе не привязано к ESP32 — см. docs/ARCHITECTURE.md. Это
 * лишь одна из возможных реализаций.
 */
#pragma once

#include "driver/uart.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/* UART к master (реальная схема). Перекрёстно с master/main/board_pins.h:
 * TX этой платы -> RX master, RX этой платы <- TX master. */
#define PIN_UART_TX_REAL  17
#define PIN_UART_RX_REAL  16
#define UART_PORT_NUM_REAL UART_NUM_1

/* --- ВРЕМЕННО, ДЛЯ РУЧНОГО ТЕСТИРОВАНИЯ ---
 * Платы master ещё нет, второго USB-UART переходника для шины
 * GPIO16/17 тоже нет. Протокол временно поднят на UART0 — тех же
 * пинах, что и штатная консоль/прошивка (GPIO1 TX / GPIO3 RX), через
 * уже подключённый USB-кабель. Консоль ESP-IDF на UART0 при этом
 * выключена в sdkconfig (CONFIG_ESP_CONSOLE_NONE), иначе будет
 * конфликт драйверов UART. Когда появится master или второй
 * переходник — вернуть PIN_UART_TX/RX/UART_PORT_NUM на *_REAL значения
 * выше. */
#define PIN_UART_TX       1
#define PIN_UART_RX       3
#define UART_PORT_NUM     UART_NUM_0
#define UART_BAUD_RATE    115200

/*
 * Энкодер KY-040, временно подключённый к slave (платы master ещё нет,
 * а ручку хочется щупать вживую — см. modules/module_encoder.h).
 * Питание модуля — 3V3, НЕ 5V: подтяжки KY-040 идут к VCC, а ноги
 * ESP32 не пятивольтовые.
 *
 * DT сначала висел на GPIO35 — нога "только вход" и без внутренней
 * подтяжки; линию там держал бы только резистор на плате KY-040.
 * Перевешен на GPIO25, у которого подтяжка есть, — так линия не
 * болтается даже без резисторов на модуле.
 */
#define PIN_ENC_SW    32
#define PIN_ENC_CLK   33
#define PIN_ENC_DT    25

/* Тестовые светодиоды на макетке (не часть реальной распиновки slave). */
#define PIN_LED_YELLOW 5
#define PIN_LED_RED    18
#define PIN_LED_GREEN  19
