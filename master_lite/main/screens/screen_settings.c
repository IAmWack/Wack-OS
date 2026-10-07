/**
 * @file    screen_settings.c
 * @author  IAmWack
 * @brief   Экран настроек: яркость, громкость, тема, маскот, время.
 *
 * Нажатие входит в режим правки (крутилка меняет значение, а не выбор
 * пункта) и выходит из него — для одной ручки без кнопок это самый
 * понятный способ. Настройка времени — отдельный подэкран (см.
 * screen_clock.c), тут только переход на него. Реализует контракт
 * ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "settings.h"
#include "sound.h"
#include "mascot.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    SET_BRIGHTNESS = 0,
    SET_VOLUME,
    SET_THEME,
    SET_MASCOT,
    SET_READER_LANDSCAPE,  /* альбомная раскладка просмотра файлов с SD */
    SET_CLOCK,  /* переход на screen_clock.c */
    SET_BACK,
    SET_COUNT,
} setting_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

static ui_list_t s_list;

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Инициализировать список пунктов настроек. */
static void enter(void)
{
    ui_list_init(&s_list, SET_COUNT, SET_COUNT, 30);
}

/**
 * @brief   Заполнить одну строку списка названием и текущим значением
 *          настройки.
 * @param   index  Индекс настройки (setting_t).
 * @param   out    Строка списка для заполнения (title, value, icon).
 */
static void row(size_t index, ui_row_t *out)
{
    switch ((setting_t)index) {
        case SET_BRIGHTNESS:
            strncpy(out->title, "Яркость", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%u%%", settings_brightness());
            out->icon = UI_ICON_SPARK;
            break;
        case SET_VOLUME:
            strncpy(out->title, "Громкость", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%u", settings_volume());
            out->icon = UI_ICON_HEART;
            break;
        case SET_THEME:
            strncpy(out->title, "Тема", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%s", theme_name(settings_theme()));
            out->icon = UI_ICON_BOW;
            break;
        case SET_MASCOT:
            strncpy(out->title, "Маскот", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%s",
                     settings_mascot() ? "вкл" : "выкл");
            out->icon = UI_ICON_HEART;
            break;
        case SET_READER_LANDSCAPE:
            strncpy(out->title, "Чтение SD", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%s",
                     settings_reader_landscape() ? "бок" : "книжн.");
            out->icon = UI_ICON_GEAR;
            break;
        case SET_CLOCK:
            strncpy(out->title, "Время", sizeof(out->title) - 1);
            out->value[0] = '\0';
            out->icon = UI_ICON_SPARK;
            break;
        default: /* SET_BACK */
            strncpy(out->title, "Назад", sizeof(out->title) - 1);
            out->value[0] = '\0';
            out->icon = UI_ICON_PLUG;
            break;
    }
}

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/**
 * @brief   Изменить значение выбранной настройки на шаг вперёд/назад.
 * @param   delta  Направление и размер шага (+1/-1 от энкодера).
 */
static void adjust(int delta)
{
    switch ((setting_t)s_list.selected) {
        case SET_BRIGHTNESS:
            settings_set_brightness((uint8_t)(settings_brightness() + delta * 5));
            /* На максимуме маскот жмурится — мелочь, а приятно. */
            if (settings_brightness() >= 100) {
                mascot_set_mood(MASCOT_HAPPY);
            }
            break;
        case SET_VOLUME: {
            int v = settings_volume() + delta;
            if (v < 0) v = 0;
            settings_set_volume((uint8_t)v);
            sound_play(SOUND_SELECT); /* сразу слышно новую громкость */
            break;
        }
        case SET_THEME: {
            size_t n = theme_count();
            settings_set_theme((settings_theme() + (delta > 0 ? 1 : n - 1)) % n);
            break;
        }
        case SET_MASCOT:
            settings_set_mascot(!settings_mascot());
            break;
        case SET_READER_LANDSCAPE:
            settings_set_reader_landscape(!settings_reader_landscape());
            break;
        default: /* SET_CLOCK/SET_BACK — тут делать нечего, вход в правку не предусмотрен */
            break;
    }
}

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/**
 * @brief   Отрисовать список настроек.
 * @param   t  Текущее время кадра (мс).
 */
static void draw(uint32_t t)
{
    ui_list_draw(&s_list, CONTENT_Y, row, t);
}

/**
 * @brief   Обработать ввод: прокрутка/правка значения, вход и выход
 *          из режима правки.
 * @param   in  Событие энкодера.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT:
            if (s_list.editing) {
                adjust(1);
            } else {
                ui_list_move(&s_list, 1);
            }
            break;

        case UI_IN_PREV:
            if (s_list.editing) {
                adjust(-1);
            } else {
                ui_list_move(&s_list, -1);
            }
            break;

        case UI_IN_PRESS:
            switch ((setting_t)s_list.selected) {
                case SET_MASCOT:
                case SET_READER_LANDSCAPE:
                    /* Переключатель правим сразу, без режима правки —
                     * один клик, можно и сохранить сразу же. */
                    adjust(1);
                    settings_save();
                    break;
                case SET_CLOCK:
                    ui_goto(SCREEN_CLOCK);
                    break;
                case SET_BACK:
                    ui_back();
                    break;
                default:
                    s_list.editing = !s_list.editing;
                    if (!s_list.editing) {
                        /* Вышли из правки слайдера (яркость/громкость/тема) —
                         * записать итог один раз, а не на каждый тик энкодера
                         * внутри adjust() (см. комментарий в settings.c). */
                        settings_save();
                    }
                    break;
            }
            break;

        case UI_IN_BACK:
            if (s_list.editing) {
                s_list.editing = false;
            } else {
                ui_back();
            }
            break;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица экрана настроек для роутера ui.c. */
const ui_screen_t screen_settings = {
    .title = "Настройки",
    .enter = enter,
    .draw = draw,
    .input = input,
};
