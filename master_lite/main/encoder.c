/**
 * @file    encoder.c
 * @author  IAmWack
 * @brief   Драйвер энкодера KY-040 на стороне master (GPIO + ISR).
 *
 * Здесь только железо: настроить ноги, поймать прерывание, отдать
 * состояние ног общему декодеру (components/wackos_encoder) и сложить
 * его вердикт в очередь. Вся логика "что считать щелчком" — в
 * wackos_encoder, этот файл лишь мост между GPIO и ней.
 */
#include "encoder.h"
#include "board_pins.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"

/* ============================================================================
 *  Static state
 * ============================================================================ */

static QueueHandle_t s_event_queue = NULL;
static wackos_quad_t s_quad;
static wackos_btn_t  s_btn;

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/** @brief  Положить событие энкодера в очередь из контекста ISR. */
static void IRAM_ATTR post(wackos_encoder_event_t ev)
{
    if (ev == WACKOS_ENC_NONE) {
        return;
    }
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_event_queue, &ev, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

/** @brief  ISR на фронт CLK/DT: обновить квадратурный декодер. */
static void IRAM_ATTR encoder_quad_isr(void *arg)
{
    post(wackos_quad_update(&s_quad,
                            (uint8_t)gpio_get_level(PIN_ENC_CLK),
                            (uint8_t)gpio_get_level(PIN_ENC_DT)));
}

/** @brief  ISR на фронт SW: обновить состояние кнопки. */
static void IRAM_ATTR encoder_btn_isr(void *arg)
{
    /* Кнопка подтянута к питанию: 0 — нажата. */
    post(wackos_btn_update(&s_btn, gpio_get_level(PIN_ENC_SW) == 0,
                           esp_timer_get_time()));
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Настроить GPIO энкодера, повесить обработчики прерываний.
 */
void encoder_init(void)
{
    s_event_queue = xQueueCreate(16, sizeof(wackos_encoder_event_t));

    gpio_config_t quad_cfg = {
        .pin_bit_mask = (1ULL << PIN_ENC_CLK) | (1ULL << PIN_ENC_DT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&quad_cfg);

    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << PIN_ENC_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE, /* нужны оба фронта: короткое/длинное */
    };
    gpio_config(&btn_cfg);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_ENC_CLK, encoder_quad_isr, NULL);
    gpio_isr_handler_add(PIN_ENC_DT,  encoder_quad_isr, NULL);
    gpio_isr_handler_add(PIN_ENC_SW,  encoder_btn_isr, NULL);

    wackos_quad_reset(&s_quad, (uint8_t)gpio_get_level(PIN_ENC_CLK),
                      (uint8_t)gpio_get_level(PIN_ENC_DT));
    s_btn = (wackos_btn_t){ .pressed = false };
}

/**
 * @brief   Неблокирующе (с тайм-аутом) получить следующее событие энкодера.
 * @param   out_event    Куда положить событие.
 * @param   timeout_ms   Сколько ждать событие, миллисекунды.
 * @return  true, если событие было получено за timeout_ms.
 */
bool encoder_poll(wackos_encoder_event_t *out_event, uint32_t timeout_ms)
{
    return xQueueReceive(s_event_queue, out_event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
