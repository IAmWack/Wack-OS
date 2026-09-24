/*
 * Master (ESP32-S3): точка входа. Собирает слои вместе и запускает
 * главный цикл — сам ничего "не решает", только связывает транспорт,
 * протокол и интерфейс.
 */
#include "wackos_protocol.h"
#include "uart_transport.h"
#include "ui.h"

void app_main(void)
{
    const wackos_transport_t *transport = uart_transport_init();
    wackos_proto_init(transport);

    ui_init();

    /*
     * Весь цикл — это кадр интерфейса плюс разбор события энкодера.
     * Логика (запросы к slave) вызывается изнутри экранов, поэтому
     * здесь больше ничего и не нужно.
     */
    while (1) {
        ui_tick();
    }
}
