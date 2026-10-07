/**
 * @file    razor.c
 * @author  IAmWack
 * @brief   Отрисовка и состояние персонажа-маскота Разора (второй маскот).
 *
 * Второй маскот наравне с Астольфо (astolfo.c) — тот же принцип
 * (состояние системы, ноль байт ассетов, примитивы components/wackos_gfx),
 * но другой силуэт и другая палитра: волчонок из Genshin Impact —
 * длинные растрёпанные серебристо-серые волосы с двумя торчащими
 * прядями-"ушами" (это причёска, не отдельные меховые уши — так он
 * нарисован в официальном арте), красные глаза, светлая кожа,
 * серо-фиолетовый капюшон со светлой опушкой у горла. Первая версия
 * этого файла угадала силуэт "уши + растрёпанность", но перепутала
 * палитру (тёплый блонд+загар вместо серебра+красных глаз) и заменила
 * капюшон меховой накидкой через одно плечо — по факту сверке с
 * референсом (creativeuncut.com/gallery-39/gi-razor.html) поправлено.
 * Электро-стихия персонажа отражена акцентом настроения (фиолетовая
 * молния при HAPPY) и тоном капюшона — сам капюшон в каноне и правда
 * серо-фиолетовый, так что тут "громовое" смещение палитры не
 * противоречит первоисточнику.
 *
 * Задел на будущее: предполагается показывать этого персонажа в
 * вооружённом режиме (SYS_ARM, см. docs/SECURITY.md) вместо Астольфо —
 * смена маскота как визуальный сигнал "сейчас доступны особые
 * команды". Сама коммутация здесь не реализована, только рисование.
 */
#include "razor.h"
#include "theme.h"
#include "wackos_gfx.h"
#include "portrait_sd.h"

#include <math.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

/*
 * Палитра по референсу: холодное серебро волос, красные глаза, светлая
 * кожа, серо-фиолетовый капюшон со светлой опушкой. "Громовой" акцент —
 * оттенок капюшона (в каноне и правда сероватый с фиолетовым отливом)
 * плюс фиолетовая молния в значке настроения HAPPY.
 */
#define HAIR         GFX_RGB(222, 226, 232)  /* холодный серебристо-серый */
#define HAIR_DARK    GFX_RGB(168, 174, 186)
#define HAIR_LIGHT   GFX_RGB(246, 247, 250)
#define SKIN         GFX_RGB(255, 234, 220)  /* светлая кожа, не загар */
#define SKIN_SHADE   GFX_RGB(235, 202, 185)
#define EYE          GFX_RGB(214,  48,  58)  /* красный — канон */
#define EYE_DEEP     GFX_RGB(122,  20,  28)
#define EYE_WHITE    GFX_RGB(255, 250, 255)
#define BROW         GFX_RGB(150, 152, 162)  /* светло-серые, под цвет волос */
#define BLUSH        GFX_RGB(255, 150, 130)
#define LINE_C       GFX_RGB( 78,  70,  86)
#define LASH         GFX_RGB( 62,  56,  70)
#define ELECTRO      GFX_RGB(150,  90, 230)  /* молния — акцент настроения */
#define SPARK        GFX_RGB(232, 210, 255)
#define OUTFIT       GFX_RGB( 64,  54,  46)  /* тёмный жилет, чуть видный под капюшоном */
#define HOOD         GFX_RGB( 96,  86, 108)  /* серо-фиолетовый капюшон — канон */
#define HOOD_TRIM    GFX_RGB(236, 230, 226)  /* светлая опушка капюшона у горла */

/* Сколько держится "реактивное" настроение, прежде чем вернуться в idle. */
#define MOOD_HOLD_MS   2200
/* Через сколько простоя Разор засыпает. */
#define SLEEP_AFTER_MS 60000

/* ============================================================================
 *  Static state
 * ============================================================================ */

