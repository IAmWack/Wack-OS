/**
 * @file    sim_serial.c
 * @author  IAmWack
 * @brief   Настоящий UART вместо петли в памяти — sim_transport.c для живой платы.
 *
 * Тот же wackos_transport_t, но за ним USB-переходник и живая плата
 * slave. Ради этого всё и разделено на слои — UI, логика и протокол
 * не замечают подмены, меняется только то, куда уходят байты. Запуск:
 *
 *     ./wackos-sim -p /dev/ttyUSB0
 */
#include "sim.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "wackos_protocol.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define ACC_MAX (WACKOS_LINE_MAX * 2)

/* ============================================================================
 *  Static state
 * ============================================================================ */

static int s_fd = -1;
static char s_port[64] = "";

/* Недочитанный хвост строки между вызовами read_line(). */
static char s_acc[ACC_MAX];
static size_t s_acc_len = 0;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief true — симулятор говорит с живой платой, а не с фальшивым slave. @return Открыт ли serial-порт. */
bool sim_serial_active(void)
{
    return s_fd >= 0;
}

/** @brief Путь открытого порта. @return Строка порта либо пустая строка, если он не открыт. */
const char *sim_serial_port(void)
{
    return s_port;
}

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/**
 * @brief   Записать байты в serial-порт (реализация wackos_transport_t::write).
 * @param   data  Буфер для отправки.
 * @param   len   Длина буфера.
 * @return  Число отправленных байт, либо -1, если порт не открыт.
 */
static int serial_write(const uint8_t *data, size_t len)
{
    if (s_fd < 0) {
        return -1;
    }
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(s_fd, data + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }
    if (g_sim_verbose) {
        printf("[REQ ->] %.*s", (int)len, (const char *)data);
        fflush(stdout);
    }
    return (int)sent;
}

/**
 * @brief   Отдать из накопителя первую целую строку, если она там есть.
 * @param   buf      Куда скопировать строку.
 * @param   max_len  Размер buf.
 * @return  Длина скопированного (0, если целой строки ещё нет).
 */
static int take_line(uint8_t *buf, size_t max_len)
{
    char *nl = memchr(s_acc, '\n', s_acc_len);
    if (!nl) {
        return 0;
    }
    size_t line_len = (size_t)(nl - s_acc) + 1;
    size_t copy = (line_len > max_len) ? max_len : line_len;
    memcpy(buf, s_acc, copy);

    memmove(s_acc, s_acc + line_len, s_acc_len - line_len);
    s_acc_len -= line_len;

    if (g_sim_verbose) {
        printf("[RES <-] %.*s", (int)copy, (const char *)buf);
        fflush(stdout);
    }
    return (int)copy;
}

/** @brief Забрать из порта всё, что уже пришло, не блокируясь. */
static void drain_fd(void)
{
    for (;;) {
        char tmp[256];
        ssize_t n = read(s_fd, tmp, sizeof(tmp));
        if (n <= 0) {
            return; /* EAGAIN — данных больше нет */
        }
        size_t space = ACC_MAX - s_acc_len;
        size_t copy = ((size_t)n > space) ? space : (size_t)n;
        memcpy(s_acc + s_acc_len, tmp, copy);
        s_acc_len += copy;
        if (s_acc_len >= ACC_MAX) {
            /* Мусор без перевода строки (например, лог загрузчика ESP
             * на той же скорости) — выкидываем, чтобы не захлебнуться. */
            s_acc_len = 0;
        }
    }
}

/**
 * @brief   Прочитать одну строку из serial-порта (реализация wackos_transport_t::read_line).
 * @param   buf         Куда скопировать строку.
 * @param   max_len     Размер buf.
 * @param   timeout_ms  Максимальное время ожидания.
 * @return  Длина строки, 0 по тайм-ауту, -1, если порт не открыт.
 */
