/**
 * @file    ui.h
 * @author  IAmWack
 * @brief   Публичный интерфейс слоя UI master: экраны, навигация, анимация.
 *
 * Знает про меню и рисование, но ничего не знает про UART, протокол и
 * то, что физически умеет slave. С логикой общается только через
 * app_logic.h (спросить данные, выполнить действие).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    SCREEN_BOOT = 0,
    SCREEN_MENU,
    SCREEN_MODULES,
    SCREEN_MODULE,
    SCREEN_SLAVE,
    SCREEN_SETTINGS,
    SCREEN_ABOUT,
    SCREEN_COUNT,
} screen_id_t;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Инициализировать дисплей, энкодер, звук, настройки и показать boot. */
void ui_init(void);

/**
 * @brief   Один шаг интерфейса.
 *
 * Кадр (анимация живёт даже без действий пользователя) + разбор
 * события энкодера, если оно было.
 */
void ui_tick(void);

/** @brief  Перейти на экран, нарастив стек навигации. */
void ui_goto(screen_id_t id);
/**
 * @brief   Заменить текущий экран, не наращивая стек.
 *
 * Нужно загрузке: с неё возвращаться некуда.
 */
void ui_replace(screen_id_t id);
/** @brief  Вернуться на предыдущий экран (длинное нажатие). */
void ui_back(void);
/** @brief  Идентификатор текущего экрана. */
screen_id_t ui_current(void);

/** @brief  Короткое сообщение пользователю (реплика в облачке у Астольфо). */
void ui_toast(const char *text);
/** @brief  Текст последнего тоста. */
const char *ui_toast_text(void);

/** @brief  Установить состояние связи со slave — индикатор в шапке. */
void ui_set_link_online(bool online);
/** @brief  Текущее состояние связи со slave. */
bool ui_link_online(void);

/**
 * @brief   Отметить, идёт ли сейчас блокирующий запрос к slave.
 * @param   busy  true — сразу рисует кадр, чтобы "думаю" было видно.
 */
void ui_set_busy(bool busy);
