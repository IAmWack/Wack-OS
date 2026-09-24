/**
 * @file    module_radio.c
 * @author  IAmWack
 * @brief   Заглушка (каркас) будущего радио-модуля slave. Namespace: RADIO_*.
 *
 * Реальной реализации нет — задача файла только застолбить namespace
 * в диспетчере и корректно отвечать E_NOT_SUPPORTED.
 */
#include "module_radio.h"
#include <string.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define NAMESPACE_PREFIX "RADIO_"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Проверить, относится ли команда к namespace RADIO_.
 * @param   cmd  Имя команды из кадра REQ.
 * @return  true, если cmd начинается с "RADIO_".
 */
bool module_radio_owns(const char *cmd)
{
    return strncmp(cmd, NAMESPACE_PREFIX, strlen(NAMESPACE_PREFIX)) == 0;
}

/**
 * @brief   Обработать команду namespace RADIO_.
 * @param   request  Входящий запрос.
 *
 * TODO: сюда встанет реальная реализация (RADIO_SCAN, RADIO_TX,
 * RADIO_RX и т.п.), см. пример долгих операций в docs/PROTOCOL.md
 * (REQ -> RES OK ACCEPTED -> поток EVT с результатами). Пока
 * namespace существует, но ни одна команда не реализована.
 */
void module_radio_handle(const wackos_frame_t *request)
{
    wackos_proto_send_err(request, WACKOS_ERR_NOT_SUPPORTED);
}
