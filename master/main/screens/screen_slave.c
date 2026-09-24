/**
 * @file    screen_slave.c
 * @author  IAmWack
 * @brief   Экран состояния slave: кто на связи и что с ним можно сделать.
 *
 * Всё, что тут написано, master узнал по протоколу — никакой зашитой
 * информации о конкретной плате здесь нет. Реализует контракт
 * ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "app_logic.h"
#include "astolfo.h"

#include <stdio.h>
#include <string.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    ACT_PING = 0,
    ACT_REFRESH,
    ACT_CAPS,
    ACT_RESET,
    ACT_COUNT,
} action_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

static const char *ACTION_NAMES[ACT_COUNT] = {
    "Пинг",
    "Обновить модули",
    "Что умеет",
    "Перезагрузить",
};

static ui_list_t s_list;

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Инициализировать список действий над slave. */
static void enter(void)
{
    ui_list_init(&s_list, ACT_COUNT, 4, 26);
}

/**
 * @brief   Заполнить одну строку списка названием и иконкой действия.
 * @param   index  Индекс действия (action_t).
 * @param   out    Строка списка для заполнения (title, icon).
 */
static void row(size_t index, ui_row_t *out)
{
    strncpy(out->title, ACTION_NAMES[index], sizeof(out->title) - 1);
    out->icon = (index == ACT_RESET) ? UI_ICON_PLUG : UI_ICON_HEART;
}

/**
 * @brief   Отрисовать карточку статуса связи и список доступных действий.
 * @param   t  Текущее время кадра (мс).
 */
static void draw(uint32_t t)
{
    ui_card(8, CONTENT_Y, GFX_W - 16, 88, 235);

    bool on = app_online();
    gfx_circle(24, CONTENT_Y + 14, 5, on ? TH->ok : TH->alert, 255);
    gfx_text(&wackos_font_body, 36, CONTENT_Y + 5,
             on ? "на связи" : "не отвечает", TH->text, 255);

    char rtt[24];
    snprintf(rtt, sizeof(rtt), "%u мс", (unsigned)app_rtt_ms());

    const int ix = 20, iw = GFX_W - 44;
    ui_info_line(ix, CONTENT_Y + 30, iw, "адрес", "01", TH->text);
    ui_info_line(ix, CONTENT_Y + 44, iw, "протокол", app_slave_proto(), TH->text);
    ui_info_line(ix, CONTENT_Y + 58, iw, "прошивка", app_slave_fw(), TH->text);
    ui_info_line(ix, CONTENT_Y + 72, iw, "ответ", rtt, on ? TH->text : TH->text_dim);

    ui_list_draw(&s_list, CONTENT_Y + 100, row, t);
}

/**
 * @brief   Обработать ввод: прокрутка списка действий и запуск
 *          выбранного (пинг / обновить модули / что умеет / сброс).
 * @param   in  Событие энкодера.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;

        case UI_IN_PRESS:
            switch ((action_t)s_list.selected) {
                case ACT_PING:    app_ping(); break;
                case ACT_REFRESH: app_refresh_items(); break;
                case ACT_CAPS:    app_read_caps(); break;
                case ACT_RESET:   app_reset_slave(); break;
                default: break;
            }
            break;

        case UI_IN_BACK:
            ui_back();
            break;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица экрана состояния slave для роутера ui.c. */
const ui_screen_t screen_slave = {
    .title = "Slave",
    .enter = enter,
    .draw = draw,
    .input = input,
    .hint = "жми: сделать|держи ←",
};
