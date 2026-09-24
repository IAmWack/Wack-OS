/**
 * @file    board_pins.h
 * @author  IAmWack
 * @brief   Распиновка master-платы (ESP32-S3).
 *
 * Единственное место, где эти номера упоминаются — остальной код
 * обращается к дисплею/энкодеру/UART только через их модули (display.h,
 * encoder.h, uart_transport.h).
 */
#pragma once

#include "driver/uart.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/* Экран ST7789, 172x320, SPI. Значения даны в задаче. */
#define PIN_LCD_MOSI  45
#define PIN_LCD_SCLK  40
#define PIN_LCD_CS    42
#define PIN_LCD_DC    41
#define PIN_LCD_RST   39
#define PIN_LCD_BL    48

/* Энкодер KY-040. Пины не были заданы явно — поставлены условные,
 * уточните под свою разводку. */
#define PIN_ENC_CLK   4
#define PIN_ENC_DT    5
#define PIN_ENC_SW    6

/* Пищалка. Её на макете ещё нет: -1 = не подключена, слой звука при
 * этом молчит (master/main/sound.c). Появится — поставить сюда номер
 * пина, больше ничего менять не нужно. */
#define PIN_BUZZER    -1

/* UART к slave. Свободные от экрана/энкодера пины ESP32-S3, тоже
 * уточните под свою разводку — важно лишь, чтобы TX master шёл на RX
 * slave и наоборот. */
#define PIN_UART_TX       17
#define PIN_UART_RX       18
#define UART_PORT_NUM     UART_NUM_1
#define UART_BAUD_RATE    115200
