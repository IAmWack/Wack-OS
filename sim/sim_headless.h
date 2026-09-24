/**
 * @file    sim_headless.h
 * @author  IAmWack
 * @brief   Интерфейс общей безоконной обвязки для sim_shot.c и sim_test.c.
 *
 * Реализация — sim_headless.c: подменяет display.h/encoder.h так, что
 * кадры уходят в файл, а события энкодера — из скрипта, без окна X11.
 */
#pragma once

#include "sim.h"
#include "encoder.h"
#include "wackos_transport.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Разобрать -p/-b/-v и поднять транспорт: живая плата либо фальшивый slave.
 * @param   argc      Как в main().
 * @param   argv      Как в main().
 * @param   out_port  Куда положить имя порта (NULL — фальшивый slave в памяти).
 * @return  Транспорт для wackos_proto_init(), либо NULL при ошибке.
 */
const wackos_transport_t *headless_connect(int argc, char **argv,
                                           const char **out_port);

/** @brief Прокрутить интерфейс n кадров (анимация доигрывает). @param frames Число кадров. */
void headless_idle(int frames);
/** @brief Одно действие пользователя на энкодере. @param ev Событие энкодера. */
void headless_act(wackos_encoder_event_t ev);
/** @brief Снять следующий кадр в shots/<name>.ppm. @param name Имя файла без расширения. */
void headless_shoot(const char *name);
