/**
 * @file    sim_slave.c
 * @author  IAmWack
 * @brief   Фальшивый slave: та самая плата, которой пока нет на столе.
 *
 * Важно, что здесь НЕ переписана логика slave заново — этот файл лишь
 * крутит цикл приёма, а команды разбирает настоящий slave/main/
 * dispatcher.c со своими модулями-заглушками. Поэтому меню и ответы в
 * симуляторе — ровно те же, что придут с живой платы, и когда она
 * появится, менять в master ничего не придётся.
 *
 * Собирается этот файл вместе со второй копией слоя протокола (см.
 * Makefile, SLAVE_DEFS): master и slave — две независимые стороны со
 * своими счётчиками seq, а слой протокола хранит состояние в
 * статических переменных, так что копии нужны две.
 */
#include "sim.h"

#include <stdio.h>

#include "wackos_protocol.h"
#include "dispatcher.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/*
 * Сколько slave "молчит" после SYS_RESET, изображая перезагрузку.
 * Не выдумано: столько живая плата ESP32 не отвечает по замеру
 * (SW_CPU_RESET, лог загрузчика, старт приложения) — от ответа на
 * SYS_RESET до первого PONG выходило ~600 мс. Держим то же число,
 * чтобы имитация врала не в свою пользу.
 */
#define REBOOT_SILENCE_MS 600

/* ============================================================================
 *  Static state
 * ============================================================================ */

static bool s_connected = true;
static uint32_t s_mute_until = 0;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief Поднять фальшивый slave: своя копия слоя протокола на внутрипроцессном транспорте. */
void sim_slave_init(void)
{
    wackos_proto_init(sim_transport_slave()); /* -> slv_proto_init, см. Makefile */
    /* Как и настоящий slave/main/main.c: на чужие адреса не отвечаем. */
    wackos_proto_set_address(WACKOS_ADDR_SLAVE1);
    s_connected = true;
    s_mute_until = 0;
}

/** @brief Имитировать физическое (от)подключение slave (клавиша O). @param connected false — кадры пропадают в проводе. */
void sim_slave_set_connected(bool connected)
{
    s_connected = connected;
}

/** @brief Подключён ли сейчас фальшивый slave. @return Текущее состояние. */
bool sim_slave_connected(void)
{
    return s_connected;
}

/** @brief "Перезагрузка": включить REBOOT_SILENCE_MS тишины (вызывается esp_restart() из sim_platform.c). */
void sim_slave_reboot(void)
{
    s_mute_until = sim_now_ms() + REBOOT_SILENCE_MS;
    printf("[sim] slave перезагружается (%d мс тишины)\n", REBOOT_SILENCE_MS);
}

/** @brief В процессе ли имитация перезагрузки прямо сейчас. @return true, пока не истекло время тишины. */
bool sim_slave_muted(void)
{
    return sim_now_ms() < s_mute_until;
}

/** @brief Дать slave обработать всё, что прилетело по проводу (разбор через dispatcher_handle()). */
void sim_slave_pump(void)
{
    wackos_frame_t request;

    /* Отключённый/перезагружающийся slave кадры не читает — они просто
     * пропадают в проводе, и master честно уходит в тайм-аут. */
    bool deaf = !s_connected || sim_slave_muted();

    /* Тайм-аут 1 мс, а не 0: слой протокола проверяет дедлайн до чтения,
     * с нулём он бы просто вышел, не заглянув в провод. */
    while (sim_wire_slave_has_frame() && wackos_proto_receive_request(&request, 1)) {
        if (deaf) {
            continue; /* кадр вычитан из провода и выброшен */
        }
        dispatcher_handle(&request);
    }
}
