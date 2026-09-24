/*
 * Заглушка esp_timer.h: монотонное время в микросекундах.
 * Слой протокола использует только эту функцию.
 */
#pragma once

#include <stdint.h>

int64_t esp_timer_get_time(void);
