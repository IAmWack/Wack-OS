/*
 * Заглушка очередей FreeRTOS: кольцевой буфер фиксированного размера.
 * Нужна, чтобы код прошивки, который общается с ISR через очередь,
 * собирался на ноутбуке без изменений.
 */
#pragma once

#include <stddef.h>

#include "freertos/FreeRTOS.h"

typedef struct sim_queue *QueueHandle_t;

QueueHandle_t xQueueCreate(size_t length, size_t item_size);
BaseType_t    xQueueSend(QueueHandle_t q, const void *item, TickType_t wait);
BaseType_t    xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *woken);
BaseType_t    xQueueReceive(QueueHandle_t q, void *out, TickType_t wait);
