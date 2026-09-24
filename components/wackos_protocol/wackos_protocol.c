/**
 * @file    wackos_protocol.c
 * @author  IAmWack
 * @brief   Реализация текстового UART-протокола Wack-OS (кадры REQ/RES/EVT).
 *
 * Общий слой для master и slave (components/wackos_protocol). Ничего не
 * знает про железо — работает только через интерфейс транспорта
 * (wackos_transport_t, см. wackos_transport.h). Формат кадров и коды
 * ошибок — см. docs/PROTOCOL.md.
 */
#include "wackos_protocol.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_timer.h" /* монотонное время для тайм-аутов; единственная
                         * "не совсем платформо-независимая" зависимость,
                         * но это системный сервис ESP-IDF, а не конкретное
                         * железо — есть на любом чипе Espressif. */

/* ============================================================================
 *  Static state
 * ============================================================================ */

static const wackos_transport_t *s_transport = NULL;

/* Свой адрес: на него отвечаем, на чужие кадры молчим. */
static uint8_t s_own_addr = WACKOS_ADDR_SLAVE1;

static uint16_t s_next_req_seq = 0; /* счётчик seq для REQ (master) */
static uint16_t s_next_evt_seq = 0; /* отдельный счётчик seq для EVT (slave) */

/* Однослотовый буфер для EVT, случайно пойманного во время ожидания RES. */
static wackos_frame_t s_pending_event;
static bool s_has_pending_event = false;

/* ============================================================================
 *  Static helpers — время, контрольная сумма, роль кадра
 * ============================================================================ */

/**
 * @brief   Текущее время в миллисекундах (монотонное, для тайм-аутов).
 * @return  Миллисекунды с произвольной точки отсчёта.
 */
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/** @brief  XOR-контрольная сумма len байт data (см. поле CSUM в docs/PROTOCOL.md). */
static uint8_t compute_checksum(const char *data, size_t len)
{
    uint8_t csum = 0;
    for (size_t i = 0; i < len; i++) {
        csum ^= (uint8_t)data[i];
    }
    return csum;
}

/** @brief  Имя роли кадра для сериализации ("REQ"/"RES"/"EVT"). */
static const char *role_to_str(wackos_role_t role)
{
    switch (role) {
        case WACKOS_ROLE_REQ: return "REQ";
        case WACKOS_ROLE_RES: return "RES";
        case WACKOS_ROLE_EVT: return "EVT";
        default:              return "???";
    }
}

/** @brief  Обратный разбор роли кадра из строки. @return true, если распознано. */
static bool str_to_role(const char *s, wackos_role_t *out)
{
    if (strcmp(s, "REQ") == 0) { *out = WACKOS_ROLE_REQ; return true; }
    if (strcmp(s, "RES") == 0) { *out = WACKOS_ROLE_RES; return true; }
    if (strcmp(s, "EVT") == 0) { *out = WACKOS_ROLE_EVT; return true; }
    return false;
}

/* ============================================================================
 *  Public API — адрес, инициализация, сериализация кадров
 * ============================================================================ */

/** @brief  Задать собственный адрес (см. пояснение в wackos_protocol.h). */
void wackos_proto_set_address(uint8_t addr)
{
    s_own_addr = addr;
}

/** @brief  Инициализация слоя протокола конкретным транспортом. Сбрасывает счётчики seq. */
void wackos_proto_init(const wackos_transport_t *transport)
{
    s_transport = transport;
    s_next_req_seq = 0;
    s_next_evt_seq = 0;
    s_has_pending_event = false;
}

/**
 * @brief   Собрать кадр в текстовую строку протокола (с CSUM и '\n').
 * @param   frame     Кадр для сериализации.
 * @param   out       Буфер для результата.
 * @param   out_size  Размер буфера out.
 * @return  Длина строки без завершающего нуля, либо 0 при ошибке (не влез).
 */
size_t wackos_frame_build(const wackos_frame_t *frame, char *out, size_t out_size)
{
    char core[WACKOS_LINE_MAX];
    const char *payload = frame->payload[0] ? frame->payload : "-";

    int core_len = snprintf(core, sizeof(core), "%s %02u %s %04X %s",
                             role_to_str(frame->role), (unsigned int)frame->addr,
                             frame->field, (unsigned int)frame->seq, payload);
    if (core_len < 0 || (size_t)core_len >= sizeof(core)) {
        return 0;
    }

    uint8_t csum = compute_checksum(core, (size_t)core_len);
    int full_len = snprintf(out, out_size, "%s %02X\n", core, (unsigned int)csum);
    if (full_len < 0 || (size_t)full_len >= out_size) {
        return 0;
    }
    return (size_t)full_len;
}

