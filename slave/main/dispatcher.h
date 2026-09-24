/*
 * Слой логики slave: диспетчер команд. Единственное место, которое
 * знает имена команд и умеет на них отвечать; main.c только вызывает
 * dispatcher_handle() на каждый принятый запрос.
 */
#pragma once

#include "wackos_protocol.h"

/* Обработать один разобранный REQ и отправить соответствующий RES. */
void dispatcher_handle(const wackos_frame_t *request);
