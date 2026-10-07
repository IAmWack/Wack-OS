/**
 * @file    ui.c
 * @author  IAmWack
 * @brief   Слой интерфейса master: роутер экранов, навигация, "хром" кадра.
 *
 * master_lite: автономная ветка без связи со slave — знает про меню и
 * рисование, никакого app_logic/протокола тут нет. Сами экраны живут в
 * master_lite/main/screens/, каждый реализует ui_screen_t (ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "theme.h"
#include "display.h"
#include "encoder.h"
#include "mascot.h"
#include "settings.h"
#include "sound.h"
#include "battery.h"
#include "wackos_gfx.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ui";

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/* Кадр перерисовывается не чаще этого — иначе SPI к ST7789 будет занят
 * только отрисовкой (полный кадр 172x320x16 бит на 40 МГц ≈ 22 мс). */
#define FRAME_INTERVAL_MS 50

/* Глубина стека экранов: меню -> модули -> модуль — больше и не нужно. */
#define NAV_DEPTH 6

/* Сколько висит всплывающее сообщение (см. draw_toast()/ui_toast()). */
#define TOAST_DURATION_MS 2200

/* ============================================================================
 *  Static state
 * ============================================================================ */

static const ui_screen_t *const SCREENS[SCREEN_COUNT] = {
    [SCREEN_BOOT]     = &screen_boot,
    [SCREEN_HOME]     = &screen_home,
    [SCREEN_MENU]     = &screen_menu,
    [SCREEN_SETTINGS] = &screen_settings,
    [SCREEN_ABOUT]    = &screen_about,
    [SCREEN_FILES]    = &screen_files,
    [SCREEN_CLOCK]    = &screen_clock,
    [SCREEN_REMOTE]   = &screen_remote,
    [SCREEN_REMOTE_KEYBOARD] = &screen_remote_keyboard,
    [SCREEN_REMOTE_MOUSE]    = &screen_remote_mouse,
    [SCREEN_BLE_SCAN]        = &screen_ble_scan,
};

static screen_id_t s_stack[NAV_DEPTH] = { SCREEN_BOOT };
static int s_depth = 0;

static char s_toast[128] = "";
static uint32_t s_toast_until_ms = 0; /* 0 — тоста не было/уже погас, см. draw_toast()/ui_toast() */

static uint32_t s_last_turn_ms = 0;

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/** @brief  Текущее время в миллисекундах (монотонное). */
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/** @brief  Указатель на текущий экран (вершина стека навигации). */
static const ui_screen_t *current(void)
{
    return SCREENS[s_stack[s_depth]];
}

/* ------------------------------------------------------------------ */
/* Обои и общий "хром" экрана                                          */
/* ------------------------------------------------------------------ */

/* Декоративные сердечки: летят снизу вверх и зацикливаются. */
static const struct { int16_t x; uint8_t size; uint8_t speed; uint8_t phase; } DECO_HEARTS[] = {
    {  18,  9, 14,   0 },
    { 148, 12, 10,  40 },
    {  56,  7, 18,  90 },
    { 120,  8, 12, 150 },
    {  86, 11,  8, 200 },
    {  34, 10, 16, 250 },
};
#define DECO_HEART_COUNT (sizeof(DECO_HEARTS) / sizeof(DECO_HEARTS[0]))

/* Мерцающие искорки — стоят на месте, но дышат прозрачностью. */
static const struct { int16_t x, y; uint8_t r; uint16_t period; } DECO_SPARKS[] = {
    {  26,  70, 4, 1700 },
    { 150, 118, 3, 2300 },
    {  14, 196, 3, 1300 },
    { 158, 232, 4, 1900 },
    {  40, 286, 3, 2600 },
    { 130,  38, 3, 2100 },
};
#define DECO_SPARK_COUNT (sizeof(DECO_SPARKS) / sizeof(DECO_SPARKS[0]))

/** @brief  Плавная пульсация [0..1] с заданным периодом и фазой (синус). */
static float pulse(uint32_t t_ms, float period_ms, float phase)
{
    return 0.5f + 0.5f * sinf((float)t_ms / period_ms * 6.2831853f + phase);
}

/** @brief  Нарисовать фон: градиент, ткань, декоративные сердечки и искорки. */
static void draw_wallpaper(uint32_t t)
{
    gfx_gradient_v(0, GFX_H / 2, TH->bg_top, TH->bg_mid);
    gfx_gradient_v(GFX_H / 2, GFX_H, TH->bg_mid, TH->bg_bot);

    /* Косые полоски: очень бледные, дают "ткань" вместо плоской заливки. */
    uint16_t *fb = gfx_framebuffer();
    for (int y = 0; y < GFX_H; y++) {
        for (int x = 0; x < GFX_W; x++) {
            if (((x + y) / 13) % 2 == 0) {
                fb[y * GFX_W + x] = gfx_lighten(fb[y * GFX_W + x], 26);
            }
        }
    }

    gfx_circle(140,  84, 34, TH->card, 26);
    gfx_circle( 24, 168, 26, TH->card, 22);
    gfx_circle(110, 250, 40, TH->card, 20);

    for (size_t i = 0; i < DECO_HEART_COUNT; i++) {
        int span = GFX_H + 40;
        int y = GFX_H + 20 -
                (int)((t / 40 * DECO_HEARTS[i].speed / 10 + DECO_HEARTS[i].phase * 3) % span);
        float sway = sinf((float)t / 900.0f + i) * 4.0f;
        gfx_heart(DECO_HEARTS[i].x + (int)sway, y, DECO_HEARTS[i].size, TH->deco, 70);
    }

    for (size_t i = 0; i < DECO_SPARK_COUNT; i++) {
        float p = pulse(t, DECO_SPARKS[i].period, (float)i);
        gfx_sparkle(DECO_SPARKS[i].x, DECO_SPARKS[i].y, DECO_SPARKS[i].r,
                    TH->card, (uint8_t)(60 + p * 170));
    }
}