static razor_mood_t s_mood = RAZOR_IDLE;
static uint32_t s_mood_at = 0;
static uint32_t s_activity_at = 0;
static uint32_t s_now = 0;

/* ============================================================================
 *  Public API — состояние (настроение, активность)
 * ============================================================================ */

/** @brief  Установить текущее настроение персонажа. */
void razor_set_mood(razor_mood_t mood)
{
    s_mood = mood;
    s_mood_at = s_now;
}

/** @brief  Текущее настроение персонажа. */
razor_mood_t razor_mood(void)
{
    return s_mood;
}

/** @brief  Отметить активность пользователя — сбросить таймер засыпания. */
void razor_note_activity(void)
{
    s_activity_at = s_now;
    if (s_mood == RAZOR_SLEEP) {
        razor_set_mood(RAZOR_IDLE);
    }
}

/**
 * @brief   Раз в кадр: обновить настроение по прошедшему времени.
 * @param   now_ms  Текущее время в миллисекундах (монотонное).
 */
void razor_tick(uint32_t now_ms)
{
    s_now = now_ms;

    if ((s_mood == RAZOR_HAPPY || s_mood == RAZOR_SAD) &&
        now_ms - s_mood_at > MOOD_HOLD_MS) {
        razor_set_mood(RAZOR_IDLE);
    }

    if (s_mood == RAZOR_IDLE && now_ms - s_activity_at > SLEEP_AFTER_MS) {
        razor_set_mood(RAZOR_SLEEP);
    }
}

/** @brief  Реплика персонажа под текущее настроение — для облачка в меню. */
const char *razor_line(void)
{
    switch (s_mood) {
        case RAZOR_HAPPY:    return "получилось!";
        case RAZOR_SAD:      return "он молчит...";
        case RAZOR_THINKING: return "спрашиваю...";
        case RAZOR_SLEEP:    return "zzz...";
        default:             return "р-р-р, жду";
    }
}

/* ------------------------------------------------------------------ */
/* Рисование                                                           */
/* ------------------------------------------------------------------ */

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/** @brief  Плавная пульсация [0..1] с заданным периодом и фазой (синус). */
static float pulse(uint32_t t, float period_ms, float phase)
{
    return 0.5f + 0.5f * sinf((float)t / period_ms * 6.2831853f + phase);
}

/* Моргание: короткое закрытие глаз раз в несколько секунд. */
static bool is_blinking(uint32_t t)
{
    return (t % 3600) < 110;
}

/**
 * @brief   Нарисовать открытый глаз с бликом и верхним веком/ресницами.
 * @param   cx, cy       Центр глаза.
 * @param   ex, ey       Полуоси глаза (радиусы).
 * @param   look_x, look_y  Смещение зрачка — направление взгляда.
 */
static void draw_eye_open(int cx, int cy, int ex, int ey, int look_x, int look_y)
{
    gfx_ellipse(cx, cy, ex, ey, EYE_WHITE, 255);
    gfx_ellipse(cx + look_x, cy + look_y, ex - 1, ey - 1, EYE, 255);
    gfx_ellipse(cx + look_x, cy + look_y + ey / 4, (ex * 2) / 3, (ey * 2) / 3, EYE_DEEP, 255);
    gfx_circle(cx + look_x - ex / 3, cy + look_y - ey / 3, (ex + 2) / 3, EYE_WHITE, 255);
    gfx_circle(cx + look_x + ex / 3, cy + look_y + ey / 3, (ex + 3) / 5, EYE_WHITE, 190);
    /* Верхнее веко чуть резче, чем у Астольфо — взгляд построже,
     * "дикий", а не мечтательный. */
    int lash = (ex > 5) ? 3 : 2;
    gfx_arc(cx, cy, ex + 1, lash, 15, 165, LASH, 255);
    gfx_line(cx - ex, cy - ey / 3, cx - ex - lash - 1, cy - ey / 2 - lash - 1, lash, LASH, 240);
}

