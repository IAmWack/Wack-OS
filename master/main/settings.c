/**
 * @file    settings.c
 * @author  IAmWack
 * @brief   Настройки устройства (яркость, громкость, тема, Астольфо).
 *
 * Отдельный слой, потому что настройки читает и UI (показать ползунок),
 * и железо (яркость -> подсветка, громкость -> пищалка). Экран
 * настроек только дёргает эти функции.
 *
 * @todo Сохранять в NVS, чтобы переживали перезагрузку. Пока живут
 *       только в ОЗУ — до появления платы это ничего не меняет.
 */
#include "settings.h"
#include "theme.h"
#include "display.h"
#include "sound.h"

/* ============================================================================
 *  Static state
 * ============================================================================ */

static uint8_t s_brightness = 80;
static uint8_t s_volume = 6;
static bool s_astolfo = true;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Применить сохранённые настройки к железу (подсветка, звук, тема). */
void settings_init(void)
{
    display_set_brightness(s_brightness);
    sound_set_volume(s_volume);
    theme_set(0);
}

/** @brief  Текущая яркость подсветки, 10..100 %. */
uint8_t settings_brightness(void)
{
    return s_brightness;
}

/**
 * @brief   Установить яркость подсветки.
 * @param   percent  Значение 10..100 % (за пределами — обрезается).
 */
void settings_set_brightness(uint8_t percent)
{
    if (percent < 10) percent = 10;
    if (percent > 100) percent = 100;
    s_brightness = percent;
    display_set_brightness(s_brightness);
}

/** @brief  Текущая громкость пищалки, 0..10 (0 — тишина). */
uint8_t settings_volume(void)
{
    return s_volume;
}

/**
 * @brief   Установить громкость пищалки.
 * @param   level  Значение 0..10 (за пределами — обрезается).
 */
void settings_set_volume(uint8_t level)
{
    if (level > 10) level = 10;
    s_volume = level;
    sound_set_volume(s_volume);
}

/** @brief  Индекс текущей темы — прокси к theme_index(). */
size_t settings_theme(void)
{
    return theme_index();
}

/**
 * @brief   Установить тему по индексу — прокси к theme_set().
 * @param   index  Индекс темы (см. theme_count()).
 */
void settings_set_theme(size_t index)
{
    theme_set(index);
}

/** @brief  Показывать ли Астольфо (вдруг захочется скриншот без персонажа). */
bool settings_astolfo(void)
{
    return s_astolfo;
}

/** @brief  Включить/выключить отрисовку Астольфо. */
void settings_set_astolfo(bool on)
{
    s_astolfo = on;
}
