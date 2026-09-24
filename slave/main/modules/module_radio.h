/*
 * Заглушка будущего радио-модуля. Namespace команд: RADIO_*.
 * Реальной логики нет — задача этого файла только застолбить место
 * в диспетчере и корректно отвечать "команда пока не поддерживается".
 */
#pragma once

#include <stdbool.h>
#include "wackos_protocol.h"

/* true, если cmd относится к namespace RADIO_ (независимо от того,
 * реализована ли конкретная команда). */
bool module_radio_owns(const char *cmd);

void module_radio_handle(const wackos_frame_t *request);
