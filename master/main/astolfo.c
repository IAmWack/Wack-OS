#include "astolfo.h"
#include "theme.h"
#include "wackos_gfx.h"

#include <math.h>

/*
 * Цвета персонажа намеренно не зависят от темы интерфейса: тема — это
 * настроение корпуса, а Астольфо всегда Астольфо.
 *
 * По канону (Fate/Apocrypha): пастельно-розовые длинные волосы,
 * собранные в толстую косу ниже пояса, с ЧЁРНОЙ лентой; глаза
 * фиолетовые; чёрный костюм с бело-розовым плащом. Отсюда и палитра —
 * розовый тут в волосах, а не во всём подряд.
 */
#define HAIR        GFX_RGB(255, 186, 214)
#define HAIR_DARK   GFX_RGB(228, 132, 178)
#define HAIR_LIGHT  GFX_RGB(255, 226, 240)
#define SKIN        GFX_RGB(255, 233, 222)
#define SKIN_SHADE  GFX_RGB(238, 198, 190)
#define EYE         GFX_RGB(170,  96, 216)  /* фиолетовый */
#define EYE_DEEP    GFX_RGB( 96,  42, 140)
#define EYE_WHITE   GFX_RGB(255, 250, 255)
#define BLUSH       GFX_RGB(255, 148, 172)
#define LINE_C      GFX_RGB(122,  58,  96)
#define LASH        GFX_RGB( 96,  48,  92)  /* верхнее веко/ресницы */
#define RIBBON      GFX_RGB( 52,  44,  60)  /* чёрная лента на косе */
#define OUTFIT      GFX_RGB( 62,  54,  72)
#define CAPE        GFX_RGB(255, 250, 252)
#define CAPE_TRIM   GFX_RGB(255, 158, 198)

/* Сколько держится "реактивное" настроение, прежде чем вернуться в idle. */
#define MOOD_HOLD_MS   2200
/* Через сколько простоя Астольфо засыпает. */
#define SLEEP_AFTER_MS 60000

static astolfo_mood_t s_mood = ASTOLFO_IDLE;
static uint32_t s_mood_at = 0;
static uint32_t s_activity_at = 0;
static uint32_t s_now = 0;

void astolfo_set_mood(astolfo_mood_t mood)
{
    s_mood = mood;
    s_mood_at = s_now;
}

astolfo_mood_t astolfo_mood(void)
{
    return s_mood;
}

void astolfo_note_activity(void)
{
    s_activity_at = s_now;
    if (s_mood == ASTOLFO_SLEEP) {
        astolfo_set_mood(ASTOLFO_IDLE);
    }
}

void astolfo_tick(uint32_t now_ms)
{
    s_now = now_ms;

    /* thinking живёт ровно столько, сколько идёт запрос, — его снимает
     * логика; остальные "реактивные" настроения гаснут сами. */
    if ((s_mood == ASTOLFO_HAPPY || s_mood == ASTOLFO_SAD) &&
        now_ms - s_mood_at > MOOD_HOLD_MS) {
        astolfo_set_mood(ASTOLFO_IDLE);
    }

    if (s_mood == ASTOLFO_IDLE && now_ms - s_activity_at > SLEEP_AFTER_MS) {
        astolfo_set_mood(ASTOLFO_SLEEP);
    }
}

const char *astolfo_line(void)
{
    switch (s_mood) {
        case ASTOLFO_HAPPY:    return "получилось!";
        case ASTOLFO_SAD:      return "он молчит...";
        case ASTOLFO_THINKING: return "спрашиваю...";
        case ASTOLFO_SLEEP:    return "zzz...";
        default:               return "крути колёсико ♥";
    }
}

/* ------------------------------------------------------------------ */
/* Рисование                                                           */
/* ------------------------------------------------------------------ */

static float pulse(uint32_t t, float period_ms, float phase)
{
    return 0.5f + 0.5f * sinf((float)t / period_ms * 6.2831853f + phase);
}

/* Моргание: короткое закрытие глаз раз в несколько секунд. */
static bool is_blinking(uint32_t t)
{
    return (t % 3600) < 110;
}

