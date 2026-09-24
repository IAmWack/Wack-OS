/**
 * @file    freertos/task.h
 * @author  IAmWack
 * @brief   Заглушка freertos/task.h — только vTaskDelay.
 */
#pragma once

#include "freertos/FreeRTOS.h"

/**
 * @brief   Заснуть на заданное число тиков (в симуляторе 1 тик = 1 мс).
 * @param   ticks  Сколько тиков спать.
 */
void vTaskDelay(TickType_t ticks);
