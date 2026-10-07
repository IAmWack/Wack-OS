/**
 * @file    settings.h
 * @author  IAmWack
 * @brief   Публичный интерфейс настроек устройства.
 *
 * Отдельный слой, потому что их читает и UI (показать ползунок), и
 * железо (яркость -> подсветка, громкость -> пищалка). Экран настроек
 * только дёргает эти функции.
 *
 * Переживают перезагрузку через файл на SD (см. settings.c) — писать
 * на каждое изменение не нужно, settings_save() зовётся явно в нужный
 * момент (экраном настроек).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Загрузить настройки с SD (если есть) и применить к железу. */
void settings_init(void);

/** @brief  Сохранить текущие настройки в файл на SD. */
void settings_save(void);

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

/* Показывать ли маскота на главном экране (вдруг захочется скриншот без
 * персонажа) — единственное место в UI, где он остался, см. screen_home.c. */
bool settings_mascot(void);
void settings_set_mascot(bool on);

/* Альбомная раскладка при просмотре файлов с SD (см. screen_files.c):
 * экран физически портретный, так что имеет смысл только если реально
 * держать плату боком. По умолчанию выключена. */
bool settings_reader_landscape(void);
void settings_set_reader_landscape(bool on);
