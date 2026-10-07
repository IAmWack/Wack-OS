/**
 * @file    settings.c
 * @author  IAmWack
 * @brief   Настройки устройства (яркость, громкость, тема, Астольфо).
 *
 * Отдельный слой, потому что настройки читает и UI (показать ползунок),
 * и железо (яркость -> подсветка, громкость -> пищалка). Экран
 * настроек только дёргает эти функции.
 *
 * Переживают перезагрузку через файл на SD (см. SETTINGS_PATH) — не
 * NVS, потому что на этой плате и так есть карта, а лишний раздел NVS
 * под пяток скаляров ни к чему. Без карты (sdcard_write_file/
 * sdcard_read_file и так проверяют sdcard_ready() внутри) просто живут
 * в ОЗУ, как раньше — ничего не ломается.
 *
 * Запись — не на каждое изменение: во время "прокрутки" слайдера
 * (яркость/громкость/тема, см. editing в screen_settings.c) это было бы
 * по записи на каждый тик энкодера и подвисало бы кадр на время записи
 * на SD. Экран настроек сам зовёт settings_save() в нужный момент —
 * тумблеры (маскот, альбомная читалка) сразу, слайдеры — когда выходят
 * из режима правки.
 */
#include "settings.h"
#include "theme.h"
#include "display.h"
#include "sound.h"
#include "sdcard.h"

#include <stdio.h>
#include <string.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define SETTINGS_PATH "/sdcard/wackos/settings.cfg"

/* ============================================================================
 *  Static state
 * ============================================================================ */

static uint8_t s_brightness = 80;
static uint8_t s_volume = 6;
static bool s_mascot = true;
static bool s_reader_landscape = false;

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/** @brief  Прочитать settings.cfg с SD (если есть) и разобрать построчно "ключ=значение". */
static void load(void)
{
    char buf[192];
    size_t n = 0;
    if (!sdcard_read_file(SETTINGS_PATH, buf, sizeof(buf) - 1, &n)) {
        return; /* нет файла (первый запуск) или нет карты — остаются дефолты выше */
    }
    buf[n] = '\0';

    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }

        char key[24];
        unsigned val;
        if (sscanf(line, "%23[^=]=%u", key, &val) == 2) {
            if (strcmp(key, "brightness") == 0) {
                s_brightness = (uint8_t)val;
            } else if (strcmp(key, "volume") == 0) {
                s_volume = (uint8_t)val;
            } else if (strcmp(key, "theme") == 0 && val < theme_count()) {
                theme_set(val);
            } else if (strcmp(key, "mascot") == 0) {
                s_mascot = (val != 0);
            } else if (strcmp(key, "reader_landscape") == 0) {
                s_reader_landscape = (val != 0);
            }
        }

        line = nl ? nl + 1 : NULL;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Загрузить настройки с SD (если есть) и применить к железу. */
void settings_init(void)
{
    load();
    display_set_brightness(s_brightness);
    sound_set_volume(s_volume);
}

/** @brief  Сохранить текущие настройки в settings.cfg на SD. */
void settings_save(void)
{
    char buf[192];
    int n = snprintf(buf, sizeof(buf),
                     "brightness=%u\n"
                     "volume=%u\n"
                     "theme=%u\n"
                     "mascot=%u\n"
                     "reader_landscape=%u\n",
                     s_brightness, s_volume, (unsigned)theme_index(),
                     s_mascot ? 1 : 0, s_reader_landscape ? 1 : 0);
    sdcard_write_file(SETTINGS_PATH, buf, (size_t)n);
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

/** @brief  Показывать ли маскота текущей темы (вдруг захочется скриншот без персонажа). */
bool settings_mascot(void)
{
    return s_mascot;
}

/** @brief  Включить/выключить отрисовку маскота. */
void settings_set_mascot(bool on)
{
    s_mascot = on;
}

/** @brief  Альбомная раскладка при просмотре файлов с SD. */
bool settings_reader_landscape(void)
{
    return s_reader_landscape;
}

/** @brief  Включить/выключить альбомную раскладку чтения. */
void settings_set_reader_landscape(bool on)
{
    s_reader_landscape = on;
}
