/**
 * @file    boot_button.c
 * @author  IAmWack
 * @brief   Временная замена энкодера: жесты кнопкой BOOT (GPIO0).
 *
 * Пока на макете не распаян KY-040, крутилку изображает штатная кнопка
 * BOOT платы. Реализует ТОТ ЖЕ публичный интерфейс, что и encoder.c
 * (encoder_init/encoder_poll из encoder.h) — ui.c и main.c не знают и
 * не должны знать, что за кнопкой жмёт пользователь. Переключение
 * между этим файлом и encoder.c — одна строчка в CMakeLists.txt, по
 * которому из двух SRCS участвует в сборке (линковать оба нельзя —
 * конфликт символов).
 *
 * Раскладка жестов (см. board_pins.h, PIN_BOOT_BTN):
 *   - одно короткое нажатие  -> WACKOS_ENC_CW   (пролистать вниз)
 *   - два коротких подряд    -> WACKOS_ENC_CCW  (пролистать вверх)
 *   - долгое нажатие         -> WACKOS_ENC_BTN  (клик/выбор)
 *
 * WACKOS_ENC_BTN_LONG ("назад" по ui.c) этим вводом не достижим — с
 * одной кнопкой только три надёжных жеста (пробовали добавить ещё один
 * порог удержания под "назад", но удержание уже занято под клик, и
 * пользователю не разобрать один порог от другого на ощупь). "Назад"
 * вместо этого — последний пункт в конце каждого списка-экрана (см.
 * screen_menu.c, screen_settings.c): его решает выбор по списку, а не
 * ещё один жест кнопкой.
 */
#include "encoder.h"
#include "board_pins.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/* Длиннее этого удержание — долгое нажатие (клик). */
#define LONG_PRESS_MS     550
/* Сколько ждать второе короткое нажатие, прежде чем считать первое одиночным. */
#define DOUBLE_WINDOW_MS  350
/* Программный дебаунс дребезга контактов. */
#define DEBOUNCE_MS       30

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef struct {
    bool     pressed; /* true — фронт нажатия, false — отпускания */
    uint32_t t_ms;
} raw_edge_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

static QueueHandle_t s_event_queue = NULL; /* наружу, через encoder_poll() */
static QueueHandle_t s_raw_queue = NULL;   /* сырые фронты из ISR */

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/** @brief  ISR на любой фронт кнопки BOOT: дебаунс и сырое событие в очередь. */
static void IRAM_ATTR boot_btn_isr(void *arg)
{
    static uint32_t s_last_edge_ms = 0;
    uint32_t t = now_ms();
    if (t - s_last_edge_ms < DEBOUNCE_MS) {
        return;
    }
    s_last_edge_ms = t;

    /* Кнопка подтянута к питанию: 0 — нажата. */
    raw_edge_t e = { .pressed = (gpio_get_level(PIN_BOOT_BTN) == 0), .t_ms = t };
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_raw_queue, &e, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

/** @brief  Положить готовое событие в очередь, которую читает encoder_poll(). */
static void post(wackos_encoder_event_t ev)
{
    xQueueSend(s_event_queue, &ev, 0);
}

/**
 * @brief   Разбор жестов: одно/два коротких нажатия, долгое нажатие.
 *
 * Логика синхронизируется по сырым фронтам из ISR (нажатие/отпускание),
 * поэтому решения о "долго/коротко" и "один/два клика" принимаются тут
 * с ожиданием по тайм-ауту, а не в прерывании — там это делать нельзя
 * (esp_timer_start/stop не ISR-safe, а подряд идущиеxQueueReceive с
 * тайм-аутом в отдельной задаче решают то же самое проще и надёжнее).
 */
static void decode_task(void *arg)
{
    raw_edge_t e;
    for (;;) {
        /* Ждём нажатия (игнорируя случайные "отпускания", если вдруг
         * рассинхронизировались на старте). */
        if (xQueueReceive(s_raw_queue, &e, portMAX_DELAY) != pdTRUE || !e.pressed) {
            continue;
        }

        /* Отпустят в пределах LONG_PRESS_MS — короткое, иначе долгое. */
        raw_edge_t rel;
        if (xQueueReceive(s_raw_queue, &rel, pdMS_TO_TICKS(LONG_PRESS_MS)) != pdTRUE) {
            /* Тайм-аут — всё ещё держат: это долгое нажатие = клик. */
            post(WACKOS_ENC_BTN);
            /* Дождаться настоящего отпускания, чтобы не потерять синхронизацию. */
            xQueueReceive(s_raw_queue, &rel, portMAX_DELAY);
            continue;
        }
        if (rel.pressed) {
            /* Не должно происходить (фронты идут строго парами), но на
             * всякий случай — ресинхронизируемся со следующего прохода. */
            continue;
        }

        /* Короткое нажатие отпущено — ждём, не последует ли второе. */
        raw_edge_t second;
        if (xQueueReceive(s_raw_queue, &second, pdMS_TO_TICKS(DOUBLE_WINDOW_MS)) == pdTRUE &&
            second.pressed) {
            post(WACKOS_ENC_CCW); /* двойной клик — вверх */
            /* Съесть отпускание второго нажатия, не разбирая его длину. */
            xQueueReceive(s_raw_queue, &rel, pdMS_TO_TICKS(LONG_PRESS_MS + 200));
        } else {
            post(WACKOS_ENC_CW); /* одиночный клик — вниз */
        }
    }
}

/* ============================================================================
 *  Public API (encoder.h)
 * ============================================================================ */

/** @brief  Настроить GPIO кнопки BOOT, запустить разбор жестов. */
void encoder_init(void)
{
    s_event_queue = xQueueCreate(16, sizeof(wackos_encoder_event_t));
    s_raw_queue = xQueueCreate(8, sizeof(raw_edge_t));

    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << PIN_BOOT_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&btn_cfg);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BOOT_BTN, boot_btn_isr, NULL);

    xTaskCreate(decode_task, "boot_btn", 2560, NULL, 5, NULL);
}

/**
 * @brief   Неблокирующе (с тайм-аутом) получить следующее событие ввода.
 * @param   out_event   Куда положить событие.
 * @param   timeout_ms  Сколько ждать событие, миллисекунды.
 * @return  true, если событие было получено за timeout_ms.
 */
bool encoder_poll(wackos_encoder_event_t *out_event, uint32_t timeout_ms)
{
    return xQueueReceive(s_event_queue, out_event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
