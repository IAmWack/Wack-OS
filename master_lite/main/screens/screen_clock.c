/**
 * @file    screen_clock.c
 * @author  IAmWack
 * @brief   Экран настройки даты и времени — подэкран screen_settings.c.
 *
 * Отдельным экраном, а не строчками в общем списке настроек: нет
 * RTC-микросхемы (см. clock.h), время сбивается при каждой перепрошивке,
 * и его правка — самостоятельная задача. Открывается только из
 * "Настроек" (пункт "Время"), в главном меню этого экрана нет.
 * Реализует контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "clock.h"

#include <time.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    CLOCK_HOUR = 0,
    CLOCK_MINUTE,
    CLOCK_DAY,
    CLOCK_MONTH,
    CLOCK_YEAR,
    CLOCK_BACK,
    CLOCK_COUNT,
} clock_setting_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

static ui_list_t s_list;

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Инициализировать список пунктов. */
static void enter(void)
{
    ui_list_init(&s_list, CLOCK_COUNT, CLOCK_COUNT, 30);
}

/**
 * @brief   Заполнить строку текущим часом/минутой либо пунктом "Назад".
 * @param   index  Индекс настройки (clock_setting_t).
 * @param   out    Строка списка для заполнения.
 */
static void row(size_t index, ui_row_t *out)
{
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);

    switch ((clock_setting_t)index) {
        case CLOCK_HOUR:
            strncpy(out->title, "Час", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%02d", lt.tm_hour);
            out->icon = UI_ICON_SPARK;
            break;
        case CLOCK_MINUTE:
            strncpy(out->title, "Минута", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%02d", lt.tm_min);
            out->icon = UI_ICON_SPARK;
            break;
        case CLOCK_DAY:
            strncpy(out->title, "День", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%02d", lt.tm_mday);
            out->icon = UI_ICON_BOW;
            break;
        case CLOCK_MONTH:
            strncpy(out->title, "Месяц", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%02d", lt.tm_mon + 1);
            out->icon = UI_ICON_BOW;
            break;
        case CLOCK_YEAR:
            strncpy(out->title, "Год", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%d", lt.tm_year + 1900);
            out->icon = UI_ICON_BOW;
            break;
        default: /* CLOCK_BACK */
            strncpy(out->title, "Назад", sizeof(out->title) - 1);
            out->value[0] = '\0';
            out->icon = UI_ICON_PLUG;
            break;
    }
}

/**
 * @brief   Изменить час/минуту на шаг вперёд/назад.
 * @param   delta  Направление (+1/-1 от энкодера).
 */
static void adjust(int delta)
{
    switch ((clock_setting_t)s_list.selected) {
        case CLOCK_HOUR:   clock_adjust_hours(delta);   break;
        case CLOCK_MINUTE: clock_adjust_minutes(delta); break;
        case CLOCK_DAY:    clock_adjust_days(delta);    break;
        case CLOCK_MONTH:  clock_adjust_months(delta);  break;
        case CLOCK_YEAR:   clock_adjust_years(delta);   break;
        default: break;
    }
}

/** @brief  Отрисовать список. */
static void draw(uint32_t t)
{
    ui_list_draw(&s_list, CONTENT_Y, row, t);
}

/**
 * @brief   Обработать ввод: прокрутка/правка значения, вход и выход
 *          из режима правки, "Назад" — сразу, без режима правки.
 * @param   in  Событие ввода.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT:
            if (s_list.editing) adjust(1); else ui_list_move(&s_list, 1);
            break;
        case UI_IN_PREV:
            if (s_list.editing) adjust(-1); else ui_list_move(&s_list, -1);
            break;
        case UI_IN_PRESS:
            if ((clock_setting_t)s_list.selected == CLOCK_BACK) {
                ui_back();
            } else {
                s_list.editing = !s_list.editing;
            }
            break;
        case UI_IN_BACK:
            if (s_list.editing) {
                s_list.editing = false;
            } else {
                ui_back();
            }
            break;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица экрана часов для роутера ui.c. */
const ui_screen_t screen_clock = {
    .title = "Время",
    .enter = enter,
    .draw = draw,
    .input = input,
};
