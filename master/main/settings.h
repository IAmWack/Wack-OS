/**
 * @file    settings.h
 * @author  IAmWack
 * @brief   Публичный интерфейс настроек устройства.
 *
 * Отдельный слой, потому что их читает и UI (показать ползунок), и
 * железо (яркость -> подсветка, громкость -> пищалка). Экран настроек
 * только дёргает эти функции.
 *
 * @todo Сохранять в NVS, чтобы переживали перезагрузку. Пока живут
 *       только в ОЗУ — до появления платы это ничего не меняет.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Применить сохранённые настройки к железу. */
void settings_init(void);

/* Яркость подсветки, 10..100 %. */
uint8_t settings_brightness(void);
void    settings_set_brightness(uint8_t percent);

/* Громкость пищалки, 0..10 (0 — тишина). */
uint8_t settings_volume(void);
void    settings_set_volume(uint8_t level);

/* Индекс темы — просто прокси к theme_set(), чтобы экран настроек
 * работал со всеми пунктами одинаково. */
size_t settings_theme(void);
void   settings_set_theme(size_t index);

/* Показывать ли Астольфо (вдруг захочется скриншот без персонажа). */
bool settings_astolfo(void);
void settings_set_astolfo(bool on);
