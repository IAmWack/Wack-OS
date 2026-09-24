/**
 * @file    uart_transport.c
 * @author  IAmWack
 * @brief   Реализация wackos_transport_t поверх UART для master (ESP32-S3).
 *
 * Это единственный файл на стороне master, который знает про UART.
 * Слой протокола (components/wackos_protocol) про него не знает вообще.
 */
#include "uart_transport.h"
#include "board_pins.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define UART_RX_BUF_SIZE 256
#define UART_TX_BUF_SIZE 256

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/** @brief  Реализация wackos_transport_t::write поверх uart_write_bytes(). */
static int transport_write(const uint8_t *data, size_t len)
{
    return uart_write_bytes(UART_PORT_NUM, (const char *)data, len);
}

/**
 * @brief   Читает строку-кадр байт за байтом до '\n' либо до истечения тайм-аута.
 * @param   buf         Буфер для результата.
 * @param   max_len     Размер буфера buf.
 * @param   timeout_ms  Тайм-аут ожидания всей строки, миллисекунды.
 * @return  Число прочитанных байт (включая '\n', если он поместился).
 *
 * Кадры короткие (см. WACKOS_LINE_MAX), так что побайтовое чтение —
 * не узкое место.
 */
static int transport_read_line(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    size_t idx = 0;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while (idx + 1 < max_len) {
        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            break;
        }
        TickType_t remaining = deadline - now;

        uint8_t byte;
        int n = uart_read_bytes(UART_PORT_NUM, &byte, 1, remaining);
        if (n <= 0) {
            break; /* тайм-аут ожидания байта => тайм-аут всей строки */
        }
        buf[idx++] = byte;
        if (byte == '\n') {
            break;
        }
    }
    return (int)idx;
}

static const wackos_transport_t s_uart_transport = {
    .write = transport_write,
    .read_line = transport_read_line,
};

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Настроить UART по board_pins.h и вернуть транспорт для протокола.
 * @return  Указатель на статический wackos_transport_t для wackos_proto_init().
 */
const wackos_transport_t *uart_transport_init(void)
{
    const uart_config_t cfg = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, PIN_UART_TX, PIN_UART_RX,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    return &s_uart_transport;
}
