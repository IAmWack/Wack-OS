/**
 * @file    module_sensor.c
 * @author  IAmWack
 * @brief   Заглушка (каркас) будущего модуля датчиков slave. Namespace: SENSOR_*.
 *
 * Реальной реализации нет — задача файла только застолбить namespace
 * в диспетчере и корректно отвечать E_NOT_SUPPORTED.
 */
#include "module_sensor.h"
#include <string.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define NAMESPACE_PREFIX "SENSOR_"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Проверить, относится ли команда к namespace SENSOR_.
 * @param   cmd  Имя команды из кадра REQ.
 * @return  true, если cmd начинается с "SENSOR_".
 */
bool module_sensor_owns(const char *cmd)
{
    return strncmp(cmd, NAMESPACE_PREFIX, strlen(NAMESPACE_PREFIX)) == 0;
}

/**
 * @brief   Обработать команду namespace SENSOR_.
 * @param   request  Входящий запрос.
 */
void module_sensor_handle(const wackos_frame_t *request)
{
    /* TODO: SENSOR_READ и т.п. Пока заглушка. */
    wackos_proto_send_err(request, WACKOS_ERR_NOT_SUPPORTED);
}
