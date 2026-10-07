/**
 * @file    ble_hid_device.c
 * @author  IAmWack
 * @brief   Составное BLE HID-устройство master_lite (клавиатура + мышь + медиаклавиши).
 *
 * Реализация построена поверх компонента esp_hid (BLE HOGP через
 * NimBLE): один HID report map с тремя Report ID и GAP/advertising-код
 * адаптированы из штатного примера ESP-IDF
 * examples/bluetooth/esp_hid_device (SPDX Unlicense/CC0), урезанного
 * до единственной нужной нам роли — BLE-периферия без сканирования
 * чужих HID-устройств. Идентично slave/main/ble_hid_device.c, кроме
 * device_name ниже — там он был "Wack-OS Slave", тут просто "Wack-OS"
 * (master_lite автономен, это не slave-роль).
 *
 * Пейринг: Just Works (без ввода пинкода — у платы нет для этого ни
 * отдельного экрана ввода), бондинг включён, бонды хранит NimBLE в NVS
 * (ble_store_config_init()) — так что после первого сопряжения
 * подключение восстанавливается само.
 */
#include "ble_hid_device.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_bt.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "esp_hidd.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define TAG "ble_hid_device"

#define GATT_SVC_HID_UUID16   0x1812

/* Задержка между "нажатием" и автоотпусканием для tap-действий
 * (media-клавиши). Достаточно для распознавания хостом одного нажатия. */
#define TAP_HOLD_MS 20

/* Report ID's составного report map — должны совпадать с 0x85 в
 * REPORT_MAP ниже и с id, который передаётся в esp_hidd_dev_input_set(). */
#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE    2
#define REPORT_ID_MEDIA    3

/* ============================================================================
 *  Types
 * ============================================================================ */

/* Отчёт клавиатуры: модификаторы, зарезервированный байт, до 5 клавиш
 * одновременно (без n-key rollover — этого достаточно для tap/hold). */
typedef struct __attribute__((packed)) {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[5];
} kbd_report_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

/*
 * Составной HID report map: клавиатура (Report ID 1) + мышь (Report ID
 * 2) + медиаклавиши/consumer control (Report ID 3) одним report map —
 * ровно так собираются композитные USB/BLE HID-устройства.
 */
static const uint8_t REPORT_MAP[] = {
    /* --- Клавиатура, Report ID 1 --- */
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_KEYBOARD,
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,        //   Usage Minimum (0xE0, LeftControl)
    0x29, 0xE7,        //   Usage Maximum (0xE7, RightGUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8) -- 8 битов модификаторов
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8) -- зарезервированный байт
    0x81, 0x03,        //   Input (Const,Var,Abs)
    0x95, 0x05,        //   Report Count (5) -- 5 клавиш
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data,Array,Abs)
    0xC0,              // End Collection

    /* --- Мышь, Report ID 2 --- */
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x85, REPORT_ID_MOUSE,
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (Button 1)
    0x29, 0x03,        //     Usage Maximum (Button 3)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3) -- left/right/middle
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5) -- padding
    0x81, 0x03,        //     Input (Const,Var,Abs)
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3) -- X, Y, Wheel
    0x81, 0x06,        //     Input (Data,Var,Rel)
    0xC0,              //   End Collection
    0xC0,              // End Collection

    /* --- Медиаклавиши (consumer control), Report ID 3 --- */
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_MEDIA,
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x07,        //   Report Count (7) -- 7 кнопок-битов, порядок = BLE_HID_MEDIA_*
    0x09, 0xB5,        //   Usage (Scan Next Track)
    0x09, 0xB6,        //   Usage (Scan Previous Track)
    0x09, 0xB7,        //   Usage (Stop)
    0x09, 0xCD,        //   Usage (Play/Pause)
    0x09, 0xE2,        //   Usage (Mute)
    0x09, 0xE9,        //   Usage (Volume Increment)
    0x09, 0xEA,        //   Usage (Volume Decrement)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x95, 0x01,        //   Report Count (1) -- padding до целого байта
    0x81, 0x01,        //   Input (Const,Array,Abs)
    0xC0,              // End Collection
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    { .data = REPORT_MAP, .len = sizeof(REPORT_MAP) },
};

