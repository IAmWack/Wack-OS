/**
 * @file    battery.c
 * @author  IAmWack
 * @brief   Чтение заряда LiPo-батареи (VBAT напрямую на ADC, без делителя).
 *
 * Если PIN_BATTERY_ADC когда-нибудь станет -1 (см. board_pins.h), весь
 * модуль тихо бездействует — тот же приём, что и у sound.c с PIN_BUZZER.
 */
#include "battery.h"
#include "board_pins.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "battery";

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/*
 * Без делителя: VBAT идёт на ADC-пин напрямую (батарея подобрана под эту
 * плату специально, напряжение в безопасных для ADC пределах). Если
 * когда-нибудь поставите делитель — коэффициент ослабления сюда.
 */
#define BATTERY_DIVIDER_RATIO  1

/* LiPo: ниже 3300 мВ считаем 0 %, выше 4200 мВ — 100 % (грубая линейная
 * оценка без учёта кривой разряда — для точного % нужен fuel-gauge IC). */
#define BATTERY_MV_EMPTY  3300
#define BATTERY_MV_FULL   4200

/*
 * На ESP32-S3 ADC1 — это GPIO1..10, канал = номер пина минус 1.
 * PIN_BATTERY_ADC вне этого диапазона (кроме -1 = "не подключено") —
 * ошибка в board_pins.h, не сюда.
 */
#define BATTERY_ADC_CHANNEL  (PIN_BATTERY_ADC - 1)

/* ============================================================================
 *  Static state
 * ============================================================================ */

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_cali_handle_t s_cali = NULL;
static bool s_cali_ok = false;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Настроить ADC под PIN_BATTERY_ADC (ничего не делает, если он -1). */
void battery_init(void)
{
    if (!battery_present()) {
        ESP_LOGI(TAG, "делитель не распаян (PIN_BATTERY_ADC == -1) — заряд не читаем");
        return;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, BATTERY_ADC_CHANNEL, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    s_cali_ok = (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) == ESP_OK);
    if (!s_cali_ok) {
        ESP_LOGW(TAG, "калибровка ADC недоступна на этом чипе, значения будут грубее");
    }

    ESP_LOGI(TAG, "ADC на GPIO%d готов (канал %d)", PIN_BATTERY_ADC, BATTERY_ADC_CHANNEL);
}

/** @brief  Подключён ли делитель — без него остальные функции смысла не имеют. */
bool battery_present(void)
{
    return PIN_BATTERY_ADC >= 0;
}

/** @brief  Напряжение батареи, милливольты (0, если battery_present() == false). */
uint32_t battery_voltage_mv(void)
{
    if (!battery_present() || !s_adc) {
        return 0;
    }

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc, BATTERY_ADC_CHANNEL, &raw));

    int pin_mv = 0;
    if (s_cali_ok) {
        adc_cali_raw_to_voltage(s_cali, raw, &pin_mv);
    } else {
        /* Без калибровки — грубая оценка по типичной крутизне 12 дБ attenuation. */
        pin_mv = raw * 3300 / 4095;
    }

    return (uint32_t)pin_mv * BATTERY_DIVIDER_RATIO;
}

/** @brief  Грубая оценка заряда, 0..100 % (линейно между 3300 и 4200 мВ). */
uint8_t battery_percent(void)
{
    uint32_t mv = battery_voltage_mv();
    if (mv <= BATTERY_MV_EMPTY) {
        return 0;
    }
    if (mv >= BATTERY_MV_FULL) {
        return 100;
    }
    return (uint8_t)((mv - BATTERY_MV_EMPTY) * 100 / (BATTERY_MV_FULL - BATTERY_MV_EMPTY));
}
