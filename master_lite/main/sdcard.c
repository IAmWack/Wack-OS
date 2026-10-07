/**
 * @file    sdcard.c
 * @author  IAmWack
 * @brief   Встроенный слот microSD платы (SDMMC, 4-битная шина, FAT).
 *
 * Знает только про монтирование: инициализировать SDMMC-хост по
 * фиксированным пинам этой платы (board_pins.h) и смонтировать FAT в
 * "/sdcard". Что лежит на карте и кто туда пишет — забота вызывающего
 * кода, сюда это не входит.
 */
#include "sdcard.h"
#include "board_pins.h"

#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "sdcard";

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define SD_MOUNT_POINT "/sdcard"

/* ============================================================================
 *  Static state
 * ============================================================================ */

static sdmmc_card_t *s_card = NULL;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Поднять SDMMC-шину и смонтировать FAT-карту в "/sdcard".
 */
bool sdcard_init(void)
{
    esp_vfs_fat_mount_config_t mount_cfg = {
        /* Карту не форматируем сами: 32 ГБ должны прийти уже в FAT32 из
         * коробки, затирать чужие данные без спроса не дело. */
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    /*
     * Слот этой платы распаян под все 4 линии данных (D0..D3) — гоним
     * 4-битную шину, она ощутимо быстрее 1-битной/SPI-эмуляции.
     */
    sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_cfg.clk = PIN_SD_CLK;
    slot_cfg.cmd = PIN_SD_CMD;
    slot_cfg.d0  = PIN_SD_D0;
    slot_cfg.d1  = PIN_SD_D1;
    slot_cfg.d2  = PIN_SD_D2;
    slot_cfg.d3  = PIN_SD_D3;
    slot_cfg.width = 4;
    slot_cfg.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_cfg,
                                            &mount_cfg, &s_card);
    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            ESP_LOGW(TAG, "не смонтировалась (карта вставлена? файловая система FAT32?)");
        } else {
            ESP_LOGW(TAG, "ошибка инициализации: %s", esp_err_to_name(err));
        }
        s_card = NULL;
        return false;
    }

    ESP_LOGI(TAG, "смонтирована: %s, %llu МБ", s_card->cid.name,
             ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));
    return true;
}

/** @brief  true, если карта сейчас смонтирована в "/sdcard". */
bool sdcard_ready(void)
{
    return s_card != NULL;
}

/** @brief  Объём карты в мегабайтах, либо 0, если не смонтирована. */
uint32_t sdcard_size_mb(void)
{
    if (!s_card) {
        return 0;
    }
    return (uint32_t)(((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));
}

/** @brief  Создать родительскую папку path (один уровень), если её нет. */
static void ensure_parent_dir(const char *path)
{
    char dir[160];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    char *slash = strrchr(dir, '/');
    if (!slash || slash == dir) {
        return;
    }
    *slash = '\0';
    mkdir(dir, 0775); /* ошибку (уже есть/нет карты) игнорируем — покажет сама запись файла */
}

/**
 * @brief   Записать данные в файл на карте целиком (перезапись).
 */
bool sdcard_write_file(const char *path, const void *data, size_t len)
{
    if (!s_card) {
        return false;
    }

    ensure_parent_dir(path);

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGW(TAG, "не открылся на запись: %s", path);
        return false;
    }
    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    if (written != len) {
        ESP_LOGW(TAG, "записалось не всё: %s (%u/%u байт)", path,
                 (unsigned)written, (unsigned)len);
        return false;
    }
    return true;
}

/**
 * @brief   Прочитать файл с карты в буфер (не больше buf_size байт).
 */
bool sdcard_read_file(const char *path, void *buf, size_t buf_size, size_t *out_len)
{
    if (!s_card) {
        return false;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    size_t n = fread(buf, 1, buf_size, f);
    fclose(f);

    if (out_len) {
        *out_len = n;
    }
    return true;
}
