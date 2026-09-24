/*
 * Мелкие связки между экранами: экран модуля должен знать, какой
 * модуль выбрали в списке. Всё остальное экраны узнают из app_logic.
 */
#pragma once

#include <stddef.h>

void   screen_module_set(size_t index);
size_t screen_module_get(void);
