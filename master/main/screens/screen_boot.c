/**
 * @file    screen_boot.c
 * @author  IAmWack
 * @brief   Экран загрузки: пошаговый пинг slave с прогрессом.
 *
 * Прогресс не бутафорский: каждый шаг — это реальный запрос к slave
 * (пинг, версия, список модулей), поэтому если slave не подключён,
 * это видно прямо здесь, а не после входа в меню. Реализует контракт
 * ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "app_logic.h"
#include "astolfo.h"
#include "sound.h"

#include <stdio.h>

/* ============================================================================
 *  Defines
 * ============================================================================ */

#define STEP_INTERVAL_MS 380

/* ============================================================================
 *  Static state
 * ============================================================================ */

static uint32_t s_next_step_ms = 0;
static bool s_running = true;
static const char *s_label = "поднимаю интерфейс";
static int s_percent = 0;

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Сбросить состояние загрузки и запустить шаги заново. */
static void enter(void)
{
    app_init();
    astolfo_set_mood(ASTOLFO_THINKING);
    s_running = true;
    s_next_step_ms = 0;
    s_percent = 0;
    sound_play(SOUND_BOOT);
}

/**
 * @brief   Выполнить очередной шаг загрузки по таймеру и, если это
 *          последний шаг, перейти в главное меню.
 * @param   t  Текущее время кадра (мс).
 *
 * Шаги загрузки — в tick, а не в draw: каждый шаг лезет к slave, а
 * запрос к slave сам перерисовывает экран.
 */
static void tick(uint32_t t)
{
    if (!s_running || t < s_next_step_ms) {
        return;
    }
    s_running = app_boot_step(&s_label, &s_percent);
    s_next_step_ms = t + STEP_INTERVAL_MS;

    if (!s_running) {
        astolfo_set_mood(app_online() ? ASTOLFO_HAPPY : ASTOLFO_SAD);
        ui_replace(SCREEN_MENU);
    }
}

/**
 * @brief   Отрисовать Астольфо, прогресс сердечками и текущий шаг.
 * @param   t  Текущее время кадра (мс), для анимации персонажа.
 */
static void draw(uint32_t t)
{
    astolfo_draw(GFX_W / 2, CONTENT_Y + 2, 78, t);

    /* Прогресс — сердечками, а не полоской. */
    const int hearts = 5;
    int filled = s_percent * hearts / 100;
    for (int i = 0; i < hearts; i++) {
        int x = GFX_W / 2 - (hearts - 1) * 13 + i * 26;
        if (i < filled) {
            gfx_heart(x, 252, 13, TH->accent, 255);
        } else {
            gfx_heart(x, 252, 11, TH->card, 170);
        }
    }

    gfx_text_center(&wackos_font_small, GFX_W / 2, 268, s_label, TH->text, 245);

    char ver[48];
    snprintf(ver, sizeof(ver), "v0.1 · протокол v%s", app_slave_proto());
    gfx_text_center(&wackos_font_small, GFX_W / 2, 282, ver, TH->text_dim, 220);
}

/**
 * @brief   Обработать ввод: нажатие или "назад" пропускает загрузку.
 * @param   in  Событие энкодера.
 */
static void input(ui_input_t in)
{
    /* Загрузку можно проскочить нажатием — ждать её каждый раз незачем. */
    if (in == UI_IN_PRESS || in == UI_IN_BACK) {
        s_running = false;
        ui_replace(SCREEN_MENU);
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
    .hint = "загружаюсь...",
};