static int serial_read_line(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    if (s_fd < 0) {
        return -1;
    }

    uint32_t deadline = sim_now_ms() + timeout_ms;
    for (;;) {
        /*
         * Сначала ВСЕГДА вычитываем порт, и только потом смотрим на
         * тайм-аут. Иначе вызов с timeout_ms = 0 (так опрашиваются
         * события энкодера) возвращался бы, ни разу не заглянув в
         * порт, и кадры EVT лежали бы в буфере ОС вечно.
         */
        int ready = take_line(buf, max_len);
        if (ready > 0) {
            return ready;
        }
        drain_fd();
        ready = take_line(buf, max_len);
        if (ready > 0) {
            return ready;
        }

        uint32_t now = sim_now_ms();
        if (now >= deadline) {
            return 0;
        }

        /*
         * Ждём байты, но не дольше 5 мс за раз: между ожиданиями нужно
         * прокрутить окно, иначе оно замирает, пока master ждёт ответа
         * (а при отключённой плате это все 450 мс ретраев).
         */
        uint32_t left = deadline - now;
        struct pollfd pfd = { .fd = s_fd, .events = POLLIN };
        poll(&pfd, 1, (int)(left > 5 ? 5 : left));

        sim_window_pump(0);
        if (!sim_window_alive()) {
            return 0;
        }
    }
}

static const wackos_transport_t s_transport = {
    .write = serial_write,
    .read_line = serial_read_line,
};

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Открыть serial-порт и настроить его под протокол Wack-OS.
 * @param   port  Путь устройства (например /dev/ttyUSB0).
 * @param   baud  Скорость порта.
 * @return  Транспорт для wackos_proto_init(), либо NULL при ошибке.
 */
const wackos_transport_t *sim_serial_open(const char *port, int baud)
{
    s_fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (s_fd < 0) {
        fprintf(stderr, "не открылся порт %s: %s\n", port, strerror(errno));
        /* Плата любит переехать с ttyUSB0 на ttyUSB1 после перетыкания —
         * подскажем, что вообще есть в системе. */
        fprintf(stderr, "доступные порты:\n");
        if (system("ls -1 /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | sed 's/^/  /'") != 0) {
            fprintf(stderr, "  (ни одного)\n");
        }
        return NULL;
    }

    /*
     * Снимаем DTR/RTS: на платах с автосбросом они заведены на EN/IO0,
     * и удерживать их — значит держать плату в сбросе. Сам факт
     * открытия порта плату всё же перезагружает, поэтому ниже пауза.
     */
    int bits = TIOCM_DTR | TIOCM_RTS;
    ioctl(s_fd, TIOCMBIC, &bits);

    struct termios tio;
    if (tcgetattr(s_fd, &tio) != 0) {
        fprintf(stderr, "tcgetattr(%s): %s\n", port, strerror(errno));
        close(s_fd);
        s_fd = -1;
        return NULL;
    }

    cfmakeraw(&tio);                 /* никаких преобразований: протокол текстовый,
                                      * но байты должны доходить как есть */
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~CRTSCTS;         /* аппаратного потока нет */
    tio.c_cflag &= ~HUPCL;           /* не ронять DTR при закрытии */
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~PARENB;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    speed_t speed;
    switch (baud) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 921600: speed = B921600; break;
        default:     speed = B115200; break;
    }
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    if (tcsetattr(s_fd, TCSANOW, &tio) != 0) {
        fprintf(stderr, "tcsetattr(%s): %s\n", port, strerror(errno));
        close(s_fd);
        s_fd = -1;
        return NULL;
    }

    snprintf(s_port, sizeof(s_port), "%s", port);

    /* Даём плате подняться после сброса и выбрасываем её загрузочный лог. */
    sim_sleep_ms(600);
    tcflush(s_fd, TCIFLUSH);
    s_acc_len = 0;

    printf("[sim] порт %s, %d бод — говорю с живой платой\n", port, baud);
    return &s_transport;
}

/** @brief Закрыть открытый ранее serial-порт. */
void sim_serial_close(void)
{
    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
    }
}