/**
 * @brief   Нарисовать шапку: карточка, заголовок/логотип, индикатор связи.
 */
static void draw_header(uint32_t t)
{
    const int card_x = 6, card_y = HEADER_Y, card_w = GFX_W - 12, card_h = HEADER_H;

    /* Кошачьи ушки — рисуем до карточки, чтобы она перекрыла их основание. */
    gfx_cat_ear(card_x + 16, card_y - EAR_H + 2, 26, EAR_H, false,
                TH->card, TH->accent_soft, 255);
    gfx_cat_ear(card_x + card_w - 42, card_y - EAR_H + 2, 26, EAR_H, true,
                TH->card, TH->accent_soft, 255);

    gfx_round_rect(card_x + 1, card_y + 3, card_w, card_h, 16, TH->shadow, 35);
    gfx_round_rect_gradient(card_x, card_y, card_w, card_h, 16,
                            TH->card, gfx_mix(TH->card, TH->accent_soft, 60), 240);
    gfx_round_rect_outline(card_x, card_y, card_w, card_h, 16, 2, TH->card_edge, 210);

    if (s_stack[s_depth] == SCREEN_HOME || s_stack[s_depth] == SCREEN_BOOT) {
        /* Главный экран — фирменная надпись с сердечком. */
        const wackos_font_t *f = &wackos_font_title;
        int w1 = gfx_text_width(f, "wack");
        int w2 = gfx_text_width(f, "-os");
        int x = card_x + (card_w - (w1 + w2 + 16)) / 2 + 4;
        int y = card_y + (card_h - f->line_height) / 2;

        float beat = pulse(t, 1100.0f, 0.0f);
        gfx_heart(x - 9, y + f->line_height / 2 - 1, 11 + (int)(beat * 3.0f), TH->accent, 255);
        gfx_text_shadow(f, x + 8, y, "wack", TH->accent, TH->shadow, 255);
        gfx_text_shadow(f, x + 8 + w1, y, "-os", TH->text, TH->shadow, 255);

        /* Заряд батареи — только если физически подключён делитель (см.
         * battery.h); без него нечего показывать, вранья лучше не рисовать.
         * Место — по центру, в промежутке между ушками над надписью. */
        if (battery_present()) {
            char pct[8];
            snprintf(pct, sizeof(pct), "%u%%", (unsigned)battery_percent());
            const wackos_font_t *fs = &wackos_font_small;
            gfx_text_center(fs, card_x + card_w / 2, card_y - EAR_H + 2, pct, TH->text_dim, 220);
        }
    } else {
        /* Вложенный экран: стрелка "назад" + заголовок. */
        const wackos_font_t *f = &wackos_font_body;
        int y = card_y + (card_h - f->line_height) / 2;
        gfx_triangle(card_x + 12, card_y + card_h / 2,
                     card_x + 21, card_y + card_h / 2 - 7,
                     card_x + 21, card_y + card_h / 2 + 7, TH->accent, 235);
        gfx_text_ellipsis(f, card_x + 28, y, current()->title, card_w - 52, TH->text, 255);
    }
}

/**
 * @brief   Короткое всплывающее сообщение (см. ui_toast()) — плавает над
 *          подсказками, видно на любом экране, само гаснет по таймеру
 *          (см. s_toast_until_ms). Раньше жило только в пузыре у маскота
 *          в меню — без персонажа там же ему было негде показаться.
 */
static void draw_toast(uint32_t t)
{
    if (gfx_is_landscape() || !s_toast[0] || t >= s_toast_until_ms) {
        return;
    }
    const wackos_font_t *f = &wackos_font_small;
    int w = GFX_W - 20;
    int h = f->line_height * 2 + 14;
    int x = 10, y = HINT_Y - h - 6;

    gfx_round_rect(x + 1, y + 2, w, h, 10, TH->shadow, 40);
    gfx_round_rect(x, y, w, h, 10, TH->card, 235);
    gfx_round_rect_outline(x, y, w, h, 10, 1, TH->card_edge, 220);
    ui_text_block(f, x + 8, y + 7, w - 16, 2, s_toast, TH->text);
}

/**
 * @brief   Отрисовать один кадр целиком (обои, шапка, экран, подсказки) и вывести его.
 */