/**
 * @brief   Разобрать строку кадра (без завершающего '\n') в frame.
 * @param   line      Строка кадра, без завершающего '\n'.
 * @param   line_len  Длина line.
 * @param   frame     Куда записать разобранные поля.
 * @return  WACKOS_PARSE_OK, WACKOS_PARSE_CRC_FAIL (см. ниже) или
 *          WACKOS_PARSE_MALFORMED, если строку не удалось разобрать вовсе.
 *
 * При WACKOS_PARSE_CRC_FAIL frame всё равно заполнен best-effort — нужно,
 * чтобы отправитель ошибки (wackos_proto_receive_request) мог ответить
 * E_CRC с тем же ADDR/SEQ, что были в исходном кадре.
 */
wackos_parse_result_t wackos_frame_parse(const char *line, size_t line_len, wackos_frame_t *frame)
{
    /* Ожидаем строку без завершающего '\n' (транспорт его отрезает). */
    if (line_len == 0 || line_len >= WACKOS_LINE_MAX) {
        return WACKOS_PARSE_MALFORMED;
    }

    char buf[WACKOS_LINE_MAX];
    memcpy(buf, line, line_len);
    buf[line_len] = '\0';

    char *last_space = strrchr(buf, ' ');
    if (!last_space || strlen(last_space + 1) != 2) {
        return WACKOS_PARSE_MALFORMED; /* нет поля контрольной суммы */
    }

    size_t core_len = (size_t)(last_space - buf);
    uint8_t expected_csum = (uint8_t)strtol(last_space + 1, NULL, 16);
    uint8_t actual_csum = compute_checksum(buf, core_len);
    bool crc_ok = (actual_csum == expected_csum);
    *last_space = '\0';

    char *saveptr = NULL;
    char *tok_role  = strtok_r(buf, " ", &saveptr);
    char *tok_addr  = strtok_r(NULL, " ", &saveptr);
    char *tok_field = strtok_r(NULL, " ", &saveptr);
    char *tok_seq   = strtok_r(NULL, " ", &saveptr);

    /*
     * PAYLOAD — весь остаток строки, включая пробелы внутри. CSUM уже
     * отрезан выше по ПОСЛЕДНЕМУ пробелу, а перед payload полей ровно
     * четыре, так что остаток — это ровно payload и ничего больше.
     * Иначе ответ вроде "Hello from slave!" доезжал бы как "Hello"
     * (см. docs/PROTOCOL.md, поле PAYLOAD).
     */
    char *tok_payload = saveptr;
    if (!tok_role || !tok_addr || !tok_field || !tok_seq || !tok_payload || !*tok_payload) {
        return WACKOS_PARSE_MALFORMED;
    }
    if (!str_to_role(tok_role, &frame->role)) {
        return WACKOS_PARSE_MALFORMED;
    }

    /* Поля извлекаем даже при несовпадении CSUM (best-effort) — нужно,
     * чтобы можно было ответить E_CRC с тем же ADDR/SEQ (см.
     * docs/PROTOCOL.md, раздел "Коды ошибок"). */
    frame->addr = (uint8_t)strtoul(tok_addr, NULL, 10);
    frame->seq  = (uint16_t)strtoul(tok_seq, NULL, 16);

    strncpy(frame->field, tok_field, sizeof(frame->field) - 1);
    frame->field[sizeof(frame->field) - 1] = '\0';

    strncpy(frame->payload, tok_payload, sizeof(frame->payload) - 1);
    frame->payload[sizeof(frame->payload) - 1] = '\0';

    return crc_ok ? WACKOS_PARSE_OK : WACKOS_PARSE_CRC_FAIL;
}

/* ============================================================================
 *  Static helpers — отправка/чтение кадра через транспорт
 * ============================================================================ */

/** @brief  Собрать и отправить кадр через текущий транспорт (no-op, если транспорт не задан). */
static void send_frame(const wackos_frame_t *frame)
{
    char line[WACKOS_LINE_MAX];
    size_t len = wackos_frame_build(frame, line, sizeof(line));
    if (len > 0 && s_transport && s_transport->write) {
        s_transport->write((const uint8_t *)line, len);
    }
}