static void draw_eye_open(int cx, int cy, int ex, int ey, int look_x, int look_y)
{
    gfx_ellipse(cx, cy, ex, ey, EYE_WHITE, 255);
    gfx_ellipse(cx + look_x, cy + look_y, ex - 1, ey - 1, EYE, 255);
    gfx_ellipse(cx + look_x, cy + look_y + ey / 4, (ex * 2) / 3, (ey * 2) / 3, EYE_DEEP, 255);
    /* Блик — та самая "аниме-искорка", без неё взгляд мёртвый. */
    gfx_circle(cx + look_x - ex / 3, cy + look_y - ey / 3, (ex + 2) / 3, EYE_WHITE, 255);
    gfx_circle(cx + look_x + ex / 3, cy + look_y + ey / 3, (ex + 3) / 5, EYE_WHITE, 190);
    /*
     * Верхнее веко с ресницами: самая "аниме" деталь. Без неё глаз
     * читается пуговицей, с ней — взглядом.
     */
    int lash = (ex > 5) ? 3 : 2;
    gfx_arc(cx, cy, ex + 1, lash, 20, 160, LASH, 255);
    gfx_line(cx - ex, cy - ey / 3, cx - ex - lash, cy - ey / 2 - lash, lash - 1 > 0 ? lash - 1 : 1,
             LASH, 235);
}

/* Закрытый глаз дугой: up = true -> "^" (радость), false -> "∪" (сон/моргание). */
static void draw_eye_closed(int cx, int cy, int ex, int t, bool up)
{
    if (up) {
        gfx_arc(cx, cy + ex / 2, ex, t, 25, 155, LINE_C, 255);
    } else {
        gfx_arc(cx, cy - ex / 2, ex, t, 205, 335, LINE_C, 255);
    }
}

/*
 * Коса. Главная узнаваемая деталь: толстая, длинная (у него она ниже
 * пояса), перекинута вперёд через плечо, с ЧЁРНОЙ лентой у основания.
 * Рисуется цепочкой "перехлёстов" со смещением влево-вправо и сужением
 * к кончику — так она читается плетением, а не шариками.
 */
static void draw_braid(int x, int y, int h, float swing)
{
    int beads = (h >= 56) ? 12 : 8;
    float r0 = h * 0.095f;
    if (r0 < 2.0f) {
        r0 = 2.0f;
    }

    for (int i = 0; i <= beads; i++) {
        float f = (float)i / beads;
        float r = r0 * (1.0f - 0.55f * f);        /* сужается к концу */
        int zig = (int)(((i % 2) ? 1 : -1) * r * 0.42f);
        int bx = x + zig + (int)(swing * f * h * 0.07f) - (int)(f * h * 0.05f);
        int by = y + (int)(i * r0 * 0.92f);

        gfx_ellipse(bx, by, (int)(r * 1.15f), (int)(r * 0.95f), HAIR_DARK, 255);
        gfx_ellipse(bx, by - 1, (int)(r * 1.05f), (int)(r * 0.85f), HAIR, 255);
        gfx_ellipse(bx - (int)(r * 0.35f), by - (int)(r * 0.35f),
                    (int)(r * 0.38f), (int)(r * 0.28f), HAIR_LIGHT, 130);
    }

    /* Лента у основания косы — чёрная, это её канонная деталь. */
    gfx_bow(x, y - (int)(r0 * 0.7f), (int)(h * 0.26f), RIBBON, 255);

    /* Кисточка на кончике. */
    int tip_y = y + (int)(beads * r0 * 0.92f) + (int)(r0 * 0.6f);
    int tip_x = x - (int)(h * 0.05f) + (int)(swing * h * 0.07f);
    gfx_ellipse(tip_x, tip_y, (int)(r0 * 0.5f), (int)(r0 * 0.8f), HAIR_DARK, 255);
    gfx_bow(tip_x, tip_y + (int)(r0 * 0.6f), (int)(h * 0.16f), RIBBON, 255);
}

