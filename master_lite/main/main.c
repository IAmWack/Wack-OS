/**
 * @file    main.c
 * @author  IAmWack
 * @brief   master_lite (ESP32-S3): точка входа автономной ветки без slave.
 *
 * Собирает слои вместе и запускает главный цикл — сам ничего "не
 * решает", только связывает хранилище и интерфейс. В отличие от
 * master/, здесь нет UART-транспорта и протокола к slave: это отдельный
 * автономный "мультитул" (см. CMakeLists.txt).
 */
#include "sdcard.h"
#include "clock.h"
#include "ble_hid_device.h"
#include "battery.h"
#include "mpu6050.h"
#include "ui.h"

#include "nvs_flash.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Точка входа ESP-IDF: собрать слои и запустить главный цикл.
 */
void app_main(void)
{
    /* Необязательно: если карты нет или она не отформатирована в FAT32,
     * sdcard_init() просто залогирует причину и вернёт false — на
     * остальную прошивку это не влияет (см. master_lite/main/sdcard.c). */
    sdcard_init();

    /* Нет RTC-микросхемы — время идёт от сборки прошивки, см. clock.h. */
    clock_init();

    /* Бонды BLE HID (см. ble_hid_device.h) хранятся в NVS — стандартный
     * паттерн ESP-IDF: если раздел испорчен/версия формата сменилась,
     * стереть и переинициализировать, а не падать на ESP_ERROR_CHECK. */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    ble_hid_device_init();
    battery_init();
    /* MPU6050 физически ещё не распаян — пока PIN_I2C_SCL/SDA == -1 (см.
     * board_pins.h), init() молча вернёт false, датчика нет и ладно. */
    mpu6050_init();

    ui_init();

    /* Весь цикл — это кадр интерфейса плюс разбор события ввода. */
    while (1) {
        ui_tick();
    }
}
