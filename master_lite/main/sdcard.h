/**
 * @file    sdcard.h
 * @author  IAmWack
 * @brief   Публичный интерфейс встроенного слота microSD (master).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Поднять SDMMC-шину и смонтировать FAT-карту в "/sdcard".
 * @return  true, если карта найдена и смонтирована.
 *
 * Не фатально при неудаче (карта может быть не вставлена) — просто
 * логирует причину и возвращает false, остальная прошивка продолжает
 * работать без карты.
 */
bool sdcard_init(void);

/** @brief  true, если карта сейчас смонтирована в "/sdcard". */
bool sdcard_ready(void);

/** @brief  Объём карты в мегабайтах, либо 0, если не смонтирована. */
uint32_t sdcard_size_mb(void);

/**
 * @brief   Записать данные в файл на карте целиком (перезапись).
 * @param   path  Абсолютный путь ("/sdcard/..."); родительская папка
 *                одного уровня вложенности создаётся сама, если её нет
 *                (глубже — не нужно, см. вызовы в коде).
 * @param   data  Буфер с данными.
 * @param   len   Сколько байт из data записать.
 * @return  true, если карта смонтирована и файл записан целиком.
 */
bool sdcard_write_file(const char *path, const void *data, size_t len);

/**
 * @brief   Прочитать файл с карты в буфер (не больше buf_size байт).
 * @param   path      Абсолютный путь.
 * @param   buf       Буфер под содержимое.
 * @param   buf_size  Размер buf.
 * @param   out_len   Сколько байт реально прочитано (можно NULL).
 * @return  true, если файл нашёлся и открылся (даже если он короче buf_size).
 */
bool sdcard_read_file(const char *path, void *buf, size_t buf_size, size_t *out_len);
