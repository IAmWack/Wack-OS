/**
 * @file    screen_about.c
 * @author  IAmWack
 * @brief   Экран "О системе": версии, немного цифр и персонаж.
 *
 * Экран нужен не только ради красоты — версия протокола и прошивки
 * slave тут те, которые реально пришли по проводу (см. app_logic.c).
 * Реализует контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "app_logic.h"
#include "astolfo.h"

#include <stdio.h>

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/**
 * @brief   Отрисовать карточку с версиями (прошивка, протокол, число
 *          модулей) и Астольфо снизу.
 * @param   t  Текущее время кадра (мс), для анимации персонажа.
 */
static void draw(uint32_t t)
{
    ui_card(8, CONTENT_Y, GFX_W - 16, 96, 235);

    gfx_text_center(&wackos_font_title, GFX_W / 2, CONTENT_Y + 6, "wack-os", TH->accent, 255);
    const int ix = 22, iw = GFX_W - 48;
    ui_info_line(ix, CONTENT_Y + 38, iw, "версия", "0.1", TH->text);
    ui_info_line(ix, CONTENT_Y + 52, iw, "протокол", app_slave_proto(), TH->text);
    ui_info_line(ix, CONTENT_Y + 66, iw, "прошивка", app_slave_fw(), TH->text);

    char items[32];
    snprintf(items, sizeof(items), "%u", (unsigned)app_item_count());
    ui_info_line(ix, CONTENT_Y + 80, iw, "модулей", items, TH->text);

    astolfo_draw(GFX_W / 2, CONTENT_Y + 112, 44, t);

    gfx_text_center(&wackos_font_small, GFX_W / 2, 276, "сделано с ♥ для себя",
                    TH->text, 235);
}

/**
 * @brief   Обработать ввод: любое нажатие уводит назад в меню.
 * @param   in  Событие энкодера.
 */
static void input(ui_input_t in)
{
    if (in == UI_IN_BACK || in == UI_IN_PRESS) {
        ui_back();
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица экрана "О системе" для роутера ui.c. */
const ui_screen_t screen_about = {
    .title = "О системе",
    .enter = NULL,
    .draw = draw,
    .input = input,
    .hint = "держи — назад",
};
