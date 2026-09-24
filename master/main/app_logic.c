#include "app_logic.h"
#include "ui.h"
#include "astolfo.h"
#include "sound.h"
#include "wackos_protocol.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "app_logic";

/* Тайм-аут/ретраи запроса — см. docs/PROTOCOL.md ("Тайм-ауты и ретраи"). */
#define REQ_TIMEOUT_MS 150
#define REQ_RETRIES    2

static bool s_online = false;
static char s_fw[32] = "?";
static char s_proto[8] = "?";
static char s_caps[WACKOS_PAYLOAD_MAX] = "";
static uint32_t s_rtt_ms = 0;

static slave_item_t s_items[SLAVE_ITEM_MAX];
static size_t s_item_count = 0;

/* payload (128) + место под приписку вроде "версия: " */
static char s_result[WACKOS_PAYLOAD_MAX + 32] = "";

static int s_boot_step = 0;

/*
 * Один запрос к slave со всей обвязкой интерфейса: показать "думаю",
 * замерить время ответа, обновить индикатор связи и настроение
 * Астольфо. Вся реакция системы на "жив ли slave" собрана здесь, чтобы
 * не повторять её в каждом экране.
 */
static wackos_status_t ask(const char *cmd, const char *args, wackos_frame_t *resp)
{
    ui_set_busy(true);
    int64_t t0 = esp_timer_get_time();

    wackos_status_t st = wackos_proto_request(WACKOS_ADDR_SLAVE1, cmd, args, resp,
                                              REQ_TIMEOUT_MS, REQ_RETRIES);

    s_rtt_ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
    ui_set_busy(false);

    /* Ответ пришёл (хоть OK, хоть ERR) — значит, провод жив. */
    s_online = (st == WACKOS_RES_OK || st == WACKOS_RES_REMOTE_ERR);
    ui_set_link_online(s_online);

    if (st == WACKOS_RES_OK) {
        astolfo_set_mood(ASTOLFO_HAPPY);
    } else {
        astolfo_set_mood(ASTOLFO_SAD);
        sound_play(SOUND_ERROR);
    }
    return st;
}

/* Разложить ответ в s_result и в тост одинаково для всех действий. */
static bool finish(wackos_status_t st, const wackos_frame_t *resp, const char *prefix)
{
    if (st == WACKOS_RES_OK) {
        snprintf(s_result, sizeof(s_result), "%s%s", prefix, resp->payload);
    } else if (st == WACKOS_RES_REMOTE_ERR) {
        snprintf(s_result, sizeof(s_result), "ошибка: %s", resp->payload);
    } else {
        snprintf(s_result, sizeof(s_result), "slave не отвечает (>_<)");
    }
    ui_toast(s_result);
    return st == WACKOS_RES_OK;
}

void app_init(void)
{
    s_boot_step = 0;
    s_item_count = 0;
    s_online = false;
}

/* ------------------------------------------------------------------ */

bool app_ping(void)
{
    wackos_frame_t resp;
    return finish(ask("SYS_PING", NULL, &resp), &resp, "");
}

bool app_read_version(void)
{
    wackos_frame_t resp;
    wackos_status_t st = ask("SYS_VERSION", NULL, &resp);
    if (st == WACKOS_RES_OK) {
        /* payload: "<версия_протокола>,<версия_прошивки>" */
        char buf[WACKOS_PAYLOAD_MAX];
        strncpy(buf, resp.payload, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *comma = strchr(buf, ',');
        if (comma) {
            *comma = '\0';
            strncpy(s_proto, buf, sizeof(s_proto) - 1);
            s_proto[sizeof(s_proto) - 1] = '\0';
            strncpy(s_fw, comma + 1, sizeof(s_fw) - 1);
            s_fw[sizeof(s_fw) - 1] = '\0';
        }
    }
    return finish(st, &resp, "версия: ");
}

bool app_read_caps(void)
{
    wackos_frame_t resp;
    wackos_status_t st = ask("SYS_CAPS", NULL, &resp);
    if (st == WACKOS_RES_OK) {
        strncpy(s_caps, resp.payload, sizeof(s_caps) - 1);
        s_caps[sizeof(s_caps) - 1] = '\0';
    }
    return finish(st, &resp, "умеет: ");
}

bool app_reset_slave(void)
{
    wackos_frame_t resp;
    ask("SYS_RESET", NULL, &resp);
    snprintf(s_result, sizeof(s_result), "отправила reset, жду slave...");
    ui_toast(s_result);
    s_online = false;
    ui_set_link_online(false);
    return true;
}

bool app_refresh_items(void)
{
    wackos_frame_t resp;
    wackos_status_t st = ask("UI_MENU_GET", NULL, &resp);

    s_item_count = 0;
    if (st != WACKOS_RES_OK || strcmp(resp.payload, "-") == 0) {
        ESP_LOGW(TAG, "slave не отдал список модулей");
        return finish(st, &resp, "модули: ");
    }

    /* payload: "<id1>:<title1>;<id2>:<title2>;..." */
    char buf[WACKOS_PAYLOAD_MAX];
    strncpy(buf, resp.payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *entry = strtok_r(buf, ";", &saveptr);
    while (entry && s_item_count < SLAVE_ITEM_MAX) {
        char *colon = strchr(entry, ':');
        if (colon) {
            *colon = '\0';
            strncpy(s_items[s_item_count].id, entry, sizeof(s_items[0].id) - 1);
            s_items[s_item_count].id[sizeof(s_items[0].id) - 1] = '\0';
            strncpy(s_items[s_item_count].title, colon + 1, SLAVE_TITLE_LEN - 1);
            s_items[s_item_count].title[SLAVE_TITLE_LEN - 1] = '\0';
            s_item_count++;
        }
        entry = strtok_r(NULL, ";", &saveptr);
    }

    snprintf(s_result, sizeof(s_result), "модулей: %u", (unsigned)s_item_count);
    ui_toast(s_result);
    return true;
}

bool app_run_item(size_t index)
{
    if (index >= s_item_count) {
        return false;
    }
    wackos_frame_t resp;
    return finish(ask("UI_MENU_SELECT", s_items[index].id, &resp), &resp, "");
}

/* ------------------------------------------------------------------ */

bool app_boot_step(const char **out_label, int *out_percent)
{
    static const char *LABELS[] = {
        "поднимаю интерфейс",
        "ищу slave",
        "спрашиваю версию",
        "читаю модули",
        "готово ♥",
    };
    const int steps = (int)(sizeof(LABELS) / sizeof(LABELS[0]));

    if (out_label) {
        *out_label = LABELS[s_boot_step < steps ? s_boot_step : steps - 1];
    }
    if (out_percent) {
        *out_percent = s_boot_step * 100 / (steps - 1);
    }

    switch (s_boot_step) {
        case 0: break;                 /* дать кадру нарисоваться */
        case 1: app_ping(); break;
        case 2: app_read_version(); break;
        case 3: app_refresh_items(); break;
        default:
            return false;              /* загрузка закончена */
    }

    s_boot_step++;
    return true;
}

bool        app_online(void)      { return s_online; }
const char *app_slave_fw(void)    { return s_fw; }
const char *app_slave_proto(void) { return s_proto; }
const char *app_slave_caps(void)  { return s_caps; }
uint32_t    app_rtt_ms(void)      { return s_rtt_ms; }

size_t app_item_count(void)
{
    return s_item_count;
}

const slave_item_t *app_item(size_t index)
{
    return (index < s_item_count) ? &s_items[index] : NULL;
}

const char *app_last_result(void)
{
    return s_result;
}
