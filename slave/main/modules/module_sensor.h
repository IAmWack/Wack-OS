/*
 * Заглушка будущего модуля датчиков. Namespace: SENSOR_*.
 */
#pragma once

#include <stdbool.h>
#include "wackos_protocol.h"

bool module_sensor_owns(const char *cmd);
void module_sensor_handle(const wackos_frame_t *request);
