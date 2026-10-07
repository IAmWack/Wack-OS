/**
 * @file    screen_remote_mouse.c
 * @author  IAmWack
 * @brief   Пульт: мышь по BLE HID — клики, скролл, сдвиг курсора шагом.
 *
 * Report ID 2 (мышь) в составном HID report map умеет относительные
 * X/Y/колесо (см. ble_hid_device.c) — но без энкодера (сейчас вместо
 * него кнопка BOOT, см. board_pins.h) аналоговое перемещение недоступно,
 * только дискретные действия: клик, скролл на одно деление, сдвиг
 * курсора фиксированным шагом в одну из 4 сторон. Когда появится
 * энкодер — можно будет добавить непрерывное перемещение отдельным
 * режимом, тут это не теряется, просто не нужно прямо сейчас. "Назад" —
 * последний пункт списка (см. конвенцию в screen_settings.c): жеста
 * кнопкой BOOT под "назад" не выделено (см. boot_button.c). Реализует
 * контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "ble_hid_device.h"

#include <string.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/* Шаг сдвига курсора по "Курсор ..." пунктам, пикселей на хосте. */
#define MOUSE_STEP 18

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    MOUSE_LEFT_CLICK = 0,
    MOUSE_RIGHT_CLICK,
    MOUSE_SCROLL_UP,
    MOUSE_SCROLL_DOWN,
    MOUSE_MOVE_UP,
    MOUSE_MOVE_DOWN,
    MOUSE_MOVE_LEFT,
    MOUSE_MOVE_RIGHT,
    MOUSE_BACK,
    MOUSE_COUNT,
} mouse_item_t;

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
    ui_list_init(&s_list, MOUSE_COUNT, MOUSE_COUNT, 30);
}

/**
 * @brief   Заполнить строку списка.
 * @param   index  Индекс пункта (mouse_item_t).
 * @param   out    Строка списка для заполнения.
 */
static void row(size_t index, ui_row_t *out)
{
    out->value[0] = '\0';
    switch ((mouse_item_t)index) {
        case MOUSE_LEFT_CLICK:   strncpy(out->title, "ЛКМ", sizeof(out->title) - 1); out->icon = UI_ICON_SPARK; break;
        case MOUSE_RIGHT_CLICK:  strncpy(out->title, "ПКМ", sizeof(out->title) - 1); out->icon = UI_ICON_SPARK; break;
        case MOUSE_SCROLL_UP:    strncpy(out->title, "Скролл вверх", sizeof(out->title) - 1); out->icon = UI_ICON_BOW; break;
        case MOUSE_SCROLL_DOWN:  strncpy(out->title, "Скролл вниз", sizeof(out->title) - 1); out->icon = UI_ICON_BOW; break;
        case MOUSE_MOVE_UP:      strncpy(out->title, "Курсор вверх", sizeof(out->title) - 1); out->icon = UI_ICON_HEART; break;
        case MOUSE_MOVE_DOWN:    strncpy(out->title, "Курсор вниз", sizeof(out->title) - 1); out->icon = UI_ICON_HEART; break;
        case MOUSE_MOVE_LEFT:    strncpy(out->title, "Курсор влево", sizeof(out->title) - 1); out->icon = UI_ICON_HEART; break;
        case MOUSE_MOVE_RIGHT:   strncpy(out->title, "Курсор вправо", sizeof(out->title) - 1); out->icon = UI_ICON_HEART; break;
        default: /* MOUSE_BACK */
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
 * @brief   Обработать ввод: прокрутка и выбранное действие мыши.
 * @param   in  Событие ввода.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;

        case UI_IN_PRESS:
            switch ((mouse_item_t)s_list.selected) {
                case MOUSE_LEFT_CLICK:  ble_hid_device_tap_mouse_button(BLE_HID_MOUSE_LEFT); break;
                case MOUSE_RIGHT_CLICK: ble_hid_device_tap_mouse_button(BLE_HID_MOUSE_RIGHT); break;
                case MOUSE_SCROLL_UP:   ble_hid_device_send_mouse(0, 0, 1, 0); break;
                case MOUSE_SCROLL_DOWN: ble_hid_device_send_mouse(0, 0, -1, 0); break;
                case MOUSE_MOVE_UP:     ble_hid_device_send_mouse(0, -MOUSE_STEP, 0, 0); break;
                case MOUSE_MOVE_DOWN:   ble_hid_device_send_mouse(0, MOUSE_STEP, 0, 0); break;
                case MOUSE_MOVE_LEFT:   ble_hid_device_send_mouse(-MOUSE_STEP, 0, 0, 0); break;
                case MOUSE_MOVE_RIGHT:  ble_hid_device_send_mouse(MOUSE_STEP, 0, 0, 0); break;
                case MOUSE_BACK:        ui_back(); break;
                default: /* MOUSE_COUNT — не настоящий пункт, сюда не попадаем */
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

/** @brief  Таблица экрана мыши-пульта для роутера ui.c. */
const ui_screen_t screen_remote_mouse = {
    .title = "Мышь",
    .enter = enter,
    .draw = draw,
    .input = input,
};