void astolfo_draw(int cx, int y, int h, uint32_t t)
{
    if (h < 16) {
        h = 16;
    }

    /* Дыхание: голова едва заметно качается вверх-вниз и вбок. */
    float breathe = pulse(t, 2600.0f, 0.0f);
    int bob = (int)((breathe - 0.5f) * (h * 0.045f));
    float sway_f = sinf((float)t / 2100.0f);
    int sway = (int)(sway_f * h * 0.018f);

    int rx = (int)(h * 0.44f);
    int ry = (int)(h * 0.47f);
    int ox = cx + sway;
    int oy = y + ry + bob;

    int line_w = h > 60 ? 2 : 1;

    /* --- плечи, шея, плащ (бюст, а не голова в воздухе) --- */
    int shoulder_y = oy + (int)(ry * 1.22f);
    gfx_ellipse(ox, oy + (int)(ry * 0.98f), (int)(rx * 0.26f), (int)(ry * 0.30f),
                SKIN_SHADE, 255);                                    /* шея */
    gfx_ellipse(cx, shoulder_y + (int)(ry * 0.70f), (int)(rx * 1.34f), (int)(ry * 0.72f),
                OUTFIT, 255);                                        /* плечи */
    /* Высокий бело-розовый воротник плаща с уголком на груди. */
    gfx_triangle(cx - (int)(rx * 0.60f), shoulder_y + (int)(ry * 0.10f),
                 cx + (int)(rx * 0.60f), shoulder_y + (int)(ry * 0.10f),
                 cx, shoulder_y + (int)(ry * 0.80f), CAPE, 255);
    gfx_line(cx - (int)(rx * 0.60f), shoulder_y + (int)(ry * 0.10f),
             cx, shoulder_y + (int)(ry * 0.80f), line_w, CAPE_TRIM, 235);
    gfx_line(cx + (int)(rx * 0.60f), shoulder_y + (int)(ry * 0.10f),
             cx, shoulder_y + (int)(ry * 0.80f), line_w, CAPE_TRIM, 235);

    /* --- волосы сзади --- */
    gfx_ellipse(ox, oy + (int)(ry * 0.24f), (int)(rx * 1.12f), (int)(ry * 1.18f),
                HAIR_DARK, 255);
    gfx_ellipse(ox, oy + (int)(ry * 0.20f), (int)(rx * 1.07f), (int)(ry * 1.13f),
                HAIR, 255);

    /* --- лицо --- */
    gfx_ellipse(ox, oy, rx, ry, SKIN_SHADE, 255);
    gfx_ellipse(ox, oy - 1, rx - 1, ry - 1, SKIN, 255);
    /* Подбородок уже черепа — иначе получается колобок. */
    gfx_ellipse(ox, oy + (int)(ry * 0.50f), (int)(rx * 0.72f), (int)(ry * 0.52f),
                SKIN, 255);

    /*
     * Боковые пряди: одна на сторону, вдоль самого края лица, сужается
     * книзу. Они же "подрезают" щёки, из-за чего лицо перестаёт быть
     * круглым.
     */
    for (int side = -1; side <= 1; side += 2) {
        int lx = ox + (int)(side * rx * 0.86f);
        gfx_ellipse(lx, oy - (int)(ry * 0.05f), (int)(rx * 0.26f), (int)(ry * 0.62f),
                    HAIR, 255);
        gfx_ellipse(lx, oy + (int)(ry * 0.52f), (int)(rx * 0.20f), (int)(ry * 0.52f),
                    HAIR, 255);
        gfx_ellipse(lx, oy + (int)(ry * 0.98f), (int)(rx * 0.13f), (int)(ry * 0.38f),
                    HAIR_DARK, 255);
        gfx_ellipse(lx - (int)(side * rx * 0.04f), oy + (int)(ry * 0.20f),
                    (int)(rx * 0.06f), (int)(ry * 0.42f), HAIR_LIGHT, 140);
    }

    /*
     * Чёлка. Ключ к узнаваемости: пряди спускаются ДО самых бровей и
     * ложатся внахлёст, а не аккуратной шапкой — тогда виден силуэт
     * причёски, а не розовый шлем.
     */
    gfx_ellipse(ox, oy - (int)(ry * 0.52f), (int)(rx * 1.03f), (int)(ry * 0.62f),
                HAIR, 255);

    /* Крупные пряди: две центральные расходятся от пробора, две
     * височные спускаются ниже, к скулам. */
    static const float LOCK_X[]    = { -0.66f, -0.24f,  0.24f,  0.66f };
    static const float LOCK_TIP[]  = { -0.02f, -0.24f, -0.24f, -0.02f };
    static const float LOCK_LEAN[] = { -0.26f, -0.16f,  0.16f,  0.26f };
    static const float LOCK_HALF[] = {  0.34f,  0.30f,  0.30f,  0.34f };
    for (int i = 0; i < 4; i++) {
        int px = ox + (int)(LOCK_X[i] * rx);
        int top = oy - (int)(ry * 0.60f);
        int tip = oy + (int)(ry * LOCK_TIP[i]);
        int half = (int)(rx * LOCK_HALF[i]);
        int lean = (int)(rx * LOCK_LEAN[i]);
        gfx_triangle(px - half, top, px + half, top, px + lean, tip + 2, HAIR_DARK, 255);
        gfx_triangle(px - half + 1, top, px + half - 1, top, px + lean, tip - 1,
                     HAIR, 255);
    }

    /* Блик на волосах: полоса света поперёк чёлки. Без него вся голова
     * выглядит одной плоской заливкой. */
    gfx_arc(ox, oy - (int)(ry * 0.34f), (int)(rx * 0.72f), h > 60 ? 3 : 2,
            30, 150, HAIR_LIGHT, 150);

    /* Ахоге — торчащая прядка-завиток. Толстая и короткая: тонкая и
     * длинная читается не волосом, а проволочкой. */
    gfx_line(ox + (int)(rx * 0.06f), y + (int)(h * 0.05f),
             ox + (int)(rx * 0.26f), y - (int)(h * 0.05f), line_w + 2, HAIR, 255);
    gfx_line(ox + (int)(rx * 0.26f), y - (int)(h * 0.05f),
             ox + (int)(rx * 0.04f), y - (int)(h * 0.09f), line_w + 1, HAIR, 255);

    /* --- коса через плечо --- */
    draw_braid(ox - (int)(rx * 1.10f), oy + (int)(ry * 0.34f), h, sway_f);

    /* --- глаза --- */
    int ex = (int)(h * 0.125f);
    int ey = (int)(h * 0.155f);
    int eye_y = oy + (int)(ry * 0.16f);
    int eye_dx = (int)(rx * 0.44f);
    if (ex < 2) ex = 2;
    if (ey < 2) ey = 2;

    switch (s_mood) {
        case ASTOLFO_HAPPY:
            draw_eye_closed(ox - eye_dx, eye_y, ex, line_w + 1, true);
            draw_eye_closed(ox + eye_dx, eye_y, ex, line_w + 1, true);
            break;

        case ASTOLFO_SLEEP:
            draw_eye_closed(ox - eye_dx, eye_y, ex, line_w + 1, false);
            draw_eye_closed(ox + eye_dx, eye_y, ex, line_w + 1, false);
            break;

        case ASTOLFO_THINKING:
            /* Взгляд уехал вверх-вбок — классическое "думаю". */
            draw_eye_open(ox - eye_dx, eye_y, ex, ey, -ex / 3, -ey / 3);
            draw_eye_open(ox + eye_dx, eye_y, ex, ey, -ex / 3, -ey / 3);
            break;

        case ASTOLFO_SAD:
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
    gfx_ellipse(ox - (int)(rx * 0.68f), blush_y, (int)(h * 0.10f), (int)(h * 0.05f), BLUSH, 115);
    gfx_ellipse(ox + (int)(rx * 0.68f), blush_y, (int)(h * 0.10f), (int)(h * 0.05f), BLUSH, 115);

    int mouth_y = oy + (int)(ry * 0.54f);
    int mouth_r = (int)(h * 0.065f) + 1;
    switch (s_mood) {
        case ASTOLFO_HAPPY:
            gfx_ellipse(ox, mouth_y, mouth_r, mouth_r, EYE_DEEP, 235);
            gfx_ellipse(ox, mouth_y + mouth_r / 2, mouth_r / 2, mouth_r / 2, BLUSH, 235);
            break;
        case ASTOLFO_SAD:
            gfx_arc(ox, mouth_y + mouth_r, mouth_r, line_w, 35, 145, LINE_C, 230);
            break;
        case ASTOLFO_SLEEP:
            gfx_ellipse(ox, mouth_y, mouth_r / 2 + 1, mouth_r / 2 + 1, LINE_C, 180);
            break;
        case ASTOLFO_THINKING:
            gfx_line(ox - mouth_r / 2, mouth_y, ox + mouth_r / 2, mouth_y, line_w, LINE_C, 200);
            break;
        default:
            gfx_arc(ox, mouth_y - mouth_r / 2, mouth_r, line_w, 200, 340, LINE_C, 220);
            break;
    }

    /* --- значки настроения рядом с головой --- */
    switch (s_mood) {
        case ASTOLFO_HAPPY: {
            float p = pulse(t, 700.0f, 0.0f);
            gfx_sparkle(ox - (int)(rx * 1.35f), oy - (int)(ry * 0.7f),
                        (int)(h * 0.09f) + 1, CAPE, (uint8_t)(120 + p * 135));
            gfx_heart(ox + (int)(rx * 1.25f), oy - (int)(ry * 0.9f) - (int)(p * h * 0.08f),
                      (int)(h * 0.16f), TH->accent, 230);
            break;
        }
        case ASTOLFO_THINKING: {
            int n = (int)((t / 350) % 3) + 1;
            for (int i = 0; i < n; i++) {
                gfx_circle(ox + (int)(rx * 0.95f) + i * (int)(h * 0.11f),
                           y - (int)(h * 0.03f), (int)(h * 0.035f) + 1, TH->card, 235);
            }
            break;
        }
        case ASTOLFO_SLEEP: {
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
