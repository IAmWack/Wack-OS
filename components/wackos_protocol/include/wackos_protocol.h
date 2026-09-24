/**
 * @file    wackos_protocol.h
 * @author  IAmWack
 * @brief   Слой протокола Wack-OS. Сборка/разбор кадров REQ/RES/EVT, контрольная сумма, тайм-ауты и ретраи.
 *
 * Формат кадра и коды ошибок описаны в docs/PROTOCOL.md — этот файл
 * реализует именно тот текстовый протокол, ничего от себя не добавляя.
 *
 * Намеренно не знает про конкретное железо — работает только через
 * wackos_transport_t (см. wackos_transport.h). Один и тот же .c/.h
 * используется и на master, и на slave.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "wackos_transport.h"

/* ============================================================================
 *  Defines
 * ============================================================================ */

#define WACKOS_PROTO_VERSION   "1"

#define WACKOS_ADDR_BROADCAST  0
#define WACKOS_ADDR_SLAVE1     1   /* единственный активный slave в v1 */

#define WACKOS_FIELD_MAX       24
#define WACKOS_PAYLOAD_MAX     128
#define WACKOS_LINE_MAX        (8 + WACKOS_FIELD_MAX + WACKOS_PAYLOAD_MAX)

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef enum {
    WACKOS_ROLE_REQ = 0,
    WACKOS_ROLE_RES,
    WACKOS_ROLE_EVT,
} wackos_role_t;

/* Разобранный кадр протокола. */
typedef struct {
    wackos_role_t role;
    uint8_t       addr;
    char          field[WACKOS_FIELD_MAX];     /* имя команды либо "OK"/"ERR" */
    uint16_t      seq;
    char          payload[WACKOS_PAYLOAD_MAX]; /* "-" если данных нет */
} wackos_frame_t;

/* Результат разбора одной строки кадра wackos_frame_parse(). */
typedef enum {
    WACKOS_PARSE_OK = 0,
    WACKOS_PARSE_CRC_FAIL,   /* ROLE/ADDR/FIELD/SEQ/PAYLOAD извлечены (best-effort
                              * из повреждённого кадра), но CSUM не сошлась */
    WACKOS_PARSE_MALFORMED,  /* кадр битый настолько, что даже поля не выделить,
                              * либо ничего не пришло (тайм-аут чтения) */
} wackos_parse_result_t;

/* Результат операций протокольного слоя. */
typedef enum {
    WACKOS_RES_OK = 0,
    WACKOS_RES_TIMEOUT,      /* не дождались RES за все ретраи */
    WACKOS_RES_BAD_FRAME,    /* пришёл кадр, который не удалось разобрать */
    WACKOS_RES_CRC_FAIL,     /* контрольная сумма не сошлась */
    WACKOS_RES_REMOTE_ERR,   /* slave ответил RES ERR — см. frame->payload */
} wackos_status_t;

/* Коды ошибок в payload кадра RES ERR (docs/PROTOCOL.md). */
#define WACKOS_ERR_UNKNOWN_CMD    "E_UNKNOWN_CMD"
#define WACKOS_ERR_BAD_ARGS       "E_BAD_ARGS"
#define WACKOS_ERR_BUSY           "E_BUSY"
#define WACKOS_ERR_NOT_SUPPORTED  "E_NOT_SUPPORTED"
#define WACKOS_ERR_INTERNAL       "E_INTERNAL"
#define WACKOS_ERR_CRC            "E_CRC"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/* Инициализация слоя протокола конкретным транспортом. */
void wackos_proto_init(const wackos_transport_t *transport);

/*
 * Задать собственный адрес (для стороны, которая принимает запросы).
 * Кадры с чужим ADDR молча игнорируются — не отвечать на чужое и есть
 * смысл поля ADDR: без этого на шине из нескольких slave все ответят
 * разом. По умолчанию WACKOS_ADDR_SLAVE1.
 */
void wackos_proto_set_address(uint8_t addr);

/* ---- Сборка / разбор кадров (используется и внутри, и в тестах) ---- */

/* Собрать строку кадра (с '\n' на конце) в out. Возвращает длину строки
 * или 0 при переполнении out_size. */
size_t wackos_frame_build(const wackos_frame_t *frame, char *out, size_t out_size);

/* Разобрать строку кадра (без завершающего '\n') в frame.
 * При WACKOS_PARSE_CRC_FAIL frame всё равно заполнен best-effort —
 * нужно, чтобы отправитель ошибки (см. wackos_proto_receive_request)
 * мог ответить E_CRC с тем же ADDR/SEQ, что были в исходном кадре. */
wackos_parse_result_t wackos_frame_parse(const char *line, size_t line_len, wackos_frame_t *frame);

/* ---- API для master: синхронный запрос-ответ ---- */

/*
 * Отправить REQ и дождаться RES с тем же seq.
 * cmd  — имя команды (например "SYS_PING")
 * args — payload запроса или NULL/"" для "-"
 * out_response — куда положить разобранный ответ
 * timeout_ms / retries — см. docs/PROTOCOL.md, там же дефолты
 */
wackos_status_t wackos_proto_request(uint8_t addr, const char *cmd, const char *args,
                                      wackos_frame_t *out_response,
                                      uint32_t timeout_ms, uint8_t retries);

/*
 * Неблокирующая проверка: не пришёл ли асинхронный EVT-кадр
 * (например, пока master ждёт следующего действия пользователя).
 * Возвращает true и заполняет out_event, если событие было.
 */
bool wackos_proto_poll_event(wackos_frame_t *out_event, uint32_t timeout_ms);

/* ---- API для slave: приём запроса, отправка ответа/события ---- */

/* Дождаться следующего REQ (блокирующе, с тайм-аутом). */
bool wackos_proto_receive_request(wackos_frame_t *out_request, uint32_t timeout_ms);

/* Отправить RES OK/ERR с тем же addr/seq, что был в запросе. */
void wackos_proto_send_ok(const wackos_frame_t *request, const char *payload);
void wackos_proto_send_err(const wackos_frame_t *request, const char *error_code);

/* Отправить асинхронное событие (не привязано к конкретному REQ). */
void wackos_proto_send_event(uint8_t addr, const char *event_name, const char *payload);
