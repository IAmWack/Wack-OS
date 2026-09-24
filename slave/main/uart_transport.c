#include "uart_transport.h"
#include "board_pins.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#define UART_RX_BUF_SIZE 256
#define UART_TX_BUF_SIZE 256

static int transport_write(const uint8_t *data, size_t len)
{
    return uart_write_bytes(UART_PORT_NUM, (const char *)data, len);
}

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
            break;
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
