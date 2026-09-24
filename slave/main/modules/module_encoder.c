/**
 * @file    module_encoder.c
 * @author  IAmWack
 * @brief   Модуль энкодера KY-040, временно подключённого к slave.
 *
 * Опрашивает GPIO энкодера через прерывания, декодирует события общим
 * декодером components/wackos_encoder и рассылает их кадрами EVT.
 * Namespace команд: ENC_*, включая набор временных отладочных команд
 * (ENC_HUNT/ENC_SCAN/ENC_DIAG) для поиска и диагностики проводки —
 * см. board_pins.h. Подробнее про временное расположение ручки на
 * slave — см. module_encoder.h.
 */
#include "module_encoder.h"
#include "board_pins.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"

#include "wackos_encoder.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define NAMESPACE_PREFIX "ENC_"
#define QUEUE_LEN 32

/* ============================================================================
 *  Static state
 * ============================================================================ */

static QueueHandle_t s_queue = NULL;
static wackos_quad_t s_quad;
static wackos_btn_t  s_btn;

/* Счётчики для ENC_GET — опросный запасной путь, если EVT потерялись. */
static volatile int16_t s_delta = 0;
static volatile uint8_t s_clicks = 0;
static volatile uint8_t s_longs = 0;

/* Счётчики срабатываний прерываний — только для ENC_DIAG: по ним видно,
 * доходит ли вообще сигнал до ног, или проблема выше. */
static volatile uint32_t s_isr_quad = 0;
static volatile uint32_t s_isr_btn = 0;

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/**
 * @brief   Положить декодированное событие в очередь и обновить счётчики.
 * @param   ev  Событие энкодера (WACKOS_ENC_NONE игнорируется).
 */
static void IRAM_ATTR post(wackos_encoder_event_t ev)
{
    if (ev == WACKOS_ENC_NONE) {
        return;
    }
    switch (ev) {
        case WACKOS_ENC_CW:       s_delta++;  break;
        case WACKOS_ENC_CCW:      s_delta--;  break;
        case WACKOS_ENC_BTN:      s_clicks++; break;
        case WACKOS_ENC_BTN_LONG: s_longs++;  break;
        default: break;
    }

    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_queue, &ev, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief   ISR фазовых ног CLK/DT — декодирует шаг вращения и постит событие.
 * @param   arg  Не используется (сигнатура gpio_isr_handler_add()).
 */
static void IRAM_ATTR quad_isr(void *arg)
{
    s_isr_quad++;
    post(wackos_quad_update(&s_quad,
                            (uint8_t)gpio_get_level(PIN_ENC_CLK),
                            (uint8_t)gpio_get_level(PIN_ENC_DT)));
}

/**
 * @brief   ISR ноги кнопки SW — декодирует клик/долгое нажатие и постит событие.
 * @param   arg  Не используется (сигнатура gpio_isr_handler_add()).
 */
static void IRAM_ATTR btn_isr(void *arg)
{
    s_isr_btn++;
    /* Кнопка подтянута к питанию: 0 — нажата. */
    post(wackos_btn_update(&s_btn, gpio_get_level(PIN_ENC_SW) == 0,
                           esp_timer_get_time()));
}

/**
 * @brief   Настроить GPIO CLK/DT/SW энкодера как вход с подтяжкой, прерывание по обоим фронтам.
 *
 * Все три ноги — вход с внутренней подтяжкой: KY-040 замыкает их на
 * землю, так что подтяжка обязательна, иначе линии болтаются.
 */
static void configure_pins(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_ENC_CLK) | (1ULL << PIN_ENC_DT) |
                        (1ULL << PIN_ENC_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE, /* оба фронта: и щелчки, и длинное нажатие */
    };
    gpio_config(&cfg);
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Настроить GPIO энкодера и обработчики прерываний.
 */
void module_encoder_init(void)
{
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(wackos_encoder_event_t));

    configure_pins();

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_ENC_CLK, quad_isr, NULL);
    gpio_isr_handler_add(PIN_ENC_DT,  quad_isr, NULL);
    gpio_isr_handler_add(PIN_ENC_SW,  btn_isr,  NULL);

    wackos_quad_reset(&s_quad, (uint8_t)gpio_get_level(PIN_ENC_CLK),
                      (uint8_t)gpio_get_level(PIN_ENC_DT));
    s_btn = (wackos_btn_t){ .pressed = false };
}

/**
 * @brief   Разослать накопленные события кадрами EVT.
 *
 * Зовётся из главного цикла, чтобы отправка не лезла в середину
 * чужого кадра.
 */
void module_encoder_pump(void)
{
    wackos_encoder_event_t ev;
    while (s_queue && xQueueReceive(s_queue, &ev, 0) == pdTRUE) {
        wackos_proto_send_event(WACKOS_ADDR_SLAVE1, "ENC",
                                wackos_encoder_event_name(ev));
    }
}

/**
 * @brief   Проверить, относится ли команда к namespace ENC_.
 * @param   cmd  Имя команды из кадра REQ.
 * @return  true, если cmd начинается с "ENC_".
 */
bool module_encoder_owns(const char *cmd)
{
    return strncmp(cmd, NAMESPACE_PREFIX, strlen(NAMESPACE_PREFIX)) == 0;
}

/**
 * @brief   Обработать команду namespace ENC_ (ENC_GET, ENC_HUNT, ENC_SCAN, ENC_DIAG).
 * @param   request  Входящий запрос.
 */
