/*
 * Реализация sound.h для ноутбука. Пищалки нет ни на макете, ни в
 * окне — поэтому звуки просто печатаются в терминал. Этого достаточно,
 * чтобы видеть, в каких местах интерфейс будет пикать, и не забыть
 * убрать лишнее до того, как пищалка появится.
 */
#include "sound.h"

#include <stdio.h>

static uint8_t s_volume = 6;

void sound_init(void)
{
    printf("[sim] звук: пищалки нет, играю в терминал\n");
}

void sound_set_volume(uint8_t level)
{
    s_volume = (level > 10) ? 10 : level;
}

void sound_play(sound_id_t id)
{
    static const char *NAMES[] = { "tick", "select", "back", "error", "boot" };
    if (s_volume == 0 || id == SOUND_TICK) {
        return; /* тиканье энкодера завалило бы весь лог */
    }
    printf("[sim] ♪ %s\n", NAMES[id]);
}
