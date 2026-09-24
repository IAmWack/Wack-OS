/*
 * Общая безоконная обвязка для sim_shot.c и sim_test.c.
 */
#pragma once

#include "sim.h"
#include "encoder.h"
#include "wackos_transport.h"

/* Разобрать -p/-b/-v и поднять транспорт: живая плата либо фальшивый
 * slave в памяти. В out_port кладёт имя порта (NULL — фальшивый). */
const wackos_transport_t *headless_connect(int argc, char **argv,
                                           const char **out_port);

/* Прокрутить интерфейс n кадров (анимация доигрывает). */
void headless_idle(int frames);
/* Одно действие пользователя на энкодере. */
void headless_act(wackos_encoder_event_t ev);
/* Снять следующий кадр в shots/<name>.ppm. */
void headless_shoot(const char *name);
