/**
 * @file    sim_platform.c
 * @author  IAmWack
 * @brief   Реализация тех кусочков ESP-IDF, без которых прошивочный код
 *          не соберётся на ноутбуке: время, задержка, GPIO-заглушки,
 *          перезагрузка, кольцевые очереди FreeRTOS.
 *
 * Объявления этих же функций — в sim/compat/ (esp_timer.h, esp_system.h,
 * driver/gpio.h, freertos-заголовки), которые подставляются вместо
 * настоящих заголовков ESP-IDF при сборке под ноутбук (см. sim/Makefile).
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

/* ============================================================================
 *  Public API — время (esp_timer.h, sim.h)
 * ============================================================================ */

/** @brief Монотонное время в микросекундах (замена esp_timer_get_time() из ESP-IDF). @return Микросекунды с произвольной точки отсчёта. */
int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/** @brief Монотонное время в миллисекундах. @return Миллисекунды с произвольной точки отсчёта. */
uint32_t sim_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/** @brief Заснуть на ms миллисекунд (настоящий сон, не занятое ожидание). @param ms Длительность паузы. */
void sim_sleep_ms(uint32_t ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/**
 * @brief   Заглушка vTaskDelay() из FreeRTOS.
 * @param   ticks  Игнорируется.
 *
 * vTaskDelay в симуляторе — пустышка. Единственное место, где её зовёт
 * прошивка slave, — пауза перед перезагрузкой; ответ к этому моменту
 * уже лежит в проводе, так что реально спать незачем (и незачем
 * подвешивать окно).
 */
void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
}

/** @brief Заглушка esp_restart() из ESP-IDF — имитирует перезагрузку фальшивого slave. */
void esp_restart(void)
{
    sim_slave_reboot();
}

/* ============================================================================
 *  Public API — GPIO-заглушки (driver/gpio.h)
 * ============================================================================ */

/** @brief Заглушка gpio_install_isr_service(). @param flags Игнорируется. @return ESP_OK. */
esp_err_t gpio_install_isr_service(int flags)
{
    (void)flags;
    return ESP_OK;
}

/**
 * @brief   Заглушка gpio_isr_handler_add().
 * @param   pin      Игнорируется.
 * @param   handler  Игнорируется.
 * @param   arg      Игнорируется.
 * @return  ESP_OK.
 *
 * Прерываний в симуляторе нет: ноги никто не дёргает, обработчик
 * просто никогда не вызовется. Ввод приходит с клавиатуры или с
 * живой платы по проводу.
 */
esp_err_t gpio_isr_handler_add(int pin, gpio_isr_t handler, void *arg)
{
    (void)pin; (void)handler; (void)arg;
    return ESP_OK;
}

/** @brief Заглушка gpio_config(). @param cfg Игнорируется. @return ESP_OK. */
esp_err_t gpio_config(const gpio_config_t *cfg)
{
    (void)cfg;
    return ESP_OK;
}

/** @brief Заглушка gpio_set_level() — печатает уровень в терминал вместо ноги. @param pin Номер ноги. @param level 0/1. @return ESP_OK. */
esp_err_t gpio_set_level(int pin, uint32_t level)
{
    printf("[sim] GPIO%d = %u\n", pin, level);
    return ESP_OK;
}

/** @brief Заглушка gpio_get_level(). @param pin Игнорируется. @return Всегда 1. */
int gpio_get_level(int pin)
{
    (void)pin;
    return 1;
}

/* ============================================================================
 *  Types — очереди FreeRTOS: кольцевой буфер
 * ============================================================================ */

/** @brief Внутреннее представление QueueHandle_t — простой кольцевой буфер. */
struct sim_queue {
    size_t item_size;
    size_t length;
    size_t head, tail, count;
    unsigned char *data;
};

/* ============================================================================
 *  Public API — очереди FreeRTOS
 * ============================================================================ */

/**
 * @brief   Заглушка xQueueCreate() — кольцевой буфер в куче.
 * @param   length     Ёмкость очереди (число элементов).
 * @param   item_size  Размер одного элемента в байтах.
 * @return  Хендл очереди, либо NULL при нехватке памяти.
 */
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

/**
 * @brief   Заглушка xQueueSend() — положить элемент в очередь.
 * @param   q     Хендл очереди.
 * @param   item  Элемент для копирования (item_size байт).
 * @param   wait  Игнорируется (в симуляторе очередь не блокирует).
 * @return  pdTRUE при успехе, pdFALSE если очередь полна.
 */
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

/**
 * @brief   Заглушка xQueueSendFromISR() — в симуляторе то же, что xQueueSend().
 * @param   q      Хендл очереди.
 * @param   item   Элемент для копирования.
 * @param   woken  Если не NULL, всегда выставляется в pdFALSE (нет реальных ISR).
 * @return  pdTRUE при успехе, pdFALSE если очередь полна.
 */
BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *woken)
{
    if (woken) {
        *woken = pdFALSE;
    }
    return xQueueSend(q, item, 0);
}

/**
 * @brief   Заглушка xQueueReceive() — забрать элемент из очереди.
 * @param   q     Хендл очереди.
 * @param   out   Куда скопировать элемент.
 * @param   wait  Игнорируется (в симуляторе очередь не блокирует).
 * @return  pdTRUE при успехе, pdFALSE если очередь пуста.
 */
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
