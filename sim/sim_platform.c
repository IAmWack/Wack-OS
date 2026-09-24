/*
 * Реализация тех кусочков ESP-IDF, без которых прошивочный код не
 * соберётся на ноутбуке: время, задержка, GPIO-заглушки, перезагрузка.
 */
#include "sim.h"

#include <stdio.h>
#include <time.h>

#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"

#include <stdlib.h>
#include <string.h>

int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

uint32_t sim_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void sim_sleep_ms(uint32_t ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/*
 * vTaskDelay в симуляторе — пустышка. Единственное место, где её зовёт
 * прошивка slave, — пауза перед перезагрузкой; ответ к этому моменту
 * уже лежит в проводе, так что реально спать незачем (и незачем
 * подвешивать окно).
 */
void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
}

void esp_restart(void)
{
    sim_slave_reboot();
}

esp_err_t gpio_install_isr_service(int flags)
{
    (void)flags;
    return ESP_OK;
}

esp_err_t gpio_isr_handler_add(int pin, gpio_isr_t handler, void *arg)
{
    /* Прерываний в симуляторе нет: ноги никто не дёргает, обработчик
     * просто никогда не вызовется. Ввод приходит с клавиатуры или с
     * живой платы по проводу. */
    (void)pin; (void)handler; (void)arg;
    return ESP_OK;
}

esp_err_t gpio_config(const gpio_config_t *cfg)
{
    (void)cfg;
    return ESP_OK;
}

esp_err_t gpio_set_level(int pin, uint32_t level)
{
    printf("[sim] GPIO%d = %u\n", pin, level);
    return ESP_OK;
}

int gpio_get_level(int pin)
{
    (void)pin;
    return 1;
}

/* ---- очереди FreeRTOS: кольцевой буфер ---- */

struct sim_queue {
    size_t item_size;
    size_t length;
    size_t head, tail, count;
    unsigned char *data;
};

QueueHandle_t xQueueCreate(size_t length, size_t item_size)
{
    struct sim_queue *q = calloc(1, sizeof(*q));
    if (!q) {
        return NULL;
    }
    q->item_size = item_size;
    q->length = length;
    q->data = calloc(length, item_size);
    if (!q->data) {
        free(q);
        return NULL;
    }
    return q;
}

BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait)
{
    (void)wait;
    if (!q || q->count == q->length) {
        return pdFALSE;
    }
    memcpy(q->data + q->head * q->item_size, item, q->item_size);
    q->head = (q->head + 1) % q->length;
    q->count++;
    return pdTRUE;
}

BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *woken)
{
    if (woken) {
        *woken = pdFALSE;
    }
    return xQueueSend(q, item, 0);
}

BaseType_t xQueueReceive(QueueHandle_t q, void *out, TickType_t wait)
{
    (void)wait;
    if (!q || q->count == 0) {
        return pdFALSE;
    }
    memcpy(out, q->data + q->tail * q->item_size, q->item_size);
    q->tail = (q->tail + 1) % q->length;
    q->count--;
    return pdTRUE;
}
