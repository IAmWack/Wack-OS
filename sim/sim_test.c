/*
 * Прогон по живой плате slave.
 *
 * Гоняет НАСТОЯЩИЙ код master (слой протокола + логика + экраны) против
 * настоящей платы по USB и проверяет, что каждый ответ такой, как
 * обещает docs/PROTOCOL.md. Это не «проверка симулятора» — симулятор
 * тут просто подставляет вместо экрана и энкодера файл и скрипт.
 *
 *   ./wackos-test -p /dev/ttyUSB0         # по железу
 *   ./wackos-test                         # по фальшивому slave (для сравнения)
 *
 * Код возврата: 0 — все проверки прошли.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sim_headless.h"

#include "wackos_protocol.h"
#include "esp_timer.h"

#include "ui.h"
#include "app_logic.h"

#define REQ_TIMEOUT_MS 150
#define REQ_RETRIES    2

static int s_pass = 0;
static int s_fail = 0;
static const wackos_transport_t *s_transport = NULL;

static void check(bool ok, const char *name, const char *detail_fmt, ...)
{
    char detail[192] = "";
    if (detail_fmt) {
        va_list ap;
        va_start(ap, detail_fmt);
        vsnprintf(detail, sizeof(detail), detail_fmt, ap);
        va_end(ap);
    }

    if (ok) {
        s_pass++;
        printf("  \033[32mok\033[0m   %-34s %s\n", name, detail);
    } else {
        s_fail++;
        printf("  \033[31mFAIL\033[0m %-34s %s\n", name, detail);
    }
}

/* Обёртка запроса: возвращает статус и заполняет resp. */
static wackos_status_t req(const char *cmd, const char *args, wackos_frame_t *resp)
{
    return wackos_proto_request(WACKOS_ADDR_SLAVE1, cmd, args, resp,
                                REQ_TIMEOUT_MS, REQ_RETRIES);
}

static void expect_ok(const char *cmd, const char *args, const char *expect,
                      const char *name)
{
    wackos_frame_t resp;
    wackos_status_t st = req(cmd, args, &resp);
    if (st != WACKOS_RES_OK) {
        check(false, name, "статус %d, ждали OK", (int)st);
        return;
    }
    if (expect && strcmp(resp.payload, expect) != 0) {
        check(false, name, "payload «%s», ждали «%s»", resp.payload, expect);
        return;
    }
    check(true, name, "«%s»", resp.payload);
}

static void expect_err(const char *cmd, const char *args, const char *code,
                       const char *name)
{
    wackos_frame_t resp;
    wackos_status_t st = req(cmd, args, &resp);
    if (st != WACKOS_RES_REMOTE_ERR) {
        check(false, name, "статус %d, ждали RES ERR", (int)st);
        return;
    }
    check(strcmp(resp.payload, code) == 0, name, "«%s», ждали «%s»", resp.payload, code);
}

/* ------------------------------------------------------------------ */
/* Проверки протокола                                                  */
/* ------------------------------------------------------------------ */

static void test_sys(void)
{
    printf("\nSYS_*\n");
    expect_ok("SYS_PING", NULL, "PONG", "SYS_PING отвечает PONG");

    wackos_frame_t resp;
    if (req("SYS_VERSION", NULL, &resp) == WACKOS_RES_OK) {
        const char *comma = strchr(resp.payload, ',');
        check(comma != NULL, "SYS_VERSION вида прото,прошивка", "«%s»", resp.payload);
        check(comma && strncmp(resp.payload, WACKOS_PROTO_VERSION,
                               (size_t)(comma - resp.payload)) == 0,
              "версия протокола совпадает с нашей", "у платы «%.*s», у нас «%s»",
              comma ? (int)(comma - resp.payload) : 0, resp.payload, WACKOS_PROTO_VERSION);
    } else {
        check(false, "SYS_VERSION отвечает", "нет ответа");
    }

    if (req("SYS_CAPS", NULL, &resp) == WACKOS_RES_OK) {
        check(strstr(resp.payload, "SYS_PING") != NULL,
              "SYS_CAPS перечисляет команды", "%zu байт", strlen(resp.payload));
    } else {
        check(false, "SYS_CAPS отвечает", "нет ответа");
    }
}

static void test_ui_commands(void)
{
    printf("\nUI_*\n");

    wackos_frame_t resp;
    if (req("UI_MENU_GET", NULL, &resp) == WACKOS_RES_OK) {
        check(strchr(resp.payload, ':') && strchr(resp.payload, ';'),
              "UI_MENU_GET отдаёт список", "«%s»", resp.payload);
        /* Пробелы внутри payload — то, ради чего правили парсер. */
        check(strstr(resp.payload, "Say hello") != NULL,
              "пробелы в названии пункта целы", "«%s»", resp.payload);
    } else {
        check(false, "UI_MENU_GET отвечает", "нет ответа");
    }

    expect_ok("UI_MENU_SELECT", "hello", "Hello from slave!",
              "UI_MENU_SELECT hello");
    expect_ok("UI_MENU_SELECT", "about", "Wack-OS slave skeleton",
              "UI_MENU_SELECT about");
    expect_err("UI_MENU_SELECT", "нет-такого", WACKOS_ERR_BAD_ARGS,
               "неизвестный id -> E_BAD_ARGS");
}

