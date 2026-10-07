/**
 * @file    ui.h
 * @author  IAmWack
 * @brief   Публичный интерфейс слоя UI master: экраны, навигация, анимация.
 *
 * master_lite: автономная ветка без связи со slave (см. CMakeLists.txt) —
 * знает только про меню и рисование, никакого app_logic/протокола тут нет.
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
    SCREEN_HOME,     /* циферблат — корень навигации, см. screen_home.c */
    SCREEN_MENU,
    SCREEN_SETTINGS,
    SCREEN_ABOUT,
    SCREEN_FILES,
    SCREEN_CLOCK,
    SCREEN_REMOTE,
    SCREEN_REMOTE_KEYBOARD,
    SCREEN_REMOTE_MOUSE,
    SCREEN_BLE_SCAN,
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
/** @brief  Сбросить стек навигации и вернуться сразу на главный экран (циферблат). */
void ui_home(void);
/** @brief  Идентификатор текущего экрана. */
screen_id_t ui_current(void);

/** @brief  Короткое всплывающее сообщение пользователю (видно на любом экране пару секунд). */
void ui_toast(const char *text);
/** @brief  Текст последнего тоста. */
const char *ui_toast_text(void);

