/**
 * @file    screen_boot.c
 * @author  IAmWack
 * @brief   Экран загрузки: короткая заставка перед главным меню.
 *
 * master_lite автономен (нет slave) — прогресс тут чисто декоративный,
 * по времени, а не по реальным шагам. Реализует контракт ui_screen_t
 * (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "mascot.h"
#include "sound.h"

#include <stdio.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/* Сколько идёт заставка, прежде чем сама уйдёт в меню. */
#define BOOT_DURATION_MS 1400

/* ============================================================================
 *  Static state
 * ============================================================================ */

static uint32_t s_start_ms = 0;
static bool s_running = true;

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Запустить заставку заново. */
static void enter(void)
{
    mascot_set_mood(MASCOT_THINKING);
    s_running = true;
    s_start_ms = 0;
    sound_play(SOUND_BOOT);
}

/**
 * @brief   По истечении BOOT_DURATION_MS перейти в главное меню.
 * @param   t  Текущее время кадра (мс).
 */
static void tick(uint32_t t)
{
    if (!s_running) {
        return;
    }
    if (s_start_ms == 0) {
        s_start_ms = t;
    }
    if (t - s_start_ms >= BOOT_DURATION_MS) {
        s_running = false;
        mascot_set_mood(MASCOT_HAPPY);
        ui_replace(SCREEN_HOME);
    }
}

/**
 * @brief   Отрисовать вордмарк и прогресс сердечками (маскот — только
 *          на главном экране, см. screen_home.c, сюда не выводится).
 * @param   t  Текущее время кадра (мс).
 */
static void draw(uint32_t t)
{
    int cy = CONTENT_Y + CONTENT_H / 2;

    gfx_text_center(&wackos_font_title, GFX_W / 2, cy - 54, "wack-os", TH->accent, 255);

    int elapsed = s_start_ms ? (int)(t - s_start_ms) : 0;
    int percent = elapsed * 100 / BOOT_DURATION_MS;
    if (percent > 100) {
        percent = 100;
    }

    const int hearts = 5;
    int filled = percent * hearts / 100;
    for (int i = 0; i < hearts; i++) {
        int x = GFX_W / 2 - (hearts - 1) * 13 + i * 26;
        if (i < filled) {
            gfx_heart(x, cy, 13, TH->accent, 255);
        } else {
            gfx_heart(x, cy, 11, TH->card, 170);
        }
    }

    gfx_text_center(&wackos_font_small, GFX_W / 2, cy + 24, "поднимаю интерфейс", TH->text, 245);
    gfx_text_center(&wackos_font_small, GFX_W / 2, cy + 38, "v0.1 lite", TH->text_dim, 220);
}

/**
 * @brief   Обработать ввод: нажатие или "назад" пропускает заставку.
 * @param   in  Событие энкодера.
 */
static void input(ui_input_t in)
{
    if (in == UI_IN_PRESS || in == UI_IN_BACK) {
        s_running = false;
        ui_replace(SCREEN_HOME);
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица экрана загрузки для роутера ui.c. */
const ui_screen_t screen_boot = {
    .title = "wack-os",
    .enter = enter,
    .draw = draw,
    .tick = tick,
    .input = input,
};