static esp_hid_device_config_t s_hid_config = {
    .vendor_id         = 0x16C0,
    .product_id        = 0x05DF,
    .version           = 0x0100,
    .device_name       = "Wack-OS",
    .manufacturer_name = "Wack-OS",
    .serial_number     = "1",
    .report_maps       = s_report_maps,
    .report_maps_len   = 1,
};

static esp_hidd_dev_t *s_hid_dev = NULL;
static volatile bool s_connected = false;
static struct ble_hs_adv_fields s_adv_fields;
static kbd_report_t s_kbd_report;

/* Окно сопряжения: реклама включена только пока s_pairing_open, гасит
 * себя сама по таймауту s_pairing_timer — см. ble_hid_device_start_pairing(). */
static volatile bool s_pairing_open = false;
static esp_timer_handle_t s_pairing_timer = NULL;

/* ============================================================================
 *  Static helpers — advertising (GAP)
 * ============================================================================ */

static int ble_hid_device_gap_event(struct ble_gap_event *event, void *arg);

/**
 * @brief   Начать/перезапустить BLE-рекламу.
 *
 * Реклама ограничена стеком 180 с (максимум для HID-устройства) — по
 * истечении срока и при обрыве связи она перезапускается сама, см.
 * nimble_hid_gap_event()/hidd_event_handler() ниже.
 */
static void start_advertising(void)
{
    int rc = ble_gap_adv_set_fields(&s_adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min  = BLE_GAP_ADV_ITVL_MS(30),
        .itvl_max  = BLE_GAP_ADV_ITVL_MS(50),
    };
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, 180000,
                            &adv_params, ble_hid_device_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start: %d", rc);
    }
}

/**
 * @brief   Обработчик GAP-событий NimBLE (подключение/реклама/пейринг).
 */
static int ble_hid_device_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connect status=%d", event->connect.status);
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect reason=%d", event->disconnect.reason);
        if (s_pairing_open) {
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        /* Истёк 180-секундный внутренний лимит NimBLE на одну рекламную
         * сессию — если окно сопряжения ещё не кончилось, продолжаем. */
        if (s_pairing_open) {
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* Хост уже был забондирован, но пробует спариться заново —
         * стираем старый бонд и разрешаем повторный пейринг, иначе
         * переустановка связи после сброса NVS/смены устройства
         * зависает. */
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    default:
        return 0;
    }
}

/* ============================================================================
 *  Static helpers — HID device events
 * ============================================================================ */

static void hidd_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg;
    (void)base;
    switch ((esp_hidd_event_t)id) {
    case ESP_HIDD_START_EVENT:
        /* Рекламу намеренно не включаем здесь — только по
         * ble_hid_device_start_pairing(), см. заголовок файла. */
        ESP_LOGI(TAG, "HID device stack ready");
        break;
    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "connected");
        s_connected = true;
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "disconnected");
        s_connected = false;
        break;
    default:
        break;
    }
}

/* ============================================================================
 *  Static helpers — init
 * ============================================================================ */

/**
 * @brief   Поднять контроллер и NimBLE-порт в режиме "только BLE".
 */
static esp_err_t init_controller(void)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_nimble_init();
}

/**
 * @brief   Заполнить поля рекламы (имя, appearance, UUID сервиса HID).
 */
