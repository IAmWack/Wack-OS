/**
 * @file    dispatcher.c
 * @author  IAmWack
 * @brief   Диспетчер команд slave: разбор REQ по имени и вызов обработчика.
 *
 * Единственное место, которое знает имена команд протокола и умеет на
 * них отвечать (см. docs/PROTOCOL.md). Неизвестные команды из
 * namespace модулей (ENC_/RADIO_/HID_/SENSOR_) отдаются на откуп
 * соответствующему module_*_handle().
 */
#include "dispatcher.h"
#include "modules/module_radio.h"
#include "modules/module_hid.h"
#include "modules/module_sensor.h"
#include "modules/module_encoder.h"
#include "board_pins.h"
#include "uart_transport.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define FW_VERSION "0.1.0-skeleton"

/* ============================================================================
 *  Types
 * ============================================================================ */

/*
 * Пункты меню, которые slave отдаёт master через UI_MENU_GET. Это
 * единственное место, где slave описывает себя — master заранее про
 * них ничего не знает и просто рисует то, что пришло.
 * Пока это демонстрационные пункты без реальных модулей.
 */
typedef struct {
    const char *id;
    const char *title;
} menu_entry_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

static const menu_entry_t MENU[] = {
    { "hello", "Say hello" },
    { "about", "About slave" },
};
#define MENU_COUNT (sizeof(MENU) / sizeof(MENU[0]))

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/**
 * @brief   Обработчик SYS_PING — отвечает "PONG".
 * @param   req  Входящий запрос SYS_PING.
 */
static void handle_sys_ping(const wackos_frame_t *req)
{
    wackos_proto_send_ok(req, "PONG");
}

/**
 * @brief   Обработчик SYS_VERSION — версия протокола и прошивки.
 * @param   req  Входящий запрос SYS_VERSION.
 */
static void handle_sys_version(const wackos_frame_t *req)
{
    char payload[WACKOS_PAYLOAD_MAX];
    snprintf(payload, sizeof(payload), "%s,%s", WACKOS_PROTO_VERSION, FW_VERSION);
    wackos_proto_send_ok(req, payload);
}

/**
 * @brief   Обработчик SYS_CAPS — список поддерживаемых команд.
 * @param   req  Входящий запрос SYS_CAPS.
 */
static void handle_sys_caps(const wackos_frame_t *req)
{
    wackos_proto_send_ok(req,
        "SYS_PING;SYS_VERSION;SYS_CAPS;SYS_RESET;UI_MENU_GET;UI_MENU_SELECT;ENC_GET");
}

/**
 * @brief   Обработчик SYS_RESET — подтверждает и перезагружает плату.
 * @param   req  Входящий запрос SYS_RESET.
 */
static void handle_sys_reset(const wackos_frame_t *req)
{
    wackos_proto_send_ok(req, "-");
    vTaskDelay(pdMS_TO_TICKS(100)); /* дать транспорту время отправить ответ */
    esp_restart();
}

/**
 * @brief   Обработчик SYS_LED <COLOR>,<STATE> — временная тестовая команда.
 * @param   req  Входящий запрос SYS_LED.
 *
 * Не часть протокола v1 (docs/PROTOCOL.md её не описывает). Нужна
 * только чтобы руками проверять формат кадров на реальном железе (два
 * светодиода на макетке) до появления master и настоящих модулей.
 */
static void handle_sys_led(const wackos_frame_t *req)
{
    char args[WACKOS_PAYLOAD_MAX];
    strncpy(args, req->payload, sizeof(args) - 1);
    args[sizeof(args) - 1] = '\0';

    char *saveptr = NULL;
    char *color = strtok_r(args, ",", &saveptr);
    char *state = strtok_r(NULL, ",", &saveptr);

    if (!color || !state) {
        wackos_proto_send_err(req, WACKOS_ERR_BAD_ARGS);
        return;
    }

    int pin;
    if (strcmp(color, "YELLOW") == 0) {
        pin = PIN_LED_YELLOW;
    } else if (strcmp(color, "RED") == 0) {
        pin = PIN_LED_RED;
    } else if (strcmp(color, "GREEN") == 0) {
        pin = PIN_LED_GREEN;
    } else {
        wackos_proto_send_err(req, WACKOS_ERR_BAD_ARGS);
        return;
    }

    int level;
    if (strcmp(state, "ON") == 0) {
        level = 1;
    } else if (strcmp(state, "OFF") == 0) {
        level = 0;
    } else {
        wackos_proto_send_err(req, WACKOS_ERR_BAD_ARGS);
        return;
    }

    gpio_set_level(pin, level);
    wackos_proto_send_ok(req, "-");
}