/** @brief  Прочитать одну строку-кадр из транспорта с заданным тайм-аутом. */
static wackos_parse_result_t read_frame(wackos_frame_t *out, uint32_t timeout_ms)
{
    if (!s_transport || !s_transport->read_line) {
        return WACKOS_PARSE_MALFORMED;
    }
    uint8_t raw[WACKOS_LINE_MAX];
    int n = s_transport->read_line(raw, sizeof(raw), timeout_ms);
    if (n <= 0) {
        return WACKOS_PARSE_MALFORMED; /* тайм-аут чтения — трактуем как "нечего разобрать" */
    }
    /* транспорт отдаёт кадр вместе с завершающим '\n' — отрезаем его */
    size_t len = (size_t)n;
    if (len > 0 && raw[len - 1] == '\n') {
        len--;
    }
    if (len > 0 && raw[len - 1] == '\r') {
        len--;
    }
    return wackos_frame_parse((const char *)raw, len, out);
}

/* ============================================================================
 *  Public API — сторона master: синхронный запрос-ответ
 * ============================================================================ */

/**
 * @brief   Отправить REQ и дождаться RES с тем же seq (с ретраями).
 * @param   addr          Адрес получателя (slave).
 * @param   cmd           Имя команды (например "SYS_PING").
 * @param   args          Payload запроса, либо NULL/"" для "-".
 * @param   out_response  Куда положить разобранный ответ (может быть NULL).
 * @param   timeout_ms    Тайм-аут ожидания одной попытки, мс.
 * @param   retries       Сколько дополнительных попыток сделать после первой.
 * @return  WACKOS_RES_OK/WACKOS_RES_REMOTE_ERR по полю RES, либо
 *          WACKOS_RES_TIMEOUT, если не дождались за все попытки.
 *          Дефолты timeout/retries — см. docs/PROTOCOL.md.
 */
wackos_status_t wackos_proto_request(uint8_t addr, const char *cmd, const char *args,
                                      wackos_frame_t *out_response,
                                      uint32_t timeout_ms, uint8_t retries)
{
    wackos_frame_t req = {
        .role = WACKOS_ROLE_REQ,
        .addr = addr,
        .seq  = s_next_req_seq++,
    };
    strncpy(req.field, cmd, sizeof(req.field) - 1);
    req.field[sizeof(req.field) - 1] = '\0';
    strncpy(req.payload, (args && args[0]) ? args : "-", sizeof(req.payload) - 1);
    req.payload[sizeof(req.payload) - 1] = '\0';

    for (uint8_t attempt = 0; attempt <= retries; attempt++) {
        send_frame(&req);

        uint32_t deadline = now_ms() + timeout_ms;
        while (now_ms() < deadline) {
            wackos_frame_t rx;
            uint32_t remaining = deadline - now_ms();
            if (read_frame(&rx, remaining) != WACKOS_PARSE_OK) {
                /* тайм-аут чтения, битый кадр, либо CRC-мусор — на
                 * стороне master отвечать на RES/EVT нечем, просто
                 * пробуем ещё, пока не истёк deadline */
                continue;
            }

            if (rx.role == WACKOS_ROLE_EVT) {
                /* Событие пришло не вовремя, но терять его не будем —
                 * положим в однослотовый буфер для wackos_proto_poll_event(). */
                s_pending_event = rx;
                s_has_pending_event = true;
                continue;
            }

            if (rx.role != WACKOS_ROLE_RES || rx.addr != addr || rx.seq != req.seq) {
                continue; /* не наш ответ (например, застрявший RES от старого ретрая) */
            }

            if (out_response) {
                *out_response = rx;
            }
            return (strcmp(rx.field, "OK") == 0) ? WACKOS_RES_OK : WACKOS_RES_REMOTE_ERR;
        }
        /* тайм-аут — уходим на следующую попытку с тем же seq */
    }

    return WACKOS_RES_TIMEOUT;
}

/**
 * @brief   Неблокирующая проверка асинхронного EVT-кадра.
 * @param   out_event   Куда положить событие, если оно было.
 * @param   timeout_ms  Сколько ждать новый кадр, если нет отложенного.
 * @return  true, если событие получено (сразу из буфера или дождались).
 */
