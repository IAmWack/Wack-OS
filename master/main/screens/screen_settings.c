/**
 * @file    screen_settings.c
 * @author  IAmWack
 * @brief   Экран настроек: яркость, громкость, тема, Астольфо вкл/выкл.
 *
 * Нажатие входит в режим правки (крутилка меняет значение, а не выбор
 * пункта) и выходит из него — для одной ручки без кнопок это самый
 * понятный способ. Реализует контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "settings.h"
#include "sound.h"
#include "astolfo.h"
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
    SET_ASTOLFO,
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
        default:
            strncpy(out->title, "Астольфо", sizeof(out->title) - 1);
            snprintf(out->value, sizeof(out->value), "%s",
                     settings_astolfo() ? "вкл" : "выкл");
            out->icon = UI_ICON_HEART;
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
            /* На максимуме Астольфо жмурится — мелочь, а приятно. */
            if (settings_brightness() >= 100) {
                astolfo_set_mood(ASTOLFO_HAPPY);
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
        default:
            settings_set_astolfo(!settings_astolfo());
            break;
    }
}

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/**
 * @brief   Отрисовать список настроек и, если включено, Астольфо.
 * @param   t  Текущее время кадра (мс), для анимации персонажа.
 */
static void draw(uint32_t t)
{
    ui_list_draw(&s_list, CONTENT_Y, row, t);

    if (settings_astolfo()) {
        astolfo_draw(140, 200, 24, t);
    }
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
            /* Переключатель правим сразу, без режима правки. */
            if ((setting_t)s_list.selected == SET_ASTOLFO) {
                adjust(1);
            } else {
                s_list.editing = !s_list.editing;
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
    .hint = "жми: менять|держи ←",
};