void module_encoder_handle(const wackos_frame_t *request)
{
    /*
     * ENC_GET — опросный путь на случай, если master пропустил EVT
     * (например, был занят своим запросом). Отдаёт и обнуляет всё,
     * что накопилось: поворотов со знаком, коротких нажатий, длинных.
     */
    if (strcmp(request->field, "ENC_GET") == 0) {
        portDISABLE_INTERRUPTS();
        int16_t delta = s_delta;
        uint8_t clicks = s_clicks;
        uint8_t longs = s_longs;
        s_delta = 0;
        s_clicks = 0;
        s_longs = 0;
        portENABLE_INTERRUPTS();

        char payload[32];
        snprintf(payload, sizeof(payload), "%d,%u,%u", delta, clicks, longs);
        wackos_proto_send_ok(request, payload);
        return;
    }

    /*
     * ENC_HUNT — поиск ноги: включает подтяжку на всех свободных
     * выводах и 400 мс смотрит, какие из них притягиваются к земле.
     * Нужен, когда сигнал не доходит и непонятно, куда воткнут провод:
     * вместо гадания по шелкографии сразу видно номер GPIO. Тоже
     * временная отладочная команда.
     */
    if (strcmp(request->field, "ENC_HUNT") == 0) {
        /* Без UART0 (1,3), флеша (6..11) и ног светодиодов (5,18,19).
         * 34..39 не берём: у них нет внутренней подтяжки, они бы
         * "залипали" в нуле и врали. */
        static const int PINS[] = { 2, 4, 12, 13, 14, 15, 16, 17,
                                    21, 22, 23, 25, 26, 27, 32, 33 };
        const int n_pins = sizeof(PINS) / sizeof(PINS[0]);

        /*
         * payload "DOWN" переворачивает опыт: подтяжка вниз и ищем
         * ноги, которые кто-то тянет ВВЕРХ. Так проверяется версия
         * "перепутаны GND и +": при такой ошибке энкодер замыкает
         * линии на питание, и подтяжкой вверх этого не увидеть.
         */
        bool look_high = (strcmp(request->payload, "DOWN") == 0);

        for (int i = 0; i < n_pins; i++) {
            gpio_config_t c = {
                .pin_bit_mask = 1ULL << PINS[i],
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = look_high ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE,
                .pull_down_en = look_high ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&c);
        }

        uint32_t low[sizeof(PINS) / sizeof(PINS[0])] = {0};
        uint32_t total = 0;
        int64_t until = esp_timer_get_time() + 400000;
        while (esp_timer_get_time() < until) {
            for (int i = 0; i < n_pins; i++) {
                low[i] += (gpio_get_level(PINS[i]) == (look_high ? 1 : 0));
            }
            total++;
        }

        char payload[WACKOS_PAYLOAD_MAX];
        int len = snprintf(payload, sizeof(payload), look_high ? "high:" : "low:");
        bool any = false;
        for (int i = 0; i < n_pins; i++) {
            /* Считаем ногу "притянутой", если она в нуле больше
             * четверти времени — иначе поймаем наводку. */
            if (low[i] * 4 > total) {
                len += snprintf(payload + len, sizeof(payload) - len, "%s%d",
                                any ? "," : "", PINS[i]);
                any = true;
            }
        }
        if (!any) {
            snprintf(payload + len, sizeof(payload) - len, "нет");
        }

        configure_pins(); /* вернуть ноги энкодера в рабочий режим */
        wackos_proto_send_ok(request, payload);
        return;
    }

    /*
     * ENC_SCAN — плотный опрос ног 200 мс: сколько раз каждая линия
     * была притянута к земле. Отвечает на самый нижний вопрос — доходит
     * ли до ноги хоть что-то, безотносительно прерываний и декодера.
     * Тоже отладочная и тоже временная.
     */
    if (strcmp(request->field, "ENC_SCAN") == 0) {
        uint32_t low_clk = 0, low_dt = 0, low_sw = 0, total = 0;
        int64_t until = esp_timer_get_time() + 200000;
        while (esp_timer_get_time() < until) {
            low_clk += (gpio_get_level(PIN_ENC_CLK) == 0);
            low_dt  += (gpio_get_level(PIN_ENC_DT) == 0);
            low_sw  += (gpio_get_level(PIN_ENC_SW) == 0);
            total++;
        }
        char payload[64];
        snprintf(payload, sizeof(payload), "clk_low=%u,dt_low=%u,sw_low=%u,n=%u",
                 (unsigned)low_clk, (unsigned)low_dt, (unsigned)low_sw,
                 (unsigned)total);
        wackos_proto_send_ok(request, payload);
        return;
    }

    /*
     * ENC_DIAG — временная отладочная команда (как SYS_LED, в
     * docs/PROTOCOL.md её нет). Отдаёт сырые уровни ног и счётчики
     * прерываний: по ним сразу видно, на каком слое рвётся — на
     * проводах, на прерываниях или уже в декодере.
     */
    if (strcmp(request->field, "ENC_DIAG") == 0) {
        char payload[64];
        snprintf(payload, sizeof(payload),
                 "clk=%d,dt=%d,sw=%d,irq_q=%u,irq_b=%u,acc=%d",
                 gpio_get_level(PIN_ENC_CLK), gpio_get_level(PIN_ENC_DT),
                 gpio_get_level(PIN_ENC_SW), (unsigned)s_isr_quad,
                 (unsigned)s_isr_btn, (int)s_quad.accum);
        wackos_proto_send_ok(request, payload);
        return;
    }

    wackos_proto_send_err(request, WACKOS_ERR_NOT_SUPPORTED);
}