static void init_adv_fields(void)
{
    static ble_uuid16_t hid_uuid = BLE_UUID16_INIT(GATT_SVC_HID_UUID16);

    memset(&s_adv_fields, 0, sizeof(s_adv_fields));
    s_adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    s_adv_fields.appearance = ESP_HID_APPEARANCE_GENERIC;
    s_adv_fields.appearance_is_present = 1;
    s_adv_fields.tx_pwr_lvl_is_present = 1;
    s_adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    s_adv_fields.name = (const uint8_t *)s_hid_config.device_name;
    s_adv_fields.name_len = strlen(s_hid_config.device_name);
    s_adv_fields.name_is_complete = 1;
    s_adv_fields.uuids16 = &hid_uuid;
    s_adv_fields.num_uuids16 = 1;
    s_adv_fields.uuids16_is_complete = 1;

    /* Just Works + bonding: у платы нет отдельного экрана ввода
     * пинкода, поэтому MITM осознанно выключен. */
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
}

/* NimBLE-хост живёт в своей задаче, пока не позовут nimble_port_stop(). */
static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* Бонды хранятся в NVS этим готовым модулем NimBLE (bt/host/nimble/.../store/config) */
void ble_store_config_init(void);

/**
 * @brief   Колбэк таймера окна сопряжения — закрыть окно и выключить рекламу.
 */
static void pairing_timer_cb(void *arg)
{
    (void)arg;
    s_pairing_open = false;
    ble_gap_adv_stop(); /* не страшно, если реклама уже не шла (например, есть соединение) */
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

void ble_hid_device_init(void)
{
    ESP_ERROR_CHECK(init_controller());
    init_adv_fields();

    ESP_ERROR_CHECK(esp_hidd_dev_init(&s_hid_config, ESP_HID_TRANSPORT_BLE,
                                       hidd_event_handler, &s_hid_dev));

    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    const esp_timer_create_args_t timer_args = {
        .callback = pairing_timer_cb,
        .name = "ble_pair_win",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_pairing_timer));

    nimble_port_freertos_init(nimble_host_task);
}

bool ble_hid_device_is_connected(void)
{
    return s_connected;
}

void ble_hid_device_start_pairing(uint32_t window_ms)
{
    s_pairing_open = true;
    start_advertising();
    esp_timer_stop(s_pairing_timer); /* не ошибка, если уже не был запущен */
    esp_timer_start_once(s_pairing_timer, (uint64_t)window_ms * 1000ULL);
}

void ble_hid_device_key_down(uint8_t modifiers, uint8_t keycode)
{
    memset(&s_kbd_report, 0, sizeof(s_kbd_report));
    s_kbd_report.modifiers = modifiers;
    s_kbd_report.keys[0] = keycode;
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_KEYBOARD,
                            (uint8_t *)&s_kbd_report, sizeof(s_kbd_report));
}

void ble_hid_device_key_up(void)
{
    memset(&s_kbd_report, 0, sizeof(s_kbd_report));
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_KEYBOARD,
                            (uint8_t *)&s_kbd_report, sizeof(s_kbd_report));
}

void ble_hid_device_send_mouse(int8_t dx, int8_t dy, int8_t wheel, uint8_t buttons)
{
    uint8_t report[4] = { buttons, (uint8_t)dx, (uint8_t)dy, (uint8_t)wheel };
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_MOUSE, report, sizeof(report));
}

void ble_hid_device_tap_media(uint8_t media_bit)
{
    uint8_t report = media_bit;
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_MEDIA, &report, sizeof(report));
    vTaskDelay(pdMS_TO_TICKS(TAP_HOLD_MS));
    report = 0;
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_MEDIA, &report, sizeof(report));
}

void ble_hid_device_tap_key(uint8_t modifiers, uint8_t keycode)
{
    ble_hid_device_key_down(modifiers, keycode);
    vTaskDelay(pdMS_TO_TICKS(TAP_HOLD_MS));
    ble_hid_device_key_up();
}

void ble_hid_device_tap_mouse_button(uint8_t button_bit)
{
    ble_hid_device_send_mouse(0, 0, 0, button_bit);
    vTaskDelay(pdMS_TO_TICKS(TAP_HOLD_MS));
    ble_hid_device_send_mouse(0, 0, 0, 0);
}
