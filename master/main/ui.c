/**
 * @file    ui.c
 * @author  IAmWack
 * @brief   Слой интерфейса master: роутер экранов, навигация, "хром" кадра.
 *
 * Знает про меню и рисование, но ничего не знает про UART, протокол и
 * то, что физически умеет slave. С логикой общается только через
 * app_logic.h (спросить данные, выполнить действие). Сами экраны живут
 * в master/main/screens/, каждый реализует ui_screen_t (ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "theme.h"
#include "display.h"
#include "encoder.h"
#include "astolfo.h"
#include "settings.h"
#include "sound.h"
#include "wackos_gfx.h"

#include <math.h>
#include <string.h>

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

/* ============================================================================
 *  Static state
 * ============================================================================ */

static const ui_screen_t *const SCREENS[SCREEN_COUNT] = {
    [SCREEN_BOOT]     = &screen_boot,
    [SCREEN_MENU]     = &screen_menu,
    [SCREEN_MODULES]  = &screen_modules,
    [SCREEN_MODULE]   = &screen_module,
    [SCREEN_SLAVE]    = &screen_slave,
    [SCREEN_SETTINGS] = &screen_settings,
    [SCREEN_ABOUT]    = &screen_about,
};

static screen_id_t s_stack[NAV_DEPTH] = { SCREEN_BOOT };
static int s_depth = 0;

static char s_toast[128] = "";
static bool s_link_online = false;
static bool s_busy = false;

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

    if (s_stack[s_depth] == SCREEN_MENU || s_stack[s_depth] == SCREEN_BOOT) {
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
    } else {
        /* Вложенный экран: стрелка "назад" + заголовок. */
        const wackos_font_t *f = &wackos_font_body;
        int y = card_y + (card_h - f->line_height) / 2;
        gfx_triangle(card_x + 12, card_y + card_h / 2,
                     card_x + 21, card_y + card_h / 2 - 7,
                     card_x + 21, card_y + card_h / 2 + 7, TH->accent, 235);
        gfx_text_ellipsis(f, card_x + 28, y, current()->title, card_w - 52, TH->text, 255);
    }

    /* Индикатор связи: точка в правом верхнем углу карточки. */
    uint16_t dot = s_link_online ? TH->ok : TH->alert;
    float blink = s_link_online ? 1.0f : (0.35f + 0.65f * pulse(t, 700.0f, 0.0f));
    gfx_circle(card_x + card_w - 13, card_y + 8, 3, dot, (uint8_t)(255 * blink));
}

/*
 * Строка подсказок. В hint можно поставить '|': тогда левая часть
 * прижимается влево, правая вправо — так две подсказки помещаются в
 * 172 пикселя, а по центру они не влезали и обрезались.
 *
 * @brief   Нарисовать строку подсказок внизу экрана.
 */
static void draw_hintbar(uint32_t t)
{
    const char *text = s_busy ? "спрашиваю slave..." : current()->hint;
    if (!text) {
        return;
    }
    const wackos_font_t *f = &wackos_font_small;
    int y = HINT_Y + (18 - f->line_height) / 2 + 1;

    const char *sep = strchr(text, '|');
    if (!sep) {
        int w = gfx_text_width(f, text) + 16;
        if (w > GFX_W - 8) {
            w = GFX_W - 8;
        }
        int x = (GFX_W - w) / 2;
        gfx_round_rect(x, HINT_Y, w, 18, 9, TH->card, 150);
        gfx_text_center(f, GFX_W / 2, y, text, TH->text, 230);

        if (s_busy) {
            int dots = (int)((t / 300) % 4);
            for (int i = 0; i < dots; i++) {
                gfx_circle(x + w + 5 + i * 6, HINT_Y + 9, 2, TH->accent, 220);
            }
        }
        return;
    }

    char left[40];
    size_t n = (size_t)(sep - text);
    if (n >= sizeof(left)) {
        n = sizeof(left) - 1;
    }
    memcpy(left, text, n);
    left[n] = '\0';
    const char *right = sep + 1;

    gfx_round_rect(4, HINT_Y, GFX_W - 8, 18, 9, TH->card, 150);

    /* Правая подсказка ("назад") важнее — её рисуем целиком, а левой
     * достаётся остаток ширины. Так они не наезжают друг на друга,
     * какой бы длины ни был текст. */
    int right_w = gfx_text_width(f, right);
    gfx_text(f, GFX_W - 12 - right_w, y, right, TH->text_dim, 230);
    gfx_text_ellipsis(f, 12, y, left, GFX_W - 32 - right_w, TH->text, 230);
}

/**
 * @brief   Отрисовать один кадр целиком (обои, шапка, экран, подсказки) и вывести его.
 */
static void render(void)
{
    /*
     * Защита от повторного входа: запрос к slave зовёт ui_set_busy(),
     * который рисует кадр, и если бы экран в этот момент сам полез в
     * логику, получилась бы рекурсия.
     */
    static bool s_rendering = false;
    if (s_rendering) {
        return;
    }
    s_rendering = true;

    uint32_t t = now_ms();
    astolfo_tick(t);

    draw_wallpaper(t);
    draw_header(t);
    current()->draw(t);
    draw_hintbar(t);

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
    s_depth--;
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
    astolfo_note_activity();
}

/** @brief  Короткое сообщение пользователю (реплика в облачке у Астольфо). */
void ui_toast(const char *text)
{
    strncpy(s_toast, text, sizeof(s_toast) - 1);
    s_toast[sizeof(s_toast) - 1] = '\0';
    ESP_LOGI(TAG, "%s", s_toast);
}

/** @brief  Текст последнего тоста. */
const char *ui_toast_text(void)
{
    return s_toast;
}

/** @brief  Установить состояние связи со slave — индикатор в шапке. */
void ui_set_link_online(bool online)
{
    s_link_online = online;
}

/** @brief  Текущее состояние связи со slave. */
bool ui_link_online(void)
{
    return s_link_online;
}

/**
 * @brief   Отметить, идёт ли сейчас блокирующий запрос к slave.
 * @param   busy  true — сразу рисует кадр, чтобы "думаю" было видно.
 */
void ui_set_busy(bool busy)
{
    s_busy = busy;
    if (busy) {
        astolfo_set_mood(ASTOLFO_THINKING);
        render(); /* запрос блокирующий — иначе "думаю" никто не увидит */
    }
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

    astolfo_note_activity();
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