bool wackos_proto_poll_event(wackos_frame_t *out_event, uint32_t timeout_ms)
{
    if (s_has_pending_event) {
        *out_event = s_pending_event;
        s_has_pending_event = false;
        return true;
    }

    wackos_frame_t rx;
    if (read_frame(&rx, timeout_ms) == WACKOS_PARSE_OK && rx.role == WACKOS_ROLE_EVT) {
        *out_event = rx;
        return true;
    }
    return false;
}

/* ============================================================================
 *  Public API — сторона slave: приём запроса, отправка ответа/события
 * ============================================================================ */

/**
 * @brief   Дождаться следующего REQ, адресованного нам (блокирующе, с тайм-аутом).
 * @param   out_request  Куда записать разобранный запрос.
 * @param   timeout_ms   Тайм-аут ожидания, мс.
 * @return  true, если запрос получен и заполнен в out_request.
 */
bool wackos_proto_receive_request(wackos_frame_t *out_request, uint32_t timeout_ms)
{
    uint32_t deadline = now_ms() + timeout_ms;
    while (now_ms() < deadline) {
        wackos_frame_t rx;
        uint32_t remaining = deadline - now_ms();
        wackos_parse_result_t pr = read_frame(&rx, remaining);

        if (pr == WACKOS_PARSE_CRC_FAIL) {
            /* ADDR/SEQ извлечены best-effort из повреждённого кадра —
             * отвечаем E_CRC по docs/PROTOCOL.md вместо молчания. Но
             * только если кадр адресован нам: чужой мусор — не наше
             * дело, иначе на шине мы затопчём чужой обмен. */
            if (rx.addr == s_own_addr) {
                wackos_proto_send_err(&rx, WACKOS_ERR_CRC);
            }
            continue;
        }
        if (pr != WACKOS_PARSE_OK) {
            continue; /* тайм-аут чтения либо совсем нечитаемый кадр */
        }
        if (rx.role != WACKOS_ROLE_REQ) {
            continue; /* slave запросы другим slave не шлёт, игнорируем мусор */
        }
        if (rx.addr != s_own_addr) {
            /*
             * Кадр не нам — молчим совсем (не ERR, именно тишина): для
             * отправителя это выглядит как "того устройства нет".
             * Broadcast (адрес 00) сюда пока не попадает: что именно
             * на него отвечать, решится, когда появится настоящая шина
             * (docs/PROTOCOL.md, раздел "Адресация").
             */
            continue;
        }
        *out_request = rx;
        return true;
    }
    return false;
}

/** @brief  Отправить RES OK с тем же addr/seq, что был в request. */
void wackos_proto_send_ok(const wackos_frame_t *request, const char *payload)
{
    wackos_frame_t res = {
        .role = WACKOS_ROLE_RES,
        .addr = request->addr,
        .seq  = request->seq,
    };
    strncpy(res.field, "OK", sizeof(res.field) - 1);
    strncpy(res.payload, (payload && payload[0]) ? payload : "-", sizeof(res.payload) - 1);
    res.payload[sizeof(res.payload) - 1] = '\0';
    send_frame(&res);
}

/** @brief  Отправить RES ERR с тем же addr/seq, что был в request (error_code — один из WACKOS_ERR_*). */
void wackos_proto_send_err(const wackos_frame_t *request, const char *error_code)
{
    wackos_frame_t res = {
        .role = WACKOS_ROLE_RES,
        .addr = request->addr,
        .seq  = request->seq,
    };
    strncpy(res.field, "ERR", sizeof(res.field) - 1);
    strncpy(res.payload, error_code, sizeof(res.payload) - 1);
    res.payload[sizeof(res.payload) - 1] = '\0';
    send_frame(&res);
}

/** @brief  Отправить асинхронное событие EVT (не привязано к конкретному REQ). */
void wackos_proto_send_event(uint8_t addr, const char *event_name, const char *payload)
{
    wackos_frame_t evt = {
        .role = WACKOS_ROLE_EVT,
        .addr = addr,
        .seq  = s_next_evt_seq++,
    };
    strncpy(evt.field, event_name, sizeof(evt.field) - 1);
    strncpy(evt.payload, (payload && payload[0]) ? payload : "-", sizeof(evt.payload) - 1);
    evt.payload[sizeof(evt.payload) - 1] = '\0';
    send_frame(&evt);
}
