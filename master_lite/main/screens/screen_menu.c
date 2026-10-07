/**
 * @file    screen_menu.c
 * @author  IAmWack
 * @brief   Меню: список разделов.
 *
 * Одним уровнем ниже главного экрана-циферблата (screen_home.c) —
 * сюда попадают с него по любому нажатию. Последний пункт списка
 * ("Назад") — тот же приём, что и в конце любого другого списка (см.
 * screen_settings.c, screen_files.c): жеста кнопкой BOOT под "назад" не
 * выделено (см. boot_button.c), выход — через выбор пункта списком.
 * Маскот живёт только на главном экране (screen_home.c) — тут ему не
 * место, чтобы не отвлекать от списка. Реализует контракт ui_screen_t
 * (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "power.h"

#include <string.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef struct {
    const char *title;
    ui_icon_t   icon;
    screen_id_t target;
    void      (*action)(void); /* если задано — вызвать вместо перехода (см. "Выключить") */
} entry_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

/*
 * Последний пункт — SCREEN_COUNT как метка "не экран, а назад" (см.
 * input()). Остальные "инструменты" мультитула (таймер и т.п.)
 * дописывать сюда по мере появления.
 */
static const entry_t ENTRIES[] = {
    { "Файлы",     UI_ICON_BOW,   SCREEN_FILES,    NULL },
    { "Настройки", UI_ICON_GEAR,  SCREEN_SETTINGS, NULL },
    { "Пульт",     UI_ICON_SPARK, SCREEN_REMOTE,   NULL },
    { "BLE рядом", UI_ICON_SPARK, SCREEN_BLE_SCAN, NULL },
    { "О системе", UI_ICON_HEART, SCREEN_ABOUT,    NULL },
    { "Выключить", UI_ICON_PLUG,  SCREEN_COUNT,    power_sleep },
    { "Назад",     UI_ICON_PLUG,  SCREEN_COUNT,    NULL },
};
#define ENTRY_COUNT (sizeof(ENTRIES) / sizeof(ENTRIES[0]))

static ui_list_t s_list;

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Инициализировать список пунктов меню. */
static void enter(void)
{
    ui_list_init(&s_list, ENTRY_COUNT, ENTRY_COUNT, 34);
}

/**
 * @brief   Заполнить одну строку списка данными пункта меню.
 * @param   index  Индекс пункта в ENTRIES.
 * @param   out    Строка списка для заполнения (title, icon).
 */
static void row(size_t index, ui_row_t *out)
{
    strncpy(out->title, ENTRIES[index].title, sizeof(out->title) - 1);
    out->icon = ENTRIES[index].icon;
}

/**
 * @brief   Отрисовать список разделов.
 * @param   t  Текущее время кадра (мс).
 */
static void draw(uint32_t t)
{
    ui_list_draw(&s_list, CONTENT_Y, row, t);
}

/**
 * @brief   Обработать ввод: прокрутка списка и переход в выбранный раздел.
 * @param   in  Событие энкодера.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;
        case UI_IN_PRESS: {
            const entry_t *e = &ENTRIES[s_list.selected];
            if (e->action) {
                e->action();
            } else if (e->target == SCREEN_COUNT) {
                ui_back();
            } else {
                ui_goto(e->target);
            }
            break;
        }
        case UI_IN_BACK: ui_back(); break;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица главного меню для роутера ui.c. */
const ui_screen_t screen_menu = {
    .title = "Меню",
    .enter = enter,
    .draw = draw,
    .input = input,
};
