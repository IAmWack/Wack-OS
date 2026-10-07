/**
 * @file    battery.h
 * @author  IAmWack
 * @brief   Чтение заряда LiPo-батареи (VBAT напрямую на ADC, без делителя).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Настроить ADC под PIN_BATTERY_ADC (ничего не делает, если он -1). */
void battery_init(void);

/** @brief  Подключён ли VBAT к ADC (PIN_BATTERY_ADC != -1) — без этого остальные функции смысла не имеют. */
bool battery_present(void);

/** @brief  Напряжение батареи, милливольты (0, если battery_present() == false). */
uint32_t battery_voltage_mv(void);

/** @brief  Грубая оценка заряда, 0..100 % (линейно между 3300 и 4200 мВ). */
uint8_t battery_percent(void);
