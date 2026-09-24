/**
 * @file    main.c
 * @author  IAmWack
 * @brief   Slave (ESP32): точка входа.
 *
 * Принимает запросы по UART и передаёт их диспетчеру. Сам ничего про
 * команды не знает — это дело dispatcher.c и модулей в modules/.
 */
#include "wackos_protocol.h"
#include "uart_transport.h"
#include "dispatcher.h"
#include "modules/module_encoder.h"
#include "board_pins.h"
#include "driver/gpio.h"

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/**
 * @brief   Настроить и погасить тестовые светодиоды макетки.
 */
static void leds_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_LED_YELLOW) | (1ULL << PIN_LED_RED) | (1ULL << PIN_LED_GREEN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(PIN_LED_YELLOW, 0);
    gpio_set_level(PIN_LED_RED, 0);
    gpio_set_level(PIN_LED_GREEN, 0);
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Точка входа ESP-IDF: инициализация слоёв и главный цикл slave.
 */
void app_main(void)
{
    leds_init();
    module_encoder_init();

    const wackos_transport_t *transport = uart_transport_init();
    wackos_proto_init(transport);
    /* На чужие адреса не отвечаем — иначе на шине из нескольких slave
     * все ответят на один запрос. */
    wackos_proto_set_address(WACKOS_ADDR_SLAVE1);

    while (1) {
        /*
         * Сначала выпускаем накопленные события энкодера, потом слушаем
         * запросы. Всё в одном потоке специально: так кадр EVT не может
         * влезть в середину кадра RES.
         *
         * Тайм-аут приёма короткий (20 мс) — иначе события ручки ждали
         * бы своей очереди до секунды, и крутить было бы "резиново".
         */
        module_encoder_pump();

        wackos_frame_t request;
        if (wackos_proto_receive_request(&request, 20)) {
            dispatcher_handle(&request);
        }
    }
}
