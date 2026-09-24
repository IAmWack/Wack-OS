/*
 * Заглушка будущего HID-модуля (эмуляция клавиатуры/мыши). Namespace: HID_*.
 */
#pragma once

#include <stdbool.h>
#include "wackos_protocol.h"

bool module_hid_owns(const char *cmd);
void module_hid_handle(const wackos_frame_t *request);
