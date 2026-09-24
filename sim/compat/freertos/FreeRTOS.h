/**
 * @file    freertos/FreeRTOS.h
 * @author  IAmWack
 * @brief   Минимальная заглушка FreeRTOS.
 *
 * Из всего API прошивке здесь нужны только тики и vTaskDelay (см.
 * freertos/task.h).
 */
#pragma once

#include <stdint.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef uint32_t TickType_t;
typedef int      BaseType_t;

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define pdTRUE  1
#define pdFALSE 0

#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms)  ((TickType_t)(ms))

/* На ESP эти вещи приезжают из esp_attr.h/портлейера; в симуляторе
 * прерываний нет, поэтому атрибут пустой, а "запрет прерываний" —
 * пустышка (всё и так в одном потоке). */
#define IRAM_ATTR
#define portDISABLE_INTERRUPTS() ((void)0)
#define portENABLE_INTERRUPTS()  ((void)0)
#define portYIELD_FROM_ISR()     ((void)0)