/**
 * @brief   Обработчик SYS_UART1_TEST — временная команда самотеста UART_NUM_1.
 * @param   req  Входящий запрос SYS_UART1_TEST.
 *
 * Не часть протокола v1. Нужна, чтобы проверить GPIO16/17
 * (PIN_UART_TX_REAL/PIN_UART_RX_REAL) до появления master: команда
 * приходит по рабочему протокольному UART0, а сама проверка идёт по
 * UART1 — требует, чтобы TX_REAL и RX_REAL были физически замкнуты
 * перемычкой (самозамыкание) на макетке.
 */
static void handle_sys_uart1_test(const wackos_frame_t *req)
{
    char result[WACKOS_PAYLOAD_MAX];
    bool ok = uart_transport_uart1_selftest(result, sizeof(result));
    if (ok) {
        wackos_proto_send_ok(req, result);
    } else {
        wackos_proto_send_err(req, result); /* result уже начинается с "FAIL ..." */
    }
}

/**
 * @brief   Обработчик UI_MENU_GET — отдаёт список пунктов меню MENU.
 * @param   req  Входящий запрос UI_MENU_GET.
 */
static void handle_ui_menu_get(const wackos_frame_t *req)
{
    char payload[WACKOS_PAYLOAD_MAX] = "";
    for (size_t i = 0; i < MENU_COUNT; i++) {
        char entry[48];
        snprintf(entry, sizeof(entry), "%s%s:%s",
                 (i == 0) ? "" : ";", MENU[i].id, MENU[i].title);
        strncat(payload, entry, sizeof(payload) - strlen(payload) - 1);
    }
    wackos_proto_send_ok(req, payload);
}

/**
 * @brief   Обработчик UI_MENU_SELECT — выполняет выбранный пункт меню.
 * @param   req  Входящий запрос UI_MENU_SELECT (payload — id пункта).
 */
static void handle_ui_menu_select(const wackos_frame_t *req)
{
    for (size_t i = 0; i < MENU_COUNT; i++) {
        if (strcmp(req->payload, MENU[i].id) != 0) {
            continue;
        }
        if (strcmp(MENU[i].id, "hello") == 0) {
            wackos_proto_send_ok(req, "Hello from slave!");
        } else {
            wackos_proto_send_ok(req, "Wack-OS slave skeleton");
        }
        return;
    }
    wackos_proto_send_err(req, WACKOS_ERR_BAD_ARGS);
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Обработать один разобранный REQ и отправить соответствующий RES.
 * @param   request  Разобранный входящий кадр-запрос.
 */
void dispatcher_handle(const wackos_frame_t *request)
{
    const char *cmd = request->field;

    if (strcmp(cmd, "SYS_PING") == 0)       { handle_sys_ping(request);       return; }
    if (strcmp(cmd, "SYS_VERSION") == 0)    { handle_sys_version(request);    return; }
    if (strcmp(cmd, "SYS_CAPS") == 0)       { handle_sys_caps(request);       return; }
    if (strcmp(cmd, "SYS_RESET") == 0)      { handle_sys_reset(request);      return; }
    if (strcmp(cmd, "SYS_LED") == 0)        { handle_sys_led(request);        return; }
    if (strcmp(cmd, "SYS_UART1_TEST") == 0) { handle_sys_uart1_test(request);  return; }
    if (strcmp(cmd, "UI_MENU_GET") == 0)    { handle_ui_menu_get(request);    return; }
    if (strcmp(cmd, "UI_MENU_SELECT") == 0) { handle_ui_menu_select(request); return; }

    /* Команда из зарезервированного namespace модуля, но ещё не
     * реализована — отвечаем E_NOT_SUPPORTED, а не E_UNKNOWN_CMD:
     * master может показать разницу "модуля нет" / "модуль есть,
     * функция не готова". */
    if (module_encoder_owns(cmd)) { module_encoder_handle(request); return; }
    if (module_radio_owns(cmd))  { module_radio_handle(request);  return; }
    if (module_hid_owns(cmd))    { module_hid_handle(request);    return; }
    if (module_sensor_owns(cmd)) { module_sensor_handle(request); return; }

    wackos_proto_send_err(request, WACKOS_ERR_UNKNOWN_CMD);
}
