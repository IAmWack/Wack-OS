/*
 * Отдельный минимальный проект: не часть Wack-OS, просто проверка, что
 * плата + Espressif-IDE + прошивка/монитор вообще работают, прежде чем
 * собирать master/ с экраном/энкодером/протоколом.
 *
 * D5 на большинстве плат ESP32/ESP32-S3 DevKit подписан как GPIO5.
 * Если на вашей плате шёлкография другая — поправьте BLINK_GPIO.
 */
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BLINK_GPIO 5
#define BLINK_PERIOD_MS 500

static const char *TAG = "blink_test";

void app_main(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BLINK_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);

    bool level = false;
    while (1) {
        level = !level;
        gpio_set_level(BLINK_GPIO, level);
        ESP_LOGI(TAG, "led = %d", level);
        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_MS));
    }
}
