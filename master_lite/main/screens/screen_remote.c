/**
 * @file    screen_remote.c
 * @author  IAmWack
 * @brief   Управление телефоном/ноутбуком по BLE HID: точка входа пульта.
 *
 * Плата сопрягается как обычная Bluetooth-клавиатура+мышь (см.
 * ble_hid_device.c, там же составной report map). Тут — только
 * медиаклавиши (play/pause, next/prev, громкость) однократным нажатием;
 * "Клавиатура"/"Мышь" ведут на отдельные экраны с более общим набором
 * действий (screen_remote_keyboard.c, screen_remote_mouse.c) — в одном
 * списке всё сразу не поместилось бы. "Сопряжение" открывает окно
 * рекламы на 60 с. "Назад" — последний пункт списка (см. конвенцию в
 * screen_settings.c):
 * жеста кнопкой BOOT под "назад" не выделено (см. boot_button.c).
 * Реализует контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "ble_hid_device.h"

#include <string.h>
#include <stdio.h>

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    REMOTE_PAIR = 0,
    REMOTE_KEYBOARD,  /* переход на screen_remote_keyboard.c */
    REMOTE_MOUSE,     /* переход на screen_remote_mouse.c */
    REMOTE_PLAY_PAUSE,
    REMOTE_NEXT,
    REMOTE_PREV,
    REMOTE_VOL_UP,
    REMOTE_VOL_DOWN,
    REMOTE_BACK,
    REMOTE_COUNT,
} remote_item_t;

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
    ui_list_init(&s_list, REMOTE_COUNT, REMOTE_COUNT, 30);
}

/**
 * @brief   Заполнить строку списка.
 * @param   index  Индекс пункта (remote_item_t).
 * @param   out    Строка списка для заполнения.
 */
static void row(size_t index, ui_row_t *out)
{
    out->value[0] = '\0';
    switch ((remote_item_t)index) {
        case REMOTE_PAIR:
            strncpy(out->title, "Сопряжение", sizeof(out->title) - 1);
            out->icon = UI_ICON_SPARK;
            break;
        case REMOTE_KEYBOARD:
            strncpy(out->title, "Клавиатура", sizeof(out->title) - 1);
            out->icon = UI_ICON_GEAR;
            break;
        case REMOTE_MOUSE:
            strncpy(out->title, "Мышь", sizeof(out->title) - 1);
            out->icon = UI_ICON_GEAR;
            break;
        case REMOTE_PLAY_PAUSE:
            strncpy(out->title, "Пауза/плей", sizeof(out->title) - 1);
            out->icon = UI_ICON_HEART;
            break;
        case REMOTE_NEXT:
            strncpy(out->title, "Дальше", sizeof(out->title) - 1);
            out->icon = UI_ICON_BOW;
            break;
        case REMOTE_PREV:
            strncpy(out->title, "Назад (трек)", sizeof(out->title) - 1);
            out->icon = UI_ICON_BOW;
            break;
        case REMOTE_VOL_UP:
            strncpy(out->title, "Громче", sizeof(out->title) - 1);
            out->icon = UI_ICON_SPARK;
            break;
        case REMOTE_VOL_DOWN:
            strncpy(out->title, "Тише", sizeof(out->title) - 1);
            out->icon = UI_ICON_SPARK;
            break;
        default: /* REMOTE_BACK */
            strncpy(out->title, "Назад", sizeof(out->title) - 1);
            out->icon = UI_ICON_PLUG;
            break;
    }
}

/**
 * @brief   Отрисовать статус подключения и список действий.
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
 * @brief   Обработать ввод: прокрутка и выбор действия.
 * @param   in  Событие ввода.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;

        case UI_IN_PRESS:
            switch ((remote_item_t)s_list.selected) {
                case REMOTE_PAIR:
                    ble_hid_device_start_pairing(60000);
                    ui_toast("сопряжение: 60 сек");
                    break;
                case REMOTE_KEYBOARD:
                    ui_goto(SCREEN_REMOTE_KEYBOARD);
                    break;
                case REMOTE_MOUSE:
                    ui_goto(SCREEN_REMOTE_MOUSE);
                    break;
                case REMOTE_PLAY_PAUSE:
                    ble_hid_device_tap_media(BLE_HID_MEDIA_PLAY_PAUSE);
                    break;
                case REMOTE_NEXT:
                    ble_hid_device_tap_media(BLE_HID_MEDIA_NEXT);
                    break;
                case REMOTE_PREV:
                    ble_hid_device_tap_media(BLE_HID_MEDIA_PREV);
                    break;
                case REMOTE_VOL_UP:
                    ble_hid_device_tap_media(BLE_HID_MEDIA_VOL_UP);
                    break;
                case REMOTE_VOL_DOWN:
                    ble_hid_device_tap_media(BLE_HID_MEDIA_VOL_DOWN);
                    break;
                case REMOTE_BACK:
                    ui_back();
                    break;
                default: /* REMOTE_COUNT — не настоящий пункт, сюда не попадаем */
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

/** @brief  Таблица экрана пульта для роутера ui.c. */
const ui_screen_t screen_remote = {
    .title = "Пульт",
    .enter = enter,
    .draw = draw,
    .input = input,
};
