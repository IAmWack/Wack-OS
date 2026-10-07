/**
 * @file    screen_about.c
 * @author  IAmWack
 * @brief   Экран "О системе": версия и карта.
 *
 * master_lite автономен — тут нет данных о slave, только то, что
 * прошивка знает о себе сама. Реализует контракт ui_screen_t (см.
 * ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "sdcard.h"

#include <stdio.h>

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/**
 * @brief   Отрисовать карточку с версиями (прошивка, карта).
 * @param   t  Текущее время кадра (мс).
 */
static void draw(uint32_t t)
{
    (void)t;
    ui_card(8, CONTENT_Y, GFX_W - 16, 96, 235);

    gfx_text_center(&wackos_font_title, GFX_W / 2, CONTENT_Y + 6, "wack-os lite", TH->accent, 255);
    const int ix = 22, iw = GFX_W - 48;
    ui_info_line(ix, CONTENT_Y + 38, iw, "версия", "0.1 lite", TH->text);

    char sd[32];
    if (sdcard_ready()) {
        snprintf(sd, sizeof(sd), "%u МБ", (unsigned)sdcard_size_mb());
    } else {
        snprintf(sd, sizeof(sd), "нет карты");
    }
    ui_info_line(ix, CONTENT_Y + 52, iw, "microSD", sd, TH->text);

    /* Маскот — только на главном экране (screen_home.c), тут без него:
     * без портрета в нижней половине остаётся пусто, так что реплику
     * поднимаем сразу под карточку. */
    gfx_text_center(&wackos_font_small, GFX_W / 2, CONTENT_Y + 118, "сделано с ♥ для себя",
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
};
