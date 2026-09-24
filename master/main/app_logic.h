/**
 * @file    app_logic.h
 * @author  IAmWack
 * @brief   Публичный интерфейс слоя логики master.
 *
 * Единственное место, которое знает имена команд протокола (SYS_PING,
 * UI_MENU_GET и т.д.) и что с ними делать. Экраны (master/main/screens/)
 * про протокол не знают — они дёргают функции отсюда и показывают то,
 * что вернулось.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define SLAVE_ITEM_MAX   8
#define SLAVE_TITLE_LEN  28

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef struct {
    char id[16];                 /* id для UI_MENU_SELECT */
    char title[SLAVE_TITLE_LEN]; /* что показать пользователю */
} slave_item_t;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Сбросить состояние слоя логики (вызывается при старте master). */
void app_init(void);

/**
 * @brief   Один шаг загрузки (пинг, версия, список модулей...).
 * @param   out_label    Куда положить подпись текущего шага (может быть NULL).
 * @param   out_percent  Куда положить прогресс в процентах (может быть NULL).
 * @return  false, когда загрузка закончена.
 *
 * Разбито по шагам, чтобы экран загрузки показывал реальный прогресс,
 * а не бутафорский прогресс-бар.
 */
bool app_boot_step(const char **out_label, int *out_percent);

/* ---- Что мы знаем о slave ---- */

/** @brief  true, если последний обмен со slave прошёл (OK или ERR, не тайм-аут). */
bool        app_online(void);
const char *app_slave_fw(void);     /* версия прошивки slave */
const char *app_slave_proto(void);  /* версия протокола, о которой он сказал */
const char *app_slave_caps(void);   /* список команд через ';' */
uint32_t    app_rtt_ms(void);       /* время последнего ответа */

/** @brief  Количество пунктов меню, полученных последним app_refresh_items(). */
size_t              app_item_count(void);
/**
 * @brief   Получить пункт меню по индексу.
 * @param   index  Индекс в диапазоне [0, app_item_count()).
 * @return  Указатель на пункт меню, либо NULL при выходе за диапазон.
 */
const slave_item_t *app_item(size_t index);

/* ---- Действия (блокирующие, с тайм-аутом и ретраями протокола) ---- */

/** @brief  Проверить, жив ли slave (SYS_PING). @return true, если ответил OK. */
bool app_ping(void);
/** @brief  Запросить версию протокола и прошивки slave (SYS_VERSION). */
bool app_read_version(void);
/** @brief  Запросить список поддерживаемых команд slave (SYS_CAPS). */
bool app_read_caps(void);
/** @brief  Отправить slave команду сброса (SYS_RESET). */
bool app_reset_slave(void);
/** @brief  Запросить у slave список модулей меню (UI_MENU_GET). */
bool app_refresh_items(void);
/**
 * @brief   Выбрать пункт меню по индексу (UI_MENU_SELECT).
 * @param   index  Индекс в списке, полученном app_refresh_items().
 */
bool app_run_item(size_t index);

/** @brief  Результат последнего действия — то, что показывать на экране. */
const char *app_last_result(void);
