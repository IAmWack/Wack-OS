/**
 * @file    screen_home.c
 * @author  IAmWack
 * @brief   Главный экран: циферблат с крупным маскотом — корень навигации.
 *
 * Не меню, а "домашний" экран в духе смарт-часов: большой портрет
 * маскота текущей темы, время и дата. Любое нажатие (неважно какое)
 * уводит в меню — разбираться в жестах тут незачем, это стартовая
 * точка. Реализует контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "mascot.h"
#include "settings.h"

#include <time.h>
#include <stdio.h>

/* ============================================================================
 *  Static state
 * ============================================================================ */

/* strftime("%a") в прошивке без локали даёт английские сокращения —
 * день недели по tm_wday (0 = воскресенье, как в struct tm) берём
 * из своего списка. */
static const char *const WEEKDAYS_RU[7] = {
    "вс", "пн", "вт", "ср", "чт", "пт", "сб",
};

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/**
 * @brief   Нарисовать портрет маскота, время и дату.
 * @param   t  Текущее время кадра (мс), для анимации персонажа.
 */
static void draw(uint32_t t)
{
    if (settings_mascot()) {
        mascot_draw_portrait(GFX_W / 2, CONTENT_Y, 136, 185);
    }

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);

    char hhmm[8];
    strftime(hhmm, sizeof(hhmm), "%H:%M", &lt);
    gfx_text_center(&wackos_font_title, GFX_W / 2, 254, hhmm, TH->text, 255);

    char dm[8];
    strftime(dm, sizeof(dm), "%d.%m", &lt);
    char date[24];
    snprintf(date, sizeof(date), "%s, %s", dm, WEEKDAYS_RU[lt.tm_wday % 7]);
    gfx_text_center(&wackos_font_small, GFX_W / 2, 282, date, TH->text_dim, 220);
}

/**
 * @brief   Обработать ввод: любое событие открывает меню.
 * @param   in  Событие ввода (не важно, какое именно).
 */
static void input(ui_input_t in)
{
    (void)in;
    ui_goto(SCREEN_MENU);
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица главного экрана для роутера ui.c. */
const ui_screen_t screen_home = {
    .title = "wack-os",
    .draw = draw,
    .input = input,
};
