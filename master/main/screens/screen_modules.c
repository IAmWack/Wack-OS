/**
 * @file    screen_modules.c
 * @author  IAmWack
 * @brief   Список модулей, которые о себе сообщил slave (UI_MENU_GET).
 *
 * Master заранее про модули ничего не знает — что пришло, то и
 * показывает. В этом весь смысл протокола: сменил плату — сменился
 * список, интерфейс трогать не нужно. Реализует контракт ui_screen_t
 * (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "app_logic.h"
#include "astolfo.h"
#include "screens.h"

#include <string.h>

/* ============================================================================
 *  Static state
 * ============================================================================ */

static ui_list_t s_list;

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Инициализировать список текущим набором модулей от slave. */
static void enter(void)
{
    ui_list_init(&s_list, app_item_count(), 5, 28);
}

/**
 * @brief   Заполнить одну строку списка данными модуля.
 * @param   index  Индекс модуля в списке app_logic.
 * @param   out    Строка списка для заполнения (title, icon).
 */
static void row(size_t index, ui_row_t *out)
{
    const slave_item_t *item = app_item(index);
    if (item) {
        strncpy(out->title, item->title, sizeof(out->title) - 1);
    }
    out->icon = UI_ICON_SPARK;
}

/**
 * @brief   Отрисовать список модулей, либо заглушку "модулей нет",
 *          если slave ничего не отдал.
 * @param   t  Текущее время кадра (мс), для анимации персонажа.
 */
static void draw(uint32_t t)
{
    if (app_item_count() == 0) {
        ui_card(8, CONTENT_Y, GFX_W - 16, 92, 235);
        ui_text_block(&wackos_font_body, 20, CONTENT_Y + 16, GFX_W - 40, 3,
                      app_online() ? "slave не отдал ни одного модуля"
                                   : "slave не на связи",
                      TH->text);
        ui_text_block(&wackos_font_small, 20, CONTENT_Y + 68, GFX_W - 40, 1,
                      "нажми — обновить список", TH->text_dim);
        astolfo_draw(GFX_W / 2, CONTENT_Y + 110, 40, t);
        return;
    }

    ui_list_draw(&s_list, CONTENT_Y, row, t);
    astolfo_draw(140, 232, 24, t);
}

/**
 * @brief   Обработать ввод: прокрутка, вход в модуль, либо обновление
 *          списка, если он пуст.
 * @param   in  Событие энкодера.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;

        case UI_IN_PRESS:
            if (app_item_count() == 0) {
                app_refresh_items();
                ui_list_init(&s_list, app_item_count(), 5, 28);
                break;
            }
            screen_module_set(s_list.selected);
            ui_goto(SCREEN_MODULE);
            break;

        case UI_IN_BACK:
            ui_back();
            break;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица экрана списка модулей для роутера ui.c. */
const ui_screen_t screen_modules = {
    .title = "Модули",
    .enter = enter,
    .draw = draw,
    .input = input,
    .hint = "жми: пуск|держи ←",
};