static void test_errors(void)
{
    printf("\nОшибки и заглушки\n");
    expect_err("SYS_NOPE", NULL, WACKOS_ERR_UNKNOWN_CMD,
               "неизвестная команда -> E_UNKNOWN_CMD");
    expect_err("RADIO_SCAN", NULL, WACKOS_ERR_NOT_SUPPORTED,
               "RADIO_* -> E_NOT_SUPPORTED");
    expect_err("HID_TYPE", NULL, WACKOS_ERR_NOT_SUPPORTED,
               "HID_* -> E_NOT_SUPPORTED");
    expect_err("SENSOR_READ", NULL, WACKOS_ERR_NOT_SUPPORTED,
               "SENSOR_* -> E_NOT_SUPPORTED");
}

/* Кадр с намеренно испорченной контрольной суммой — собираем руками,
 * потому что слой протокола правильный кадр испортить не даст. */
static void test_bad_csum(void)
{
    printf("\nБитый кадр\n");

    const char *line = "REQ 01 SYS_PING 0BAD - 00\n";
    s_transport->write((const uint8_t *)line, strlen(line));

    uint8_t raw[WACKOS_LINE_MAX];
    int n = s_transport->read_line(raw, sizeof(raw), 400);
    if (n <= 0) {
        check(false, "битая CSUM -> RES ERR E_CRC", "плата промолчала");
        return;
    }

    wackos_frame_t resp;
    size_t len = (size_t)n;
    if (len && raw[len - 1] == '\n') {
        len--;
    }
    if (wackos_frame_parse((const char *)raw, len, &resp) == WACKOS_PARSE_MALFORMED) {
        check(false, "битая CSUM -> RES ERR E_CRC", "невнятный ответ «%.*s»", n, raw);
        return;
    }
    check(strcmp(resp.field, "ERR") == 0 && strcmp(resp.payload, WACKOS_ERR_CRC) == 0,
          "битая CSUM -> RES ERR E_CRC", "«%s %s», seq %04X",
          resp.field, resp.payload, resp.seq);
}

/* Адресация: запрос на чужой адрес не наш, отвечать на него slave 01
 * не должен (docs/PROTOCOL.md: адрес — часть контракта). */
static void test_addressing(void)
{
    printf("\nАдресация\n");
    wackos_frame_t resp;
    wackos_status_t st = wackos_proto_request(2, "SYS_PING", NULL, &resp,
                                              REQ_TIMEOUT_MS, 0);
    check(st == WACKOS_RES_TIMEOUT, "запрос на адрес 02 игнорируется",
          st == WACKOS_RES_TIMEOUT ? "тишина, как и ждали"
                                   : "ответил: addr %02u «%s»", resp.addr, resp.payload);
}

static void test_rtt(void)
{
    printf("\nВремя ответа\n");

    const int n = 20;
    int64_t total = 0, worst = 0;
    int lost = 0;

    for (int i = 0; i < n; i++) {
        wackos_frame_t resp;
        int64_t t0 = esp_timer_get_time();
        wackos_status_t st = req("SYS_PING", NULL, &resp);
        int64_t dt = esp_timer_get_time() - t0;

        if (st != WACKOS_RES_OK) {
            lost++;
            continue;
        }
        total += dt;
        if (dt > worst) {
            worst = dt;
        }
    }

    int ok_count = n - lost;
    check(lost == 0, "20 пингов подряд без потерь", "потеряно %d", lost);
    if (ok_count > 0) {
        double avg = (double)total / ok_count / 1000.0;
        check(avg * 1000 < REQ_TIMEOUT_MS * 1000,
              "среднее время ответа в пределах тайм-аута",
              "среднее %.1f мс, худшее %.1f мс (тайм-аут %d мс)",
              avg, worst / 1000.0, REQ_TIMEOUT_MS);
    }
}

/*
 * Перезагрузка — последней: плата на ~0.6 с выпадает из эфира.
 *
 * Проверяем не «master увидел обрыв», а ровно то, что обещает
 * docs/PROTOCOL.md: одиночная попытка в это время отваливается по
 * тайм-ауту, но три попытки за 450 мс перекрывают ребут, и для
 * пользователя перезагрузка проходит незаметно. Плюс лог загрузчика
 * ESP летит по тому же проводу — он не должен приниматься за кадр.
 */