/**
 * @brief   Нарисовать закрытый глаз дугой.
 * @param   cx, cy  Центр глаза.
 * @param   ex      Радиус дуги.
 * @param   t       Толщина линии.
 * @param   up      true -> "^" (радость), false -> "∪" (сон/моргание).
 */
static void draw_eye_closed(int cx, int cy, int ex, int t, bool up)
{
    if (up) {
        gfx_arc(cx, cy + ex / 2, ex, t, 25, 155, LINE_C, 255);
    } else {
        gfx_arc(cx, cy - ex / 2, ex, t, 205, 335, LINE_C, 255);
    }
}

/**
 * @brief   Молния-разряд вместо сердечка — знак хорошего настроения электро-волчонка.
 * @param   cx, cy  Опорная точка (нижний кончик разряда).
 * @param   size    Масштаб (примерно высота фигуры).
 */
static void draw_bolt(int cx, int cy, int size, uint16_t color, uint8_t alpha)
{
    int w = size / 2 + 1;
    int h2 = size;
    gfx_line(cx, cy - h2, cx - w, cy - h2 / 4, 2, color, alpha);
    gfx_line(cx - w, cy - h2 / 4, cx + w / 3, cy - h2 / 4, 2, color, alpha);
    gfx_line(cx + w / 3, cy - h2 / 4, cx - w / 4, cy, 2, color, alpha);
}

/* ============================================================================
 *  Public API — отрисовка
 * ============================================================================ */

/**
 * @brief   Нарисовать бюст Разора (голова, растрёпанные волосы, мех, плечи).
 * @param   cx  Центр по X.
 * @param   y   Верх фигуры по Y.
 * @param   h   Высота головы — масштаб всей фигуры (см. RAZOR_BUST_H).
 * @param   t   Время в миллисекундах, для анимации (дыхание, моргание...).
 */
