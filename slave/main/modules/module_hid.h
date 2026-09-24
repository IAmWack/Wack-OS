/**
 * @file    module_hid.h
 * @author  IAmWack
 * @brief   Заглушка (каркас) будущего HID-модуля slave.
 *
 * Эмуляция клавиатуры/мыши, namespace команд HID_*. Реальной логики
 * пока нет — только место в диспетчере и ответ E_NOT_SUPPORTED.
 */
#pragma once

#include <stdbool.h>
#include "wackos_protocol.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Проверить, относится ли команда к namespace HID_.
 * @param   cmd  Имя команды из кадра REQ.
 * @return  true, если cmd начинается с "HID_".
 */
bool module_hid_owns(const char *cmd);

/**
 * @brief   Обработать команду namespace HID_ (пока всегда E_NOT_SUPPORTED).
 * @param   request  Входящий запрос.
 */
void module_hid_handle(const wackos_frame_t *request);
