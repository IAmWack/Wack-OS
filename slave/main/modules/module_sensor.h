/**
 * @file    module_sensor.h
 * @author  IAmWack
 * @brief   Заглушка (каркас) будущего модуля датчиков slave. Namespace: SENSOR_*.
 */
#pragma once

#include <stdbool.h>
#include "wackos_protocol.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Проверить, относится ли команда к namespace SENSOR_.
 * @param   cmd  Имя команды из кадра REQ.
 * @return  true, если cmd начинается с "SENSOR_".
 */
bool module_sensor_owns(const char *cmd);

/**
 * @brief   Обработать команду namespace SENSOR_ (пока всегда E_NOT_SUPPORTED).
 * @param   request  Входящий запрос.
 */
void module_sensor_handle(const wackos_frame_t *request);
