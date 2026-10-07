/**
 * @file    screen_ble_scan.c
 * @author  IAmWack
 * @brief   "Радар": пассивный скан BLE-рекламы вокруг — имя (если
 *          рекламируется), адрес, уровень сигнала.
 *
 * Чисто наблюдение: слушает чужую рекламу (NimBLE observer-роль, см.
 * sdkconfig CONFIG_BT_NIMBLE_ROLE_OBSERVER=y) и ни к чему не
 * подключается — отдельная GAP-процедура от HID-периферии (см.
 * ble_hid_device.c), конфликта нет, обе роли живут параллельно. Скан
 * идёт только пока этот экран открыт: enter() запускает, exit() (см.
 * ui_screen_t, зовётся из ui.c при уходе с экрана) останавливает —
 * иначе радио крутилось бы вхолостую и после выхода.
 *
 * Список устройств пишется из колбэка NimBLE (своя задача), а читается
 * из отрисовки (главная задача) — отсюда мьютекс: без него можно
 * поймать строку имени на половине memcpy.
 *
 * "Назад" — последний пункт списка (см. конвенцию в screen_settings.c):
 * жеста кнопкой BOOT под "назад" не выделено (см. boot_button.c).
 * Реализует контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "nimble/ble.h"

#include <string.h>
#include <stdio.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define MAX_SCAN_DEVICES 16
#define NAME_LEN         20

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef struct {
    ble_addr_t addr;
    char       name[NAME_LEN]; /* пусто, если устройство имя не рекламирует */
    int8_t     rssi;
} scan_entry_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

static scan_entry_t s_entries[MAX_SCAN_DEVICES];
static size_t s_count;
static SemaphoreHandle_t s_mutex;
static bool s_scanning;

static ui_list_t s_list;

/* ============================================================================
 *  Static helpers — NimBLE
 * ============================================================================ */

/**
 * @brief   Колбэк GAP-дискавери: новое объявление — найти по адресу
 *          или завести новую запись, обновить имя/RSSI.
 */
static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type != BLE_GAP_EVENT_DISC) {
        return 0;
    }

    struct ble_hs_adv_fields fields;
    ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    size_t idx = s_count;
    for (size_t i = 0; i < s_count; i++) {
        if (memcmp(&s_entries[i].addr, &event->disc.addr, sizeof(ble_addr_t)) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == s_count && s_count < MAX_SCAN_DEVICES) {
        s_count++;
    }
    if (idx < MAX_SCAN_DEVICES) {
        scan_entry_t *e = &s_entries[idx];
        e->addr = event->disc.addr;
        e->rssi = event->disc.rssi;
        if (fields.name_len > 0) {
            size_t n = fields.name_len < sizeof(e->name) - 1 ? fields.name_len : sizeof(e->name) - 1;
            memcpy(e->name, fields.name, n);
            e->name[n] = '\0';
        }
    }

    xSemaphoreGive(s_mutex);
    return 0;
}

/** @brief  Запустить непрерывный пассивный скан (пока не остановят явно). */
static void start_scan(void)
{
    struct ble_gap_disc_params params = {
        .itvl = 0x0050,  /* 50 мс */
        .window = 0x0030, /* 30 мс слушаем из каждого интервала */
        .passive = 1,     /* не шлём scan request — только слушаем рекламу */
    };
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, gap_event, NULL);
    s_scanning = (rc == 0);
}

/** @brief  Остановить скан (не страшно, если уже не шёл). */
static void stop_scan(void)
{
    if (s_scanning) {
        ble_gap_disc_cancel();
        s_scanning = false;
    }
}

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Сбросить список и запустить скан заново. */
static void enter(void)
{
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
    }
    s_count = 0;
    ui_list_init(&s_list, 1, 5, 28); /* +1 — "Назад", count обновится в draw() */
    start_scan();
}

/** @brief  Остановить скан при уходе с экрана (см. ui_screen_t.exit). */
static void leave(void)
{
    stop_scan();
}

/**
 * @brief   Заполнить строку списка найденным устройством либо (для
 *          последнего индекса) пунктом "Назад".
 */
static void row(size_t index, ui_row_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (index >= s_count) {
        strncpy(out->title, "Назад", sizeof(out->title) - 1);
        out->value[0] = '\0';
        out->icon = UI_ICON_PLUG;
    } else {
        const scan_entry_t *e = &s_entries[index];
        if (e->name[0]) {
            strncpy(out->title, e->name, sizeof(out->title) - 1);
        } else {
            snprintf(out->title, sizeof(out->title), "%02x:%02x:%02x:%02x:%02x:%02x",
                     e->addr.val[5], e->addr.val[4], e->addr.val[3],
                     e->addr.val[2], e->addr.val[1], e->addr.val[0]);
        }
        snprintf(out->value, sizeof(out->value), "%ddБ", e->rssi);
        out->icon = UI_ICON_SPARK;
    }
    out->title[sizeof(out->title) - 1] = '\0';

    xSemaphoreGive(s_mutex);
}

/**
 * @brief   Отрисовать статус скана и список найденных устройств.
 * @param   t  Текущее время кадра (мс).
 */
static void draw(uint32_t t)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_list.count = s_count + 1; /* +1 — "Назад" */
    size_t count = s_count;
    xSemaphoreGive(s_mutex);

    char status[64];
    snprintf(status, sizeof(status), "%s, найдено: %u",
             s_scanning ? "скан идёт" : "скан остановлен", (unsigned)count);
    gfx_text_center(&wackos_font_small, GFX_W / 2, CONTENT_Y - 2, status, TH->text_dim, 220);

    ui_list_draw(&s_list, CONTENT_Y + 16, row, t);
}

/**
 * @brief   Обработать ввод: прокрутка и показ полного адреса выбранного устройства.
 * @param   in  Событие ввода.
 */
static void input(ui_input_t in)
{
    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;

        case UI_IN_PRESS: {
            if (s_list.selected >= s_count) {
                ui_back();
                break;
            }
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            const scan_entry_t *e = &s_entries[s_list.selected];
            char msg[48];
            snprintf(msg, sizeof(msg), "%02x:%02x:%02x:%02x:%02x:%02x",
                     e->addr.val[5], e->addr.val[4], e->addr.val[3],
                     e->addr.val[2], e->addr.val[1], e->addr.val[0]);
            xSemaphoreGive(s_mutex);
            ui_toast(msg);
            break;
        }

        case UI_IN_BACK:
            ui_back();
            break;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица экрана BLE-радара для роутера ui.c. */
const ui_screen_t screen_ble_scan = {
    .title = "BLE рядом",
    .enter = enter,
    .draw = draw,
    .input = input,
    .exit = leave,
};
