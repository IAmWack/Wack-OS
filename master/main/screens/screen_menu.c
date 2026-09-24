/*
 * Главное меню. Четыре карточки-раздела плюс Астольфо в углу с
 * репликой: он показывает состояние системы (ответил slave / молчит /
 * идёт запрос) — то же самое, что индикатор в шапке, но так на это
 * приятнее смотреть.
 */
#include "ui.h"
#include "ui_screen.h"
#include "app_logic.h"
#include "astolfo.h"
#include "settings.h"

#include <string.h>

typedef struct {
    const char *title;
    ui_icon_t   icon;
    screen_id_t target;
} entry_t;

static const entry_t ENTRIES[] = {
    { "Модули",    UI_ICON_BOW,   SCREEN_MODULES },
    { "Slave",     UI_ICON_PLUG,  SCREEN_SLAVE },
    { "Настройки", UI_ICON_GEAR,  SCREEN_SETTINGS },
    { "О системе", UI_ICON_HEART, SCREEN_ABOUT },
};
#define ENTRY_COUNT (sizeof(ENTRIES) / sizeof(ENTRIES[0]))

static ui_list_t s_list;

static void enter(void)
{
    ui_list_init(&s_list, ENTRY_COUNT, ENTRY_COUNT, 34);
}

static void row(size_t index, ui_row_t *out)
{
    strncpy(out->title, ENTRIES[index].title, sizeof(out->title) - 1);
    out->icon = ENTRIES[index].icon;
}

static void draw(uint32_t t)
{
    ui_list_draw(&s_list, CONTENT_Y, row, t);

    if (!settings_astolfo()) {
        return;
    }

    /* Персонаж и его реплика внизу: пока ничего не выбрано, экран не
     * выглядит пустым. */
    astolfo_draw(32, 212, 30, t);

    const char *line = ui_toast_text();
    if (!line || !line[0]) {
        line = astolfo_line();
    }
    ui_bubble(66, 216, 100, line);
}

static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;
        case UI_IN_PRESS: ui_goto(ENTRIES[s_list.selected].target); break;
        default: break; /* из корня меню возвращаться некуда */
    }
}

const ui_screen_t screen_menu = {
    .title = "wack-os",
    .enter = enter,
    .draw = draw,
    .input = input,
    .hint = "жми — выбрать",
};
