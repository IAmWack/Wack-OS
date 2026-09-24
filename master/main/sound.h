/*
 * Звук. Пищалки на макете ещё нет, поэтому слой существует, а железо за
 * ним — опциональное: если PIN_BUZZER не задан (-1), все вызовы просто
 * ничего не делают, и ни один экран об этом не знает.
 *
 * Сделано слоем сразу, чтобы потом не выискивать по всему UI места, где
 * "тут надо пикнуть".
 */
#pragma once

#include <stdint.h>

typedef enum {
    SOUND_TICK = 0,  /* щелчок энкодера */
    SOUND_SELECT,    /* вход в пункт */
    SOUND_BACK,      /* возврат назад */
    SOUND_ERROR,     /* slave не ответил */
    SOUND_BOOT,      /* мелодия загрузки */
} sound_id_t;

void sound_init(void);
void sound_set_volume(uint8_t level); /* 0..10 */
void sound_play(sound_id_t id);
