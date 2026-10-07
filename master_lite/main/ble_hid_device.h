/**
 * @file    ble_hid_device.h
 * @author  IAmWack
 * @brief   Составное BLE HID-устройство master_lite: клавиатура + мышь + медиаклавиши.
 *
 * Один HID-профиль (BLE HOGP через компонент esp_hid поверх NimBLE) с
 * тремя отчётами в общем report map — Report ID 1 (клавиатура),
 * 2 (мышь), 3 (медиаклавиши/consumer control). Именно так плата
 * "управляет" телефоном/ноутбуком: сопрягается с ними как обычная
 * Bluetooth-клавиатура+мышь.
 *
 * Изначально был идентичен slave/main/ble_hid_device.c (аппаратно-
 * независимая реализация, никакой зависимости от wackos_protocol/slave);
 * после добавления tap_key()/tap_mouse_button() тут — больше не 1:1,
 * проверяйте перед переносом правок в обе стороны.
 *
 * Пейринг — Just Works с бондингом (без ввода пинкода: у платы нет для
 * этого отдельного экрана ввода), бонды сохраняются в NVS, так что
 * после переподключения повторный пейринг не нужен.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/* Биты консьюмер-репорта (Report ID 3) — см. ble_hid_device_tap_media(). */
#define BLE_HID_MEDIA_NEXT       (1 << 0)
#define BLE_HID_MEDIA_PREV       (1 << 1)
#define BLE_HID_MEDIA_STOP       (1 << 2)
#define BLE_HID_MEDIA_PLAY_PAUSE (1 << 3)
#define BLE_HID_MEDIA_MUTE       (1 << 4)
#define BLE_HID_MEDIA_VOL_UP     (1 << 5)
#define BLE_HID_MEDIA_VOL_DOWN   (1 << 6)

/* Биты кнопок мыши (Report ID 2) — см. ble_hid_device_send_mouse(). */
#define BLE_HID_MOUSE_LEFT   (1 << 0)
#define BLE_HID_MOUSE_RIGHT  (1 << 1)
#define BLE_HID_MOUSE_MIDDLE (1 << 2)

/*
 * HID usage-коды клавиш (USB HID Usage Tables, раздел "Keyboard/Keypad") —
 * только то, что реально нужно пульту (см. screens/screen_remote_keyboard.c).
 * Полная таблица занимает сотню строк, тут не нужна.
 */
#define BLE_HID_KEY_ENTER     0x28
#define BLE_HID_KEY_ESC       0x29
#define BLE_HID_KEY_BACKSPACE 0x2A
#define BLE_HID_KEY_TAB       0x2B
#define BLE_HID_KEY_SPACE     0x2C
#define BLE_HID_KEY_RIGHT     0x4F
#define BLE_HID_KEY_LEFT      0x50
#define BLE_HID_KEY_DOWN      0x51
#define BLE_HID_KEY_UP        0x52

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Поднять BLE-стек (NimBLE) и HID-профиль. Рекламу не включает.
 *
 * Требует, чтобы nvs_flash_init() уже был вызван (бонды и калибровка
 * радио хранятся в NVS) — см. main.c. Реклама (advertising) специально
 * НЕ стартует сама: без неё плата невидима для чужих BLE-сканеров, пока
 * кто-то явно не попросит ble_hid_device_start_pairing().
 */
void ble_hid_device_init(void);

/**
 * @brief   Подключён ли сейчас BLE-центр (ноутбук/телефон)?
 */
bool ble_hid_device_is_connected(void);

/**
 * @brief   Открыть окно сопряжения — включить рекламу на ограниченное время.
 * @param   window_ms  Сколько миллисекунд быть видимым/подключаемым.
 *
 * Реклама сама выключается по истечении окна (даже если за это время
 * подключение обрывалось и переустанавливалось несколько раз) — вызов
 * только продлевает предыдущее окно, если оно уже открыто.
 */
void ble_hid_device_start_pairing(uint32_t window_ms);

/**
 * @brief   Нажать клавишу клавиатуры (репорт остаётся "нажатым" до key_up()).
 * @param   modifiers  Битовая маска модификаторов (Ctrl/Shift/Alt/GUI, см. USB HID usage table).
 * @param   keycode    HID usage-код клавиши (0 = отпущено).
 */
void ble_hid_device_key_down(uint8_t modifiers, uint8_t keycode);

/**
 * @brief   Отпустить все клавиши (обнулить отчёт клавиатуры).
 */
void ble_hid_device_key_up(void);

/**
 * @brief   Отправить один относительный отчёт мыши.
 * @param   dx       Смещение по X, -127..127.
 * @param   dy       Смещение по Y, -127..127.
 * @param   wheel    Прокрутка колеса, -127..127.
 * @param   buttons  Битовая маска BLE_HID_MOUSE_* — состояние кнопок в этом отчёте.
 */
void ble_hid_device_send_mouse(int8_t dx, int8_t dy, int8_t wheel, uint8_t buttons);

/**
 * @brief   Кратко "нажать-отпустить" одну медиаклавишу (play/pause, громкость и т.п.).
 * @param   media_bit  Один из BLE_HID_MEDIA_*.
 */
void ble_hid_device_tap_media(uint8_t media_bit);

/**
 * @brief   Кратко "нажать-отпустить" одну клавишу клавиатуры (см. BLE_HID_KEY_*).
 * @param   modifiers  Битовая маска модификаторов (0, если не нужны).
 * @param   keycode    HID usage-код клавиши.
 */
void ble_hid_device_tap_key(uint8_t modifiers, uint8_t keycode);

/**
 * @brief   Кратко "кликнуть-отпустить" одну кнопку мыши (см. BLE_HID_MOUSE_*),
 *          без смещения курсора.
 * @param   button_bit  Один из BLE_HID_MOUSE_*.
 */
void ble_hid_device_tap_mouse_button(uint8_t button_bit);
