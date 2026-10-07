/**
 * @file    power.c
 * @author  IAmWack
 * @brief   Программное "выключение" платки — глубокий сон.
 */
#include "power.h"
#include "board_pins.h"
#include "display.h"

#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_log.h"

static const char *TAG = "power";

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Погасить подсветку и уйти в глубокий сон до нажатия BOOT. */
void power_sleep(void)
{
    ESP_LOGI(TAG, "уходим в сон, разбудит кнопка BOOT");
    display_set_brightness(0);

    /* RTC-подтяжка нужна явно: обычный gpio_config() из boot_button.c не
     * гарантированно сохраняется в глубоком сне — пин там переходит под
     * RTC-домен, а не обычный GPIO-домен. */
    rtc_gpio_pullup_en((gpio_num_t)PIN_BOOT_BTN);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_BOOT_BTN);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BTN, 0);

    esp_deep_sleep_start();
}
