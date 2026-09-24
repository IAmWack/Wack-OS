/**
 * @file    screen_module.c
 * @author  IAmWack
 * @brief   Экран одного модуля: что выбрали, что ответил slave.
 *
 * Ответ — произвольная строка, master её не разбирает (и не должен),
 * только показывает. Можно запустить модуль ещё раз. Реализует
 * контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "app_logic.h"
#include "astolfo.h"
#include "screens.h"

#include <stdio.h>

/* ============================================================================
 *  Static state
 * ============================================================================ */

static size_t s_index = 0;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Запомнить, какой модуль выбрали в списке (screen_modules.c),
 *          перед переходом на этот экран.
 * @param   index  Индекс модуля в списке app_logic.
 */
void screen_module_set(size_t index)
{
    s_index = index;
}

/**
 * @brief   Индекс модуля, выбранного для этого экрана.
 * @return  Индекс, ранее установленный screen_module_set().
 */
size_t screen_module_get(void)
{
    return s_index;
}

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Войти в модуль = сразу выполнить его. */
static void enter(void)
{
    app_run_item(s_index); /* войти в модуль = выполнить его */
}

/**
 * @brief   Отрисовать карточку модуля: название, ответ slave, RTT.
 * @param   t  Текущее время кадра (мс), для анимации персонажа.
 */
static void draw(uint32_t t)
{
    const slave_item_t *item = app_item(s_index);

    ui_card(8, CONTENT_Y, GFX_W - 16, 150, 235);

    /* Заголовок карточки: иконка + название модуля. */
    ui_icon(UI_ICON_SPARK, 26, CONTENT_Y + 20, 12, TH->accent, 255);
    gfx_text_ellipsis(&wackos_font_body, 42, CONTENT_Y + 12,
                      item ? item->title : "модуль", GFX_W - 60, TH->text, 255);

    gfx_line(20, CONTENT_Y + 36, GFX_W - 28, CONTENT_Y + 36, 1, TH->accent_soft, 160);

    /* Ответ slave — до пяти строк с переносом по словам. */
    ui_text_block(&wackos_font_small, 20, CONTENT_Y + 46, GFX_W - 40, 5,
                  app_last_result(), TH->text);

    char rtt[32];
    snprintf(rtt, sizeof(rtt), "ответ за %u мс", (unsigned)app_rtt_ms());
    gfx_text(&wackos_font_small, 20, CONTENT_Y + 130, rtt, TH->text_dim, 220);

    astolfo_draw(134, CONTENT_Y + 168, 26, t);
}

/**
 * @brief   Обработать ввод: нажатие запускает модуль ещё раз, "назад"
 *          уводит в список модулей.
 * @param   in  Событие энкодера.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_PRESS:
            app_run_item(s_index);
            break;
        case UI_IN_BACK:
            ui_back();
            break;
        default:
            break;
    }
}

/** @brief  Таблица экрана модуля для роутера ui.c. */
const ui_screen_t screen_module = {
    .title = "Модуль",
    .enter = enter,
    .draw = draw,
    .input = input,
    .hint = "жми: ещё раз|держи ←",
};
