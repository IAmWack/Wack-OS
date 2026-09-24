/**
 * @file    module_radio.h
 * @author  IAmWack
 * @brief   Заглушка (каркас) будущего радио-модуля slave.
 *
 * Namespace команд: RADIO_*. Реальной логики нет — задача этого файла
 * только застолбить место в диспетчере и корректно отвечать "команда
 * пока не поддерживается".
 */
#pragma once

#include <stdbool.h>
#include "wackos_protocol.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Проверить, относится ли команда к namespace RADIO_.
 * @param   cmd  Имя команды из кадра REQ.
 * @return  true, если cmd относится к namespace RADIO_ (независимо от
 *          того, реализована ли конкретная команда).
 */
bool module_radio_owns(const char *cmd);

/**
 * @brief   Обработать команду namespace RADIO_ (пока всегда E_NOT_SUPPORTED).
 * @param   request  Входящий запрос.
 */
void module_radio_handle(const wackos_frame_t *request);
