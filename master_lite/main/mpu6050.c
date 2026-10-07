/**
 * @file    mpu6050.c
 * @author  IAmWack
 * @brief   Акселерометр/гироскоп MPU6050 по I2C.
 *
 * Если PIN_I2C_SCL/PIN_I2C_SDA == -1 (см. board_pins.h), весь модуль
 * тихо бездействует — тот же приём, что и у sound.c с PIN_BUZZER.
 */
#include "mpu6050.h"
#include "board_pins.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "mpu6050";

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define MPU6050_ADDR        0x68 /* AD0 на GND — заводской адрес по умолчанию */
#define REG_PWR_MGMT_1      0x6B
#define REG_WHO_AM_I        0x75
#define REG_ACCEL_XOUT_H    0x3B /* 14 байт подряд: accel(6) + temp(2) + gyro(6) */
#define WHO_AM_I_EXPECTED   0x68

#define ACCEL_LSB_PER_G     16384.0f /* диапазон по умолчанию после сброса — ±2g */
#define GYRO_LSB_PER_DPS    131.0f   /* диапазон по умолчанию после сброса — ±250 °/с */

/* ============================================================================
 *  Static state
 * ============================================================================ */

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static bool s_present = false;

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

static inline bool wired(void)
{
    return PIN_I2C_SCL >= 0 && PIN_I2C_SDA >= 0;
}

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 50);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, len, 50);
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Поднять I2C и найти MPU6050 по WHO_AM_I. */
bool mpu6050_init(void)
{
    if (!wired()) {
        ESP_LOGI(TAG, "I2C не распаян (PIN_I2C_SCL/SDA == -1) — MPU6050 не ищем");
        return false;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) {
        ESP_LOGW(TAG, "не удалось поднять I2C на GPIO%d/%d", PIN_I2C_SCL, PIN_I2C_SDA);
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDR,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev) != ESP_OK) {
        ESP_LOGW(TAG, "не удалось добавить устройство 0x%02X на шину", MPU6050_ADDR);
        return false;
    }

    uint8_t who = 0;
    if (reg_read(REG_WHO_AM_I, &who, 1) != ESP_OK || who != WHO_AM_I_EXPECTED) {
        ESP_LOGW(TAG, "датчик не отозвался (WHO_AM_I=0x%02X, ждали 0x%02X) — не распаян?", who, WHO_AM_I_EXPECTED);
        return false;
    }

    /* После включения MPU6050 спит (sleep-бит в PWR_MGMT_1) — будим. */
    reg_write(REG_PWR_MGMT_1, 0x00);

    s_present = true;
    ESP_LOGI(TAG, "MPU6050 найден и готов (GPIO%d=SCL, GPIO%d=SDA)", PIN_I2C_SCL, PIN_I2C_SDA);
    return true;
}

/** @brief  Нашёлся ли датчик при mpu6050_init(). */
bool mpu6050_present(void)
{
    return s_present;
}

/** @brief  Прочитать свежий замер акселерометра и гироскопа. */
bool mpu6050_read(mpu6050_sample_t *out)
{
    if (!s_present || !out) {
        return false;
    }

    uint8_t raw[14];
    if (reg_read(REG_ACCEL_XOUT_H, raw, sizeof(raw)) != ESP_OK) {
        return false;
    }

    int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t az = (int16_t)((raw[4] << 8) | raw[5]);
    /* raw[6..7] — температура, сейчас не читаем. */
    int16_t gx = (int16_t)((raw[8] << 8) | raw[9]);
    int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

    out->accel_x = ax / ACCEL_LSB_PER_G;
    out->accel_y = ay / ACCEL_LSB_PER_G;
    out->accel_z = az / ACCEL_LSB_PER_G;
    out->gyro_x = gx / GYRO_LSB_PER_DPS;
    out->gyro_y = gy / GYRO_LSB_PER_DPS;
    out->gyro_z = gz / GYRO_LSB_PER_DPS;
    return true;
}
