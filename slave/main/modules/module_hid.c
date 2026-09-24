/**
 * @file    module_hid.c
 * @author  IAmWack
 * @brief   Заглушка (каркас) будущего HID-модуля slave. Namespace: HID_*.
 *
 * Реальной реализации нет — задача файла только застолбить namespace
 * в диспетчере и корректно отвечать E_NOT_SUPPORTED.
 */
#include "module_hid.h"
#include <string.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define NAMESPACE_PREFIX "HID_"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Проверить, относится ли команда к namespace HID_.
 * @param   cmd  Имя команды из кадра REQ.
 * @return  true, если cmd начинается с "HID_".
 */
bool module_hid_owns(const char *cmd)
{
    return strncmp(cmd, NAMESPACE_PREFIX, strlen(NAMESPACE_PREFIX)) == 0;
}

/**
 * @brief   Обработать команду namespace HID_.
 * @param   request  Входящий запрос.
 */
void module_hid_handle(const wackos_frame_t *request)
{
    /* TODO: HID_PRESS, HID_TYPE и т.п. Пока заглушка. */
    wackos_proto_send_err(request, WACKOS_ERR_NOT_SUPPORTED);
}