void razor_draw(int cx, int y, int h, uint32_t t)
{
    if (h < 16) {
        h = 16;
    }

    float breathe = pulse(t, 2600.0f, 0.0f);
    int bob = (int)((breathe - 0.5f) * (h * 0.045f));
    float sway_f = sinf((float)t / 2100.0f);
    int sway = (int)(sway_f * h * 0.018f);

    int rx = (int)(h * 0.44f);
    int ry = (int)(h * 0.47f);
    int ox = cx + sway;
    int oy = y + ry + bob;

    int line_w = h > 60 ? 2 : 1;

    /* --- плечи, шея --- */
    int shoulder_y = oy + (int)(ry * 1.22f);
    gfx_ellipse(ox, oy + (int)(ry * 0.98f), (int)(rx * 0.26f), (int)(ry * 0.30f),
                SKIN_SHADE, 255);

    /*
     * Капюшон — большой округлый серо-фиолетовый силуэт позади плеч со
     * светлой опушкой у горла (канон, см. header-комментарий), а не
     * меховая накидка через одно плечо, как в первой версии. Жилет
     * поверх капюшона — только чтобы под опушкой был виден край другого
     * материала, а не голая шея.
     */
    gfx_ellipse(cx, shoulder_y + (int)(ry * 0.35f), (int)(rx * 1.55f), (int)(ry * 0.95f),
                HOOD, 255);
    gfx_arc(cx, shoulder_y + (int)(ry * 0.02f), (int)(rx * 1.10f), line_w + 2,
            195, 345, HOOD_TRIM, 255);
    gfx_ellipse(cx, shoulder_y + (int)(ry * 0.78f), (int)(rx * 1.05f), (int)(ry * 0.52f),
                OUTFIT, 255);

    /* --- волосы сзади --- */
    gfx_ellipse(ox, oy + (int)(ry * 0.20f), (int)(rx * 1.08f), (int)(ry * 1.10f),
                HAIR_DARK, 255);
    gfx_ellipse(ox, oy + (int)(ry * 0.16f), (int)(rx * 1.03f), (int)(ry * 1.05f),
                HAIR, 255);

    /*
     * "Уши" — на деле две пряди волос, скрученные в острые торчащие
     * пики (канон: не отдельные меховые уши, а причёска — см.
     * header-комментарий), поэтому цвет тот же, что у волос.
     */
    int ear_w = (int)(rx * 0.30f);
    int ear_h = (int)(h * 0.30f);
    int ear_top = oy - (int)(ry * 0.95f);
    gfx_cat_ear(ox - (int)(rx * 0.98f) - ear_w / 2, ear_top, ear_w, ear_h, false,
                HAIR_DARK, HAIR, 255);
    gfx_cat_ear(ox + (int)(rx * 0.98f) - ear_w / 2, ear_top, ear_w, ear_h, true,
                HAIR_DARK, HAIR, 255);

    /* --- лицо --- */
    gfx_ellipse(ox, oy, rx, ry, SKIN_SHADE, 255);
    gfx_ellipse(ox, oy - 1, rx - 1, ry - 1, SKIN, 255);
    gfx_ellipse(ox, oy + (int)(ry * 0.50f), (int)(rx * 0.72f), (int)(ry * 0.52f), SKIN, 255);

    /* Короткие пряди по бокам — до подбородка, не ниже (в отличие от
     * длинных локонов Астольфо: у Разора причёска короткая и растрёпанная). */
    for (int side = -1; side <= 1; side += 2) {
        int lx = ox + (int)(side * rx * 0.88f);
        gfx_ellipse(lx, oy - (int)(ry * 0.02f), (int)(rx * 0.28f), (int)(ry * 0.46f),
                    HAIR, 255);
        gfx_ellipse(lx, oy + (int)(ry * 0.34f), (int)(rx * 0.20f), (int)(ry * 0.30f),
                    HAIR_DARK, 255);
    }

    /* Аккуратная часть чёлки над лбом — три коротких пряди. */
    gfx_ellipse(ox, oy - (int)(ry * 0.50f), (int)(rx * 0.98f), (int)(ry * 0.56f),
                HAIR, 255);
    {
        static const float LOCK_X[]    = { -0.46f,  0.00f,  0.46f };
        static const float LOCK_TIP[]  = { -0.14f, -0.20f, -0.14f };
        static const float LOCK_LEAN[] = { -0.10f,  0.00f,  0.10f };
        static const float LOCK_HALF[] = {  0.26f,  0.24f,  0.26f };
        int top = oy - (int)(ry * 0.60f);
        for (int i = 0; i < 3; i++) {
            int px = ox + (int)(LOCK_X[i] * rx);
            int tip = oy + (int)(ry * LOCK_TIP[i]);
            int half = (int)(rx * LOCK_HALF[i]);
            int lean = (int)(rx * LOCK_LEAN[i]);
            gfx_triangle(px - half, top, px + half, top, px + lean, tip + 2, HAIR_DARK, 255);
            gfx_triangle(px - half + 1, top, px + half - 1, top, px + lean, tip - 1,
                         HAIR, 255);
        }
    }

    /*
     * Пара мелких вихров между "ушами" — добавляют растрёпанности, не
     * перетягивая внимание с них (в первой версии их было 4 широких, и
     * вместе с "ушами" получалась сплошная зубчатая корона).
     */
    {
        static const float TUFT_X[]    = { -0.16f,  0.20f };
        static const float TUFT_LEAN[] = { -0.10f,  0.14f };
        static const float TUFT_LEN[]  = {  0.20f,  0.17f };
        static const float TUFT_HALF[] = {  0.16f,  0.15f };
        int base = oy - (int)(ry * 0.50f);
        for (int i = 0; i < 2; i++) {
            int bx = ox + (int)(TUFT_X[i] * rx);
            int half = (int)(TUFT_HALF[i] * rx);
            int tipx = bx + (int)(TUFT_LEAN[i] * rx);
            int tipy = base - (int)(TUFT_LEN[i] * h);
            gfx_triangle(bx - half, base, bx + half, base, tipx, tipy, HAIR_DARK, 255);
            gfx_triangle(bx - half + 1, base, bx + half - 1, base, tipx, tipy + 2, HAIR, 255);
        }
    }

    /* Блик на волосах. */
    gfx_arc(ox, oy - (int)(ry * 0.34f), (int)(rx * 0.72f), h > 60 ? 3 : 2,
            30, 150, HAIR_LIGHT, 150);

    /* --- глаза --- */
    int ex = (int)(h * 0.125f);
    int ey = (int)(h * 0.155f);
    int eye_y = oy + (int)(ry * 0.16f);
    int eye_dx = (int)(rx * 0.44f);
    if (ex < 2) ex = 2;
    if (ey < 2) ey = 2;

    /* Тёмные брови вопреки светлым волосам — канонная примета лица,
     * без неё лицо со светлыми ресницами читается "стёртым". При SAD
     * их роль уже играют собственные "нахмуренные" линии ниже. */
    if (s_mood != RAZOR_SAD) {
        int brow_y = eye_y - ey - line_w * 3;
        gfx_line(ox - eye_dx - ex, brow_y + line_w, ox - eye_dx + ex, brow_y, line_w + 1, BROW, 255);
        gfx_line(ox + eye_dx - ex, brow_y, ox + eye_dx + ex, brow_y + line_w, line_w + 1, BROW, 255);
    }

    switch (s_mood) {
        case RAZOR_HAPPY:
            draw_eye_closed(ox - eye_dx, eye_y, ex, line_w + 1, true);
            draw_eye_closed(ox + eye_dx, eye_y, ex, line_w + 1, true);
            break;

        case RAZOR_SLEEP:
            draw_eye_closed(ox - eye_dx, eye_y, ex, line_w + 1, false);
            draw_eye_closed(ox + eye_dx, eye_y, ex, line_w + 1, false);
            break;

        case RAZOR_THINKING:
            draw_eye_open(ox - eye_dx, eye_y, ex, ey, -ex / 3, -ey / 3);
            draw_eye_open(ox + eye_dx, eye_y, ex, ey, -ex / 3, -ey / 3);
            break;

        case RAZOR_SAD:
            draw_eye_open(ox - eye_dx, eye_y, ex, ey, 0, ey / 4);
            draw_eye_open(ox + eye_dx, eye_y, ex, ey, 0, ey / 4);
            gfx_line(ox - eye_dx - ex, eye_y - ey - line_w * 2,
                     ox - eye_dx + ex, eye_y - ey - line_w * 4, line_w, LINE_C, 220);
            gfx_line(ox + eye_dx - ex, eye_y - ey - line_w * 4,
                     ox + eye_dx + ex, eye_y - ey - line_w * 2, line_w, LINE_C, 220);
            gfx_ellipse(ox + eye_dx + ex, eye_y + ey, (int)(h * 0.035f) + 1,
                        (int)(h * 0.055f) + 1, GFX_RGB(150, 210, 255), 230);
            break;

        default:
            if (is_blinking(t)) {
                draw_eye_closed(ox - eye_dx, eye_y, ex, line_w + 1, false);
                draw_eye_closed(ox + eye_dx, eye_y, ex, line_w + 1, false);
            } else {
                draw_eye_open(ox - eye_dx, eye_y, ex, ey, 0, 0);
                draw_eye_open(ox + eye_dx, eye_y, ex, ey, 0, 0);
            }
            break;
    }

    /* --- румянец и рот --- */
    int blush_y = eye_y + (int)(ry * 0.42f);
    gfx_ellipse(ox - (int)(rx * 0.68f), blush_y, (int)(h * 0.10f), (int)(h * 0.05f), BLUSH, 100);
    gfx_ellipse(ox + (int)(rx * 0.68f), blush_y, (int)(h * 0.10f), (int)(h * 0.05f), BLUSH, 100);

    int mouth_y = oy + (int)(ry * 0.54f);
    int mouth_r = (int)(h * 0.065f) + 1;
    switch (s_mood) {
        case RAZOR_HAPPY:
            /* Ухмылка, а не мягкая улыбка Астольфо — уголок вверх, дуга пошире. */
            gfx_arc(ox, mouth_y - mouth_r, mouth_r + 1, line_w + 1, 15, 165, LINE_C, 235);
            break;
        case RAZOR_SAD:
            gfx_arc(ox, mouth_y + mouth_r, mouth_r, line_w, 35, 145, LINE_C, 230);
            break;
        case RAZOR_SLEEP:
            gfx_ellipse(ox, mouth_y, mouth_r / 2 + 1, mouth_r / 2 + 1, LINE_C, 180);
            break;
        case RAZOR_THINKING:
            gfx_line(ox - mouth_r / 2, mouth_y, ox + mouth_r / 2, mouth_y, line_w, LINE_C, 200);
            break;
        default:
            gfx_arc(ox, mouth_y - mouth_r / 2, mouth_r, line_w, 200, 340, LINE_C, 220);
            break;
    }

    /* --- значки настроения рядом с головой --- */
    switch (s_mood) {
        case RAZOR_HAPPY: {
            float p = pulse(t, 700.0f, 0.0f);
            gfx_sparkle(ox - (int)(rx * 1.35f), oy - (int)(ry * 0.7f),
                        (int)(h * 0.14f) + 1, SPARK, (uint8_t)(150 + p * 105));
            draw_bolt(ox + (int)(rx * 1.35f), oy - (int)(ry * 0.55f) - (int)(p * h * 0.08f),
                      (int)(h * 0.46f), ELECTRO, 255);
            break;
        }
        case RAZOR_THINKING: {
            int n = (int)((t / 350) % 3) + 1;
            for (int i = 0; i < n; i++) {
                gfx_circle(ox + (int)(rx * 0.95f) + i * (int)(h * 0.11f),
                           y - (int)(h * 0.03f), (int)(h * 0.035f) + 1, TH->card, 235);
            }
            break;
        }
        case RAZOR_SLEEP: {
            float p = pulse(t, 2400.0f, 0.0f);
            int zx = ox + (int)(rx * 1.05f);
            int zy = y - (int)(p * h * 0.18f);
            gfx_text(&wackos_font_small, zx, zy, "z", TH->card, (uint8_t)(90 + p * 165));
            gfx_text(&wackos_font_small, zx + (int)(h * 0.10f), zy - (int)(h * 0.12f), "z",
                     TH->card, (uint8_t)(200 - p * 120));
            break;
        }
        default:
            break;
    }
}

/**
 * @brief   Нарисовать растровый портрет вместо процедурного (см. razor.h).
 *
 * Читается с microSD (portrait_sd.h), не из прошивки — три маскота
 * вместе с BLE/NimBLE стеком не влезали во флеш как C-таблицы.
 * @param   cx, top_y     Центр по X и верх по Y.
 * @param   dst_w, dst_h  Размер на экране.
 */
void razor_draw_portrait(int cx, int top_y, int dst_w, int dst_h)
{
    static const char *const MOOD_NAME[] = {
        [RAZOR_IDLE]     = "idle",
        [RAZOR_HAPPY]    = "happy",
        [RAZOR_SAD]      = "sad",
        [RAZOR_THINKING] = "thinking",
        [RAZOR_SLEEP]    = "sleep",
    };
    const uint16_t *px = portrait_sd_load("razor", MOOD_NAME[s_mood]);
    if (!px) {
        return;
    }
    gfx_image_scaled(cx, top_y, dst_w, dst_h, px, PORTRAIT_SD_W, PORTRAIT_SD_H, 255);
}
