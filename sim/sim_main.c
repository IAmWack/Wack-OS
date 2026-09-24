/**
 * @file    sim_main.c
 * @author  IAmWack
 * @brief   Точка входа оконного симулятора (./wackos-sim).
 *
 * Повторяет master/main/main.c один в один: транспорт -> протокол ->
 * интерфейс -> логика, и тот же главный цикл. Отличий ровно два —
 * вместо UART петля в памяти с фальшивым slave (или живой serial-порт),
 * и цикл заканчивается, когда закрывают окно.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sim.h"

#include "wackos_protocol.h"
#include "ui.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Разбор аргументов командной строки, запуск окна и главного цикла.
 * @param   argc  Число аргументов.
 * @param   argv  Аргументы: -s масштаб, -p порт, -b скорость, -v verbose, -h справка.
 * @return  0 при штатном выходе, 1 при ошибке открытия окна/порта.
 */
int main(int argc, char **argv)
{
    int scale = 0;            /* 0 = подобрать под высоту экрана */
    const char *port = NULL;  /* NULL = фальшивый slave в памяти */
    int baud = 115200;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            g_sim_verbose = true;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            scale = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = argv[++i];
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            baud = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Симулятор экрана Wack-OS\n"
                   "  -s N   масштаб пикселя (1..4), по умолчанию — под высоту экрана\n"
                   "  -p ПОРТ  говорить с живой платой slave (напр. /dev/ttyUSB0)\n"
                   "           вместо фальшивого slave в памяти\n"
                   "  -b СКОР  скорость порта, по умолчанию 115200\n"
                   "  -v     печатать кадры протокола REQ/RES в терминал\n"
                   "\n"
                   "Управление: колесо мыши или стрелки — крутить энкодер,\n"
                   "Enter/ЛКМ — нажать, Backspace/ПКМ — длинное нажатие (назад),\n"
                   "O — отключить slave, L — залипающий провод (только с\n"
                   "фальшивым slave), S — снимок экрана в .ppm, Q — выход.\n");
            return 0;
        }
    }

    if (!sim_window_open(scale)) {
        return 1;
    }

    /*
     * Либо живая плата по USB, либо фальшивый slave в памяти. Выше по
     * стеку разницы нет вообще: и протокол, и логика, и UI видят один и
     * тот же wackos_transport_t.
     */
    const wackos_transport_t *transport = NULL;
    if (port) {
        transport = sim_serial_open(port, baud);
        if (!transport) {
            sim_window_close();
            return 1;
        }
    } else {
        sim_slave_init();
        transport = sim_transport_master();
    }
    wackos_proto_init(transport);

    ui_init();

    while (sim_window_alive()) {
        ui_tick();
    }

    sim_serial_close();
    sim_window_close();
    return 0;
}
