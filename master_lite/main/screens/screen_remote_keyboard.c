/**
 * @file    screen_remote_keyboard.c
 * @author  IAmWack
 * @brief   Пульт: клавиши клавиатуры по BLE HID (навигация/презентация).
 *
 * Report ID 1 (клавиатура) в составном HID report map уже умеет куда
 * больше медиаклавиш (см. ble_hid_device.c) — тут выбран небольшой
 * набор, который реально полезен с управлением в одну кнопку: стрелки
 * (листать слайды/видео), Enter/Esc/Tab/Backspace/Space для базовой
 * навигации. "Назад" — последний пункт списка (см. конвенцию в
 * screen_settings.c): жеста кнопкой BOOT под "назад" не выделено (см.
 * boot_button.c). Реализует контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "ble_hid_device.h"

#include <string.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    KEY_LEFT = 0,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_ENTER,
    KEY_ESC,
    KEY_SPACE,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_BACK,
    KEY_COUNT,
} key_item_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

static ui_list_t s_list;

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Инициализировать список пунктов. */
static void enter(void)
{
    ui_list_init(&s_list, KEY_COUNT, KEY_COUNT, 30);
}

/**
 * @brief   Заполнить строку списка.
 * @param   index  Индекс пункта (key_item_t).
 * @param   out    Строка списка для заполнения.
 */
static void row(size_t index, ui_row_t *out)
{
    out->value[0] = '\0';
    switch ((key_item_t)index) {
        case KEY_LEFT:      strncpy(out->title, "<- (слайд назад)", sizeof(out->title) - 1); out->icon = UI_ICON_BOW; break;
        case KEY_RIGHT:     strncpy(out->title, "-> (слайд вперёд)", sizeof(out->title) - 1); out->icon = UI_ICON_BOW; break;
        case KEY_UP:        strncpy(out->title, "Вверх", sizeof(out->title) - 1); out->icon = UI_ICON_BOW; break;
        case KEY_DOWN:      strncpy(out->title, "Вниз", sizeof(out->title) - 1); out->icon = UI_ICON_BOW; break;
        case KEY_ENTER:     strncpy(out->title, "Enter", sizeof(out->title) - 1); out->icon = UI_ICON_SPARK; break;
        case KEY_ESC:       strncpy(out->title, "Esc", sizeof(out->title) - 1); out->icon = UI_ICON_SPARK; break;
        case KEY_SPACE:     strncpy(out->title, "Пробел", sizeof(out->title) - 1); out->icon = UI_ICON_SPARK; break;
        case KEY_TAB:       strncpy(out->title, "Tab", sizeof(out->title) - 1); out->icon = UI_ICON_SPARK; break;
        case KEY_BACKSPACE: strncpy(out->title, "Backspace", sizeof(out->title) - 1); out->icon = UI_ICON_SPARK; break;
        default: /* KEY_BACK */
            strncpy(out->title, "Назад", sizeof(out->title) - 1);
            out->icon = UI_ICON_PLUG;
            break;
    }
}

/**
 * @brief   Отрисовать статус подключения и список клавиш.
 * @param   t  Текущее время кадра (мс).
 */
static void draw(uint32_t t)
{
    const char *status = ble_hid_device_is_connected() ? "подключено" : "не подключено";
    uint16_t color = ble_hid_device_is_connected() ? TH->ok : TH->alert;
    gfx_text_center(&wackos_font_small, GFX_W / 2, CONTENT_Y - 2, status, color, 230);

    ui_list_draw(&s_list, CONTENT_Y + 16, row, t);
}

/**
 * @brief   Обработать ввод: прокрутка и "нажатие" выбранной клавиши.
 * @param   in  Событие ввода.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;

        case UI_IN_PRESS:
            switch ((key_item_t)s_list.selected) {
                case KEY_LEFT:      ble_hid_device_tap_key(0, BLE_HID_KEY_LEFT); break;
                case KEY_RIGHT:     ble_hid_device_tap_key(0, BLE_HID_KEY_RIGHT); break;
                case KEY_UP:        ble_hid_device_tap_key(0, BLE_HID_KEY_UP); break;
                case KEY_DOWN:      ble_hid_device_tap_key(0, BLE_HID_KEY_DOWN); break;
                case KEY_ENTER:     ble_hid_device_tap_key(0, BLE_HID_KEY_ENTER); break;
                case KEY_ESC:       ble_hid_device_tap_key(0, BLE_HID_KEY_ESC); break;
                case KEY_SPACE:     ble_hid_device_tap_key(0, BLE_HID_KEY_SPACE); break;
                case KEY_TAB:       ble_hid_device_tap_key(0, BLE_HID_KEY_TAB); break;
                case KEY_BACKSPACE: ble_hid_device_tap_key(0, BLE_HID_KEY_BACKSPACE); break;
                case KEY_BACK:      ui_back(); break;
                default: /* KEY_COUNT — не настоящий пункт, сюда не попадаем */
                    break;
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

/** @brief  Таблица экрана клавиатуры-пульта для роутера ui.c. */
const ui_screen_t screen_remote_keyboard = {
    .title = "Клавиатура",
    .enter = enter,
    .draw = draw,
    .input = input,
};