static void test_reset_recovery(void)
{
    printf("\nПерезагрузка и восстановление\n");

    wackos_frame_t resp;
    wackos_status_t st = req("SYS_RESET", NULL, &resp);
    check(st == WACKOS_RES_OK, "SYS_RESET принят", "статус %d", (int)st);

    /* Сразу после reset одиночная попытка обязана уйти в тайм-аут. */
    int64_t t0 = esp_timer_get_time();
    wackos_status_t single = wackos_proto_request(WACKOS_ADDR_SLAVE1, "SYS_PING", NULL,
                                                  &resp, REQ_TIMEOUT_MS, 0);
    check(single == WACKOS_RES_TIMEOUT, "во время ребута одиночная попытка отваливается",
          single == WACKOS_RES_TIMEOUT ? "тайм-аут, как и ждали"
                                       : "ответ «%s» — плата не ушла в ребут?", resp.payload);

    /* А запрос с ретраями должен пережить ребут молча. */
    bool recovered = false;
    int attempts = 0;
    while ((esp_timer_get_time() - t0) < 5 * 1000000LL) {
        attempts++;
        if (req("SYS_PING", NULL, &resp) == WACKOS_RES_OK &&
            strcmp(resp.payload, "PONG") == 0) {
            recovered = true;
            break;
        }
    }
    double secs = (esp_timer_get_time() - t0) / 1000000.0;
    check(recovered, "ретраи протокола перекрывают ребут",
          recovered ? "снова PONG за %.2f с (%d запрос(ов))"
                    : "не дождались за %.2f с (%d запросов)", secs, attempts);
    /* Ребут (~0.6 с) длиннее одного окна ретраев (450 мс), поэтому
     * один-два запроса — норма, а вот больше трёх значит, что плата
     * поднимается непозволительно долго. */
    check(recovered && attempts <= 3, "ребут переживается за пару запросов",
          "хватило %d запрос(ов) с ретраями", attempts);

    /* Лог загрузчика шёл по тому же проводу — убеждаемся, что связь
     * после него в порядке, а не «наполовину разобрана». */
    expect_ok("SYS_VERSION", NULL, NULL, "после ребута версия читается");
}

/* ------------------------------------------------------------------ */
/* Проверки интерфейса поверх живой платы                              */
/* ------------------------------------------------------------------ */

static void test_ui_flow(void)
{
    printf("\nИнтерфейс на живых данных\n");

    ui_init();

    /* Загрузка сама сходит к плате: пинг, версия, список модулей. */
    int guard = 0;
    while (ui_current() != SCREEN_MENU && guard++ < 200) {
        headless_idle(5);
    }
    check(ui_current() == SCREEN_MENU, "загрузка доходит до меню",
          "экран %d", (int)ui_current());
    check(ui_link_online(), "после загрузки связь помечена живой", "");
    check(app_item_count() > 0, "модули приехали с платы", "%zu шт",
          app_item_count());

    const slave_item_t *first = app_item(0);
    check(first && strchr(first->title, ' ') != NULL,
          "название модуля с пробелом дошло целиком",
          "«%s»", first ? first->title : "-");
    headless_shoot("live-01-menu");

    /* Меню -> Модули -> первый модуль. */
    headless_act(WACKOS_ENC_BTN);
    check(ui_current() == SCREEN_MODULES, "вход в список модулей", "");
    headless_shoot("live-02-modules");

    headless_act(WACKOS_ENC_BTN);
    check(ui_current() == SCREEN_MODULE, "вход в модуль", "");
    check(strcmp(app_last_result(), "Hello from slave!") == 0,
          "экран модуля показал ответ платы", "«%s»", app_last_result());
    headless_shoot("live-03-module");

    /* Длинное нажатие возвращает назад. */
    headless_act(WACKOS_ENC_BTN_LONG);
    check(ui_current() == SCREEN_MODULES, "длинное нажатие — назад", "");
    headless_act(WACKOS_ENC_BTN_LONG);
    check(ui_current() == SCREEN_MENU, "и ещё раз назад — в меню", "");

    /* Экран slave: версии и RTT приходят из ответов платы. */
    ui_goto(SCREEN_SLAVE);
    headless_act(WACKOS_ENC_BTN); /* Пинг */
    check(app_rtt_ms() < REQ_TIMEOUT_MS, "RTT на экране slave осмысленный",
          "%u мс", (unsigned)app_rtt_ms());
    headless_shoot("live-04-slave");
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    const char *port = NULL;
    s_transport = headless_connect(argc, argv, &port);
    if (!s_transport) {
        return 1;
    }
    wackos_proto_init(s_transport);

    printf("\n=== Wack-OS: прогон по %s ===\n",
           port ? port : "фальшивому slave (в памяти)");

    test_ui_flow();
    test_sys();
    test_ui_commands();
    test_errors();
    test_bad_csum();
    test_addressing();
    test_rtt();
    test_reset_recovery();

    printf("\n=== прошло %d, упало %d ===\n\n", s_pass, s_fail);
    sim_serial_close();
    return s_fail == 0 ? 0 : 1;
}
