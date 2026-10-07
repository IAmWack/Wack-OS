/**
 * @file    portrait_sd.c
 * @author  IAmWack
 * @brief   Общий загрузчик растровых портретов маскотов с microSD (см. portrait_sd.h).
 *
 * Карта сейчас физически может быть без /portraits/ (ассеты ещё не
 * перенесены) — на этот случай для пары настроений есть запасной
 * вариант прямо во флеше (portrait_fallback.h/.c): если файла на карте
 * нет, портрет всё равно найдётся, если он есть в PORTRAIT_FALLBACKS.
 * Как только карта с настоящими файлами встанет в плату — SD-файлы
 * возьмут верх сами (см. portrait_sd_load()), фоллбэк не трогать не
 * придётся.
 */
#include "portrait_sd.h"
#include "portrait_fallback.h"

#include <stdio.h>
#include <string.h>

/* ============================================================================
 *  Static state
 * ============================================================================ */

/* Один буфер на всю систему — одновременно показывается только один
 * портрет, нет смысла кэшировать все 15 файлов в ОЗУ. */
static uint16_t s_buf[PORTRAIT_SD_W * PORTRAIT_SD_H];
static char s_loaded_key[32] = "";

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Указатель на встроенный запасной портрет, если он есть для этой пары. */
static const uint16_t *find_fallback(const char *character, const char *mood)
{
    for (size_t i = 0; i < PORTRAIT_FALLBACK_COUNT; i++) {
        if (strcmp(PORTRAIT_FALLBACKS[i].character, character) == 0 &&
            strcmp(PORTRAIT_FALLBACKS[i].mood, mood) == 0) {
            return PORTRAIT_FALLBACKS[i].pixels;
        }
    }
    return NULL;
}

/**
 * @brief   Загрузить (с кэшем) портрет "<character>_<mood>.bin" с карты,
 *          а если его там нет — встроенный запасной вариант, если есть.
 */
const uint16_t *portrait_sd_load(const char *character, const char *mood)
{
    char key[32];
    snprintf(key, sizeof(key), "%s_%s", character, mood);

    if (strcmp(key, s_loaded_key) == 0) {
        return s_buf; /* уже в кэше, на карту не ходим */
    }

    char path[64];
    snprintf(path, sizeof(path), "/sdcard/portraits/%s.bin", key);

    FILE *f = fopen(path, "rb");
    if (!f) {
        s_loaded_key[0] = '\0';
        return find_fallback(character, mood);
    }

    size_t n = fread(s_buf, sizeof(uint16_t), PORTRAIT_SD_W * PORTRAIT_SD_H, f);
    fclose(f);

    if (n != (size_t)(PORTRAIT_SD_W * PORTRAIT_SD_H)) {
        s_loaded_key[0] = '\0'; /* битый/неполный файл — не кэшировать как валидный */
        return find_fallback(character, mood);
    }

    strncpy(s_loaded_key, key, sizeof(s_loaded_key) - 1);
    s_loaded_key[sizeof(s_loaded_key) - 1] = '\0';
    return s_buf;
}
