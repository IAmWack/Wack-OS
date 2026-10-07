/**
 * @file    venti.c
 * @author  IAmWack
 * @brief   Отрисовка и состояние персонажа-маскота Венти (третий маскот).
 *
 * В отличие от Астольфо (astolfo.c) и Разора (razor.c) — не процедурный
 * рисунок, а растровые портреты с microSD (portrait_sd.h), по одному на
 * настроение. Источники — не референс для процедурной отрисовки, а
 * кадры из готового арт-листа, который прислал пользователь; он явно
 * попросил именно этот арт, не интерпретацию. Используются растрово
 * ПОВСЮДУ, включая мелкий бюст в углу меню (venti_draw()) — обычно для
 * такого размера растр "мылится", но этот арт — простой чиби-стиль с
 * толстым контуром, масштабируется вниз нормально. Привязан к теме
 * "мята" (theme.c, THEME_MASCOT_VENTI).
 *
 * Стихия — ветер: реплики venti_line() про ветер/музыку, не про
 * slave/протокол (в master_lite это не актуально).
 */
#include "venti.h"
#include "wackos_gfx.h"
#include "portrait_sd.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/* Сколько держится "реактивное" настроение, прежде чем вернуться в idle. */
#define MOOD_HOLD_MS   2200
/* Через сколько простоя Венти засыпает. */
#define SLEEP_AFTER_MS 60000

/* ============================================================================
 *  Static state
 * ============================================================================ */

static venti_mood_t s_mood = VENTI_IDLE;
static uint32_t s_mood_at = 0;
static uint32_t s_activity_at = 0;
static uint32_t s_now = 0;

/* ============================================================================
 *  Public API — состояние (настроение, активность)
 * ============================================================================ */

/** @brief  Установить текущее настроение персонажа. */
void venti_set_mood(venti_mood_t mood)
{
    s_mood = mood;
    s_mood_at = s_now;
}

/** @brief  Текущее настроение персонажа. */
venti_mood_t venti_mood(void)
{
    return s_mood;
}

/** @brief  Отметить активность пользователя — сбросить таймер засыпания. */
void venti_note_activity(void)
{
    s_activity_at = s_now;
    if (s_mood == VENTI_SLEEP) {
        venti_set_mood(VENTI_IDLE);
    }
}

/**
 * @brief   Раз в кадр: обновить настроение по прошедшему времени.
 * @param   now_ms  Текущее время в миллисекундах (монотонное).
 */
void venti_tick(uint32_t now_ms)
{
    s_now = now_ms;

    if ((s_mood == VENTI_HAPPY || s_mood == VENTI_SAD) &&
        now_ms - s_mood_at > MOOD_HOLD_MS) {
        venti_set_mood(VENTI_IDLE);
    }

    if (s_mood == VENTI_IDLE && now_ms - s_activity_at > SLEEP_AFTER_MS) {
        venti_set_mood(VENTI_SLEEP);
    }
}

/** @brief  Реплика персонажа под текущее настроение — для облачка в меню. */
const char *venti_line(void)
{
    switch (s_mood) {
        case VENTI_HAPPY:    return "та-да!";
        case VENTI_SAD:      return "эх...";
        case VENTI_THINKING: return "дай подумать...";
        case VENTI_SLEEP:    return "zzz...";
        default:             return "дую по ветру";
    }
}

/* ============================================================================
 *  Public API — отрисовка
 * ============================================================================ */

/** @brief  Имя настроения — для portrait_sd_load("venti", ...). */
static const char *mood_name(void)
{
    switch (s_mood) {
        case VENTI_HAPPY:    return "happy";
        case VENTI_SAD:      return "sad";
        case VENTI_THINKING: return "thinking";
        case VENTI_SLEEP:    return "sleep";
        default:             return "idle";
    }
}

/**
 * @brief   Нарисовать бюст Венти (растровый портрет, масштаб по h).
 * @param   cx  Центр по X.
 * @param   y   Верх фигуры по Y.
 * @param   h   Высота головы — масштаб (см. VENTI_BUST_H), не время анимации.
 * @param   t   Не используется — растр не дышит/не моргает, в отличие
 *              от процедурных Астольфо/Разора.
 */
void venti_draw(int cx, int y, int h, uint32_t t)
{
    (void)t;
    const uint16_t *px = portrait_sd_load("venti", mood_name());
    if (!px) {
        return; /* нет карты/файла — молча пропустить, не рисовать мусор */
    }
    int dst_h = VENTI_BUST_H(h);
    int dst_w = dst_h * PORTRAIT_SD_W / PORTRAIT_SD_H; /* сохранить пропорции исходника */
    gfx_image_scaled(cx, y, dst_w, dst_h, px, PORTRAIT_SD_W, PORTRAIT_SD_H, 255);
}

/**
 * @brief   Нарисовать растровый портрет настроения крупнее.
 * @param   cx, top_y     Центр по X и верх по Y.
 * @param   dst_w, dst_h  Размер на экране.
 */
void venti_draw_portrait(int cx, int top_y, int dst_w, int dst_h)
{
    const uint16_t *px = portrait_sd_load("venti", mood_name());
    if (!px) {
        return;
    }
    gfx_image_scaled(cx, top_y, dst_w, dst_h, px, PORTRAIT_SD_W, PORTRAIT_SD_H, 255);
}