static void render(void)
{
    /* Защита от повторного входа на случай, если draw() экрана сам
     * как-то спровоцирует повторный вызов render(). */
    static bool s_rendering = false;
    if (s_rendering) {
        return;
    }
    s_rendering = true;

    uint32_t t = now_ms();
    mascot_tick(t);

    /* Сброс на портрет перед обоями/шапкой: альбомный режим включает
     * сам экран на время своей отрисовки (см. gfx_set_landscape()) и
     * не обязан его выключать — следующий кадр начинается чисто. */
    gfx_set_landscape(false);

    draw_wallpaper(t);
    draw_header(t);
    current()->draw(t);
    draw_toast(t);

    /* Яркость: на плате это ШИМ подсветки, в симуляторе — затемнение
     * кадра. Здесь применяем второй вариант; на железе display_flush
     * просто отправит кадр, а яркостью займётся подсветка. */
    display_flush();
    s_rendering = false;
}

/* ------------------------------------------------------------------ */
/* Навигация                                                           */
/* ------------------------------------------------------------------ */

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Перейти на экран, нарастив стек навигации.
 * @param   id  Идентификатор экрана.
 */
void ui_goto(screen_id_t id)
{
    if (id >= SCREEN_COUNT) {
        return;
    }
    if (current()->exit) {
        current()->exit();
    }
    if (s_depth + 1 < NAV_DEPTH) {
        s_depth++;
    }
    s_stack[s_depth] = id;
    if (SCREENS[id]->enter) {
        SCREENS[id]->enter();
    }
}

/**
 * @brief   Заменить текущий экран, не наращивая стек (нужно загрузке).
 * @param   id  Идентификатор экрана.
 */
void ui_replace(screen_id_t id)
{
    if (id >= SCREEN_COUNT) {
        return;
    }
    if (current()->exit) {
        current()->exit();
    }
    s_stack[s_depth] = id;
    if (SCREENS[id]->enter) {
        SCREENS[id]->enter();
    }
}

/** @brief  Вернуться на предыдущий экран (длинное нажатие). */
void ui_back(void)
{
    if (s_depth == 0) {
        return;
    }
    if (current()->exit) {
        current()->exit();
    }
    s_depth--;
    sound_play(SOUND_BACK);
    if (current()->enter) {
        current()->enter();
    }
}

/** @brief  Сбросить стек навигации и вернуться сразу на главный экран (циферблат). */
void ui_home(void)
{
    if (current()->exit) {
        current()->exit();
    }
    s_depth = 0;
    s_stack[0] = SCREEN_HOME;
    sound_play(SOUND_BACK);
    if (current()->enter) {
        current()->enter();
    }
}

/** @brief  Идентификатор текущего экрана. */
screen_id_t ui_current(void)
{
    return s_stack[s_depth];
}

/* ------------------------------------------------------------------ */

/** @brief  Инициализировать дисплей, энкодер, звук, настройки и показать boot. */
void ui_init(void)
{
    display_init();
    encoder_init();
    sound_init();
    settings_init();

    s_depth = 0;
    s_stack[0] = SCREEN_BOOT;
    if (screen_boot.enter) {
        screen_boot.enter();
    }
    mascot_note_activity();
}

/** @brief  Короткое сообщение пользователю (всплывает над подсказками, см. draw_toast()). */
void ui_toast(const char *text)
{
    strncpy(s_toast, text, sizeof(s_toast) - 1);
    s_toast[sizeof(s_toast) - 1] = '\0';
    s_toast_until_ms = now_ms() + TOAST_DURATION_MS;
    ESP_LOGI(TAG, "%s", s_toast);
}

/** @brief  Текст последнего тоста. */
const char *ui_toast_text(void)
{
    return s_toast;
}

/**
 * @brief   Один шаг интерфейса: кадр плюс разбор события энкодера.
 */
void ui_tick(void)
{
    render();

    /* Логика экрана — после кадра, а не внутри него. */
    if (current()->tick) {
        current()->tick(now_ms());
    }

    wackos_encoder_event_t ev;
    if (!encoder_poll(&ev, FRAME_INTERVAL_MS)) {
        return;
    }

    mascot_note_activity();
    uint32_t t = now_ms();

    switch (ev) {
        case WACKOS_ENC_CW:
        case WACKOS_ENC_CCW: {
            /*
             * Ускорение прокрутки: чем быстрее крутишь, тем крупнее шаг.
             * Иначе длинный список листается вечность — а энкодер у
             * KY-040 щёлкает мелко.
             */
            uint32_t dt = t - s_last_turn_ms;
            s_last_turn_ms = t;
            int steps = (dt < 70) ? 3 : (dt < 140 ? 2 : 1);

            sound_play(SOUND_TICK);
            for (int i = 0; i < steps; i++) {
                current()->input((ev == WACKOS_ENC_CW) ? UI_IN_NEXT : UI_IN_PREV);
            }
            break;
        }

        case WACKOS_ENC_BTN:
            sound_play(SOUND_SELECT);
            current()->input(UI_IN_PRESS);
            break;

        case WACKOS_ENC_BTN_LONG:
            current()->input(UI_IN_BACK);
            break;

        default:
            break;
    }
}
