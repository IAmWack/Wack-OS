/*
 * "Провод" между master и slave внутри одного процесса.
 *
 * Снаружи это два обычных wackos_transport_t — ровно тот же интерфейс,
 * что даёт настоящий UART. Слой протокола не замечает подмены, поэтому
 * в симуляторе гоняются те же самые текстовые кадры REQ/RES, что потом
 * побегут по проводу между платами (их видно ключом -v).
 *
 * Дополнительно провод умеет то, чего не умеет настоящий: задержку
 * (клавиша L) и обрыв (клавиша O) — чтобы посмотреть, как интерфейс
 * выглядит в состоянии "slave offline", не выдёргивая ничего руками.
 */
#include "sim.h"

#include <stdio.h>
#include <string.h>

#include "wackos_protocol.h"

#define WIRE_SLOTS 8
#define LINE_MAX   (WACKOS_LINE_MAX + 2)

typedef struct {
    char     line[WIRE_SLOTS][LINE_MAX];
    size_t   len[WIRE_SLOTS];
    uint32_t ready_at[WIRE_SLOTS]; /* когда кадр "долетит" (эмуляция задержки) */
    int      head, tail;
} wire_t;

static wire_t s_to_slave;
static wire_t s_to_master;

/* Накопители полукадров: транспорт вправе получить write() по кускам. */
static char s_master_out[LINE_MAX];
static size_t s_master_out_len = 0;
static char s_slave_out[LINE_MAX];
static size_t s_slave_out_len = 0;

static uint32_t s_latency_ms = 6;
bool g_sim_verbose = false;

static void wire_push(wire_t *w, const char *line, size_t len, uint32_t delay_ms)
{
    int next = (w->head + 1) % WIRE_SLOTS;
    if (next == w->tail) {
        return; /* провод забит — кадр теряется, как и в жизни при переполнении */
    }
    if (len > LINE_MAX) {
        len = LINE_MAX;
    }
    memcpy(w->line[w->head], line, len);
    w->len[w->head] = len;
    w->ready_at[w->head] = sim_now_ms() + delay_ms;
    w->head = next;
}

static int wire_pop(wire_t *w, uint8_t *buf, size_t max_len, bool respect_delay)
{
    if (w->head == w->tail) {
        return 0;
    }
    if (respect_delay && sim_now_ms() < w->ready_at[w->tail]) {
        return 0;
    }
    size_t len = w->len[w->tail];
    if (len > max_len) {
        len = max_len;
    }
    memcpy(buf, w->line[w->tail], len);
    w->tail = (w->tail + 1) % WIRE_SLOTS;
    return (int)len;
}

/* Собрать байты в строку и, когда придёт '\n', отправить её в провод. */
static int feed(char *acc, size_t *acc_len, const uint8_t *data, size_t len,
                wire_t *dest, const char *tag)
{
    for (size_t i = 0; i < len; i++) {
        if (*acc_len < LINE_MAX - 1) {
            acc[(*acc_len)++] = (char)data[i];
        }
        if (data[i] != '\n') {
            continue;
        }
        if (g_sim_verbose) {
            acc[*acc_len - 1] = '\0';
            printf("[%s] %s\n", tag, acc);
            acc[*acc_len - 1] = '\n';
        }
        wire_push(dest, acc, *acc_len, s_latency_ms);
        *acc_len = 0;
    }
    return (int)len;
}

/* ---- сторона master ---- */

static int master_write(const uint8_t *data, size_t len)
{
    int n = feed(s_master_out, &s_master_out_len, data, len, &s_to_slave, "REQ ->");
    /*
     * Провод односторонне "прокачивается" здесь же: slave в симуляторе
     * не отдельная задача, а функция, которую дёргают, когда для неё
     * появилась работа.
     */
    sim_slave_pump();
    return n;
}

static int master_read_line(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    uint32_t deadline = sim_now_ms() + timeout_ms;
    for (;;) {
        /* Даём slave разобрать кадр, который уже "долетел" по проводу. */
        sim_slave_pump();
        int n = wire_pop(&s_to_master, buf, max_len, true);
        if (n > 0) {
            return n;
        }
        if (sim_now_ms() >= deadline) {
            return 0;
        }
        /* Ждём мелкими шагами, попутно давая окну обработать события —
         * иначе при "slave offline" окно замирало бы на все 450 мс. */
        sim_window_pump(2);
        if (!sim_window_alive()) {
            return 0;
        }
    }
}

/* ---- сторона slave ---- */

static int slave_write(const uint8_t *data, size_t len)
{
    return feed(s_slave_out, &s_slave_out_len, data, len, &s_to_master, "RES <-");
}

static int slave_read_line(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    (void)timeout_ms; /* slave в симуляторе никогда не ждёт: его зовут
                       * ровно тогда, когда кадр уже в проводе */
    return wire_pop(&s_to_slave, buf, max_len, true);
}

static const wackos_transport_t s_master_transport = {
    .write = master_write,
    .read_line = master_read_line,
};

static const wackos_transport_t s_slave_transport = {
    .write = slave_write,
    .read_line = slave_read_line,
};

bool sim_wire_slave_has_frame(void)
{
    return s_to_slave.head != s_to_slave.tail &&
           sim_now_ms() >= s_to_slave.ready_at[s_to_slave.tail];
}

const wackos_transport_t *sim_transport_master(void)
{
    return &s_master_transport;
}

const wackos_transport_t *sim_transport_slave(void)
{
    return &s_slave_transport;
}

void sim_transport_set_latency(uint32_t ms)
{
    s_latency_ms = ms;
}

uint32_t sim_transport_latency(void)
{
    return s_latency_ms;
}
