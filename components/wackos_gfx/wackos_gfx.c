#include "wackos_gfx.h"

#include <math.h>
#include <string.h>

/*
 * Кадровый буфер. 172*320*2 = 110 КБ — на ESP32-S3 это спокойно ложится
 * в PSRAM (её 8 МБ), а на ноутбуке просто статический массив. Держим
 * один общий буфер, а не рисуем сразу в панель: так экран не мерцает
 * (за кадр меняется всё разом) и так симулятор видит ровно ту же
 * картинку, что и железо.
 */
static uint16_t s_fb[GFX_W * GFX_H];

uint16_t *gfx_framebuffer(void)
{
    return s_fb;
}

/* ---- Цвет ---- */

static inline void rgb565_split(uint16_t c, int *r, int *g, int *b)
{
    *r = (c >> 11) & 0x1F;
    *g = (c >> 5) & 0x3F;
    *b = c & 0x1F;
}

static inline uint16_t rgb565_join(int r, int g, int b)
{
    return (uint16_t)((r << 11) | (g << 5) | b);
}

uint16_t gfx_mix(uint16_t a, uint16_t b, uint8_t t)
{
    int ar, ag, ab, br, bg, bb;
    rgb565_split(a, &ar, &ag, &ab);
    rgb565_split(b, &br, &bg, &bb);
    return rgb565_join(ar + ((br - ar) * t) / 255,
                       ag + ((bg - ag) * t) / 255,
                       ab + ((bb - ab) * t) / 255);
}

uint16_t gfx_lighten(uint16_t c, uint8_t amount)
{
    return gfx_mix(c, 0xFFFF, amount);
}

uint16_t gfx_darken(uint16_t c, uint8_t amount)
{
    return gfx_mix(c, 0x0000, amount);
}

/* ---- Примитивы ---- */

void gfx_clear(uint16_t color)
{
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        s_fb[i] = color;
    }
}

void gfx_pixel(int x, int y, uint16_t color, uint8_t alpha)
{
    if (x < 0 || y < 0 || x >= GFX_W || y >= GFX_H || alpha == 0) {
        return;
    }
    uint16_t *p = &s_fb[y * GFX_W + x];
    *p = (alpha == 255) ? color : gfx_mix(*p, color, alpha);
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t color, uint8_t alpha)
{
    if (w <= 0 || h <= 0 || alpha == 0) {
        return;
    }
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GFX_W ? GFX_W : x + w;
    int y1 = y + h > GFX_H ? GFX_H : y + h;

    for (int py = y0; py < y1; py++) {
        uint16_t *row = &s_fb[py * GFX_W];
        for (int px = x0; px < x1; px++) {
            row[px] = (alpha == 255) ? color : gfx_mix(row[px], color, alpha);
        }
    }
}

void gfx_gradient_v(int y0, int y1, uint16_t top, uint16_t bottom)
{
    if (y1 <= y0) {
        return;
    }
    int span = y1 - y0;
    for (int y = y0; y < y1; y++) {
        if (y < 0 || y >= GFX_H) {
            continue;
        }
        uint8_t t = (uint8_t)((y - y0) * 255 / (span - 1 > 0 ? span - 1 : 1));
        uint16_t c = gfx_mix(top, bottom, t);
        uint16_t *row = &s_fb[y * GFX_W];
        for (int x = 0; x < GFX_W; x++) {
            row[x] = c;
        }
    }
}

/*
 * Покрытие пикселя скруглённым прямоугольником, 0..255.
 * Считаем расстояние до "скелета" фигуры — внутри прямых участков оно
 * нулевое, у углов работает как окружность. Полпикселя мягкости по краю
 * и даёт сглаживание, без него углы выглядят лесенкой.
 */
static uint8_t round_rect_cov(float px, float py, float x, float y,
                              float w, float h, float r)
{
    float cx = px - (x + r);
    float cy = py - (y + r);
    float mx = (x + w - r) - px;
    float my = (y + h - r) - py;

    float dx = 0.0f, dy = 0.0f;
    if (cx < 0.0f) dx = -cx;
    if (mx < 0.0f) dx = -mx;
    if (cy < 0.0f) dy = -cy;
    if (my < 0.0f) dy = -my;

    float d = sqrtf(dx * dx + dy * dy) - r;
    float cov = 0.5f - d;
    if (cov <= 0.0f) return 0;
    if (cov >= 1.0f) return 255;
    return (uint8_t)(cov * 255.0f);
}

void gfx_round_rect(int x, int y, int w, int h, int r, uint16_t color, uint8_t alpha)
{
    gfx_round_rect_gradient(x, y, w, h, r, color, color, alpha);
}

void gfx_round_rect_gradient(int x, int y, int w, int h, int r,
                             uint16_t top, uint16_t bottom, uint8_t alpha)
{
    if (w <= 0 || h <= 0 || alpha == 0) {
        return;
    }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GFX_W ? GFX_W : x + w;
    int y1 = y + h > GFX_H ? GFX_H : y + h;

    for (int py = y0; py < y1; py++) {
        uint8_t t = (h > 1) ? (uint8_t)((py - y) * 255 / (h - 1)) : 0;
        uint16_t c = (top == bottom) ? top : gfx_mix(top, bottom, t);
        for (int px = x0; px < x1; px++) {
            uint8_t cov = round_rect_cov(px + 0.5f, py + 0.5f,
                                         (float)x, (float)y, (float)w, (float)h, (float)r);
            if (cov) {
                gfx_pixel(px, py, c, (uint8_t)(cov * alpha / 255));
            }
        }
    }
}

void gfx_round_rect_outline(int x, int y, int w, int h, int r, int t,
                            uint16_t color, uint8_t alpha)
{
    if (w <= 0 || h <= 0 || t <= 0 || alpha == 0) {
        return;
    }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GFX_W ? GFX_W : x + w;
    int y1 = y + h > GFX_H ? GFX_H : y + h;

    int ir = r - t < 0 ? 0 : r - t;
    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            uint8_t outer = round_rect_cov(px + 0.5f, py + 0.5f,
                                           (float)x, (float)y, (float)w, (float)h, (float)r);
            if (!outer) {
                continue;
            }
            uint8_t inner = round_rect_cov(px + 0.5f, py + 0.5f,
                                           (float)(x + t), (float)(y + t),
                                           (float)(w - 2 * t), (float)(h - 2 * t), (float)ir);
            int cov = outer - inner;
            if (cov > 0) {
                gfx_pixel(px, py, color, (uint8_t)(cov * alpha / 255));
            }
        }
    }
}

void gfx_circle(int cx, int cy, int r, uint16_t color, uint8_t alpha)
{
    if (r <= 0 || alpha == 0) {
        return;
    }
    for (int py = cy - r - 1; py <= cy + r + 1; py++) {
        for (int px = cx - r - 1; px <= cx + r + 1; px++) {
            float dx = px + 0.5f - cx;
            float dy = py + 0.5f - cy;
            float cov = 0.5f - (sqrtf(dx * dx + dy * dy) - r);
            if (cov <= 0.0f) {
                continue;
            }
            if (cov > 1.0f) cov = 1.0f;
            gfx_pixel(px, py, color, (uint8_t)(cov * alpha));
        }
    }
}

void gfx_ellipse(int cx, int cy, int rx, int ry, uint16_t color, uint8_t alpha)
{
    if (rx <= 0 || ry <= 0 || alpha == 0) {
        return;
    }
    for (int py = cy - ry - 1; py <= cy + ry + 1; py++) {
        for (int px = cx - rx - 1; px <= cx + rx + 1; px++) {
            float dx = (px + 0.5f - cx) / rx;
            float dy = (py + 0.5f - cy) / ry;
            float d = sqrtf(dx * dx + dy * dy);
            /* Переводим "расстояние в долях радиуса" обратно в пиксели,
             * иначе у вытянутых эллипсов край размывался бы неровно. */
            float scale = (rx < ry) ? (float)rx : (float)ry;
            float cov = (1.0f - d) * scale + 0.5f;
            if (cov <= 0.0f) {
                continue;
            }
            if (cov > 1.0f) cov = 1.0f;
            gfx_pixel(px, py, color, (uint8_t)(cov * alpha));
        }
    }
}

void gfx_arc(int cx, int cy, int r, int t, int a0, int a1, uint16_t color, uint8_t alpha)
{
    if (r <= 0 || t <= 0 || alpha == 0) {
        return;
    }
    float half = t * 0.5f;
    float rad0 = a0 * 3.14159265f / 180.0f;
    float rad1 = a1 * 3.14159265f / 180.0f;
    if (rad1 < rad0) {
        float tmp = rad0; rad0 = rad1; rad1 = tmp;
    }

    for (int py = cy - r - t; py <= cy + r + t; py++) {
        for (int px = cx - r - t; px <= cx + r + t; px++) {
            float dx = px + 0.5f - cx;
            float dy = py + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy);
            float band = half - fabsf(d - r) + 0.5f;
            if (band <= 0.0f) {
                continue;
            }
            if (band > 1.0f) band = 1.0f;

            float ang = atan2f(-dy, dx); /* экранный Y вниз, а углы считаем как в математике */
            if (ang < 0.0f) {
                ang += 6.2831853f;
            }
            if (ang < rad0 || ang > rad1) {
                /* тот же угол, но на круг больше — дуга может пересекать 0 */
                float alt = ang + 6.2831853f;
                if (alt < rad0 || alt > rad1) {
                    continue;
                }
            }
            gfx_pixel(px, py, color, (uint8_t)(band * alpha));
        }
    }
}

void gfx_line(int x0, int y0, int x1, int y1, int w, uint16_t color, uint8_t alpha)
{
    if (w <= 0 || alpha == 0) {
        return;
    }
    float half = w * 0.5f;
    int minx = (x0 < x1 ? x0 : x1) - w, maxx = (x0 > x1 ? x0 : x1) + w;
    int miny = (y0 < y1 ? y0 : y1) - w, maxy = (y0 > y1 ? y0 : y1) + w;

    float vx = (float)(x1 - x0), vy = (float)(y1 - y0);
    float len2 = vx * vx + vy * vy;

    for (int py = miny; py <= maxy; py++) {
        for (int px = minx; px <= maxx; px++) {
            float wx = px + 0.5f - x0, wy = py + 0.5f - y0;
            float t = (len2 > 0.0f) ? (wx * vx + wy * vy) / len2 : 0.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float dx = wx - vx * t, dy = wy - vy * t;
            float cov = half - sqrtf(dx * dx + dy * dy) + 0.5f;
            if (cov <= 0.0f) {
                continue;
            }
            if (cov > 1.0f) cov = 1.0f;
            gfx_pixel(px, py, color, (uint8_t)(cov * alpha));
        }
    }
}

/* ---- Милота ---- */

/*
 * Сердце — по классической неявной кривой (x^2+y^2-1)^3 - x^2*y^3 = 0.
 * Пиксель считаем 2x2 подвыборками: фигура маленькая (10-20px), без
 * сглаживания края были бы рваные.
 */
static inline bool heart_inside(float u, float v)
{
    float a = u * u + v * v - 1.0f;
    return (a * a * a - u * u * v * v * v) <= 0.0f;
}

void gfx_heart(int cx, int cy, int h, uint16_t color, uint8_t alpha)
{
    if (h <= 0 || alpha == 0) {
        return;
    }
    float half = h * 0.5f;
    float halfw = half * 1.12f; /* сердце чуть шире, чем выше */

    for (int py = cy - h; py <= cy + h; py++) {
        for (int px = cx - h; px <= cx + h; px++) {
            int hits = 0;
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    float u = (px + 0.25f + sx * 0.5f - cx) / halfw;
                    float v = (cy + 0.12f * h - (py + 0.25f + sy * 0.5f)) / half;
                    if (heart_inside(u, v)) {
                        hits++;
                    }
                }
            }
            if (hits) {
                gfx_pixel(px, py, color, (uint8_t)(hits * 255 / 4 * alpha / 255));
            }
        }
    }
}

/*
 * Искорка — астроида |u|^(2/3) + |v|^(2/3) <= 1: четыре тонких луча
 * с пухлой серединкой, ровно как блик в аниме-глазах.
 */
void gfx_sparkle(int cx, int cy, int r, uint16_t color, uint8_t alpha)
{
    if (r <= 0 || alpha == 0) {
        return;
    }
    for (int py = cy - r - 1; py <= cy + r + 1; py++) {
        for (int px = cx - r - 1; px <= cx + r + 1; px++) {
            int hits = 0;
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    float u = fabsf((px + 0.25f + sx * 0.5f - cx) / r);
                    float v = fabsf((py + 0.25f + sy * 0.5f - cy) / r);
                    if (powf(u, 0.55f) + powf(v, 0.55f) <= 1.0f) {
                        hits++;
                    }
                }
            }
            if (hits) {
                gfx_pixel(px, py, color, (uint8_t)(hits * 255 / 4 * alpha / 255));
            }
        }
    }
}

/* Знак векторного произведения — для теста "точка внутри треугольника". */
static inline float edge_sign(float px, float py, float ax, float ay, float bx, float by)
{
    return (px - bx) * (ay - by) - (ax - bx) * (py - by);
}

static bool point_in_tri(float px, float py,
                         float x0, float y0, float x1, float y1, float x2, float y2)
{
    float d0 = edge_sign(px, py, x0, y0, x1, y1);
    float d1 = edge_sign(px, py, x1, y1, x2, y2);
    float d2 = edge_sign(px, py, x2, y2, x0, y0);
    bool neg = (d0 < 0.0f) || (d1 < 0.0f) || (d2 < 0.0f);
    bool pos = (d0 > 0.0f) || (d1 > 0.0f) || (d2 > 0.0f);
    return !(neg && pos);
}

void gfx_triangle(int x0, int y0, int x1, int y1, int x2, int y2,
                  uint16_t color, uint8_t alpha)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

    for (int py = miny - 1; py <= maxy + 1; py++) {
        for (int px = minx - 1; px <= maxx + 1; px++) {
            int hits = 0;
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    if (point_in_tri(px + 0.25f + sx * 0.5f, py + 0.25f + sy * 0.5f,
                                     (float)x0, (float)y0, (float)x1, (float)y1,
                                     (float)x2, (float)y2)) {
                        hits++;
                    }
                }
            }
            if (hits) {
                gfx_pixel(px, py, color, (uint8_t)(hits * 255 / 4 * alpha / 255));
            }
        }
    }
}

/*
 * Бантик: две треугольные петли, сходящиеся к узелку. Треугольники, а
 * не эллипсы — на 16-20 пикселях эллипсы сливаются в бесформенное
 * пятно, а острые петли читаются даже такими мелкими.
 */
void gfx_bow(int cx, int cy, int w, uint16_t color, uint8_t alpha)
{
    if (w <= 0 || alpha == 0) {
        return;
    }
    float half = w * 0.5f;
    float h = w * 0.62f;
    float knot = w * 0.16f;

    for (int py = cy - (int)h - 1; py <= cy + (int)h + 1; py++) {
        for (int px = cx - (int)half - 1; px <= cx + (int)half + 1; px++) {
            int hits = 0;
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    float fx = px + 0.25f + sx * 0.5f;
                    float fy = py + 0.25f + sy * 0.5f;
                    bool in =
                        point_in_tri(fx, fy, cx - half, cy - h * 0.5f,
                                     cx - half, cy + h * 0.5f, cx - knot * 0.5f, (float)cy) ||
                        point_in_tri(fx, fy, cx + half, cy - h * 0.5f,
                                     cx + half, cy + h * 0.5f, cx + knot * 0.5f, (float)cy);
                    if (in) {
                        hits++;
                    }
                }
            }
            if (hits) {
                gfx_pixel(px, py, color, (uint8_t)(hits * 255 / 4 * alpha / 255));
            }
        }
    }

    /* Узелок посередине — чуть светлее, чтобы петли не слипались. */
    gfx_circle(cx, cy, (int)knot < 2 ? 2 : (int)knot, gfx_lighten(color, 60), alpha);
}

void gfx_cat_ear(int x, int y, int w, int h, bool flip,
                 uint16_t outer, uint16_t inner, uint8_t alpha)
{
    /* Треугольник с наклоном: flip зеркалит его (левое/правое ушко). */
    float ax = flip ? (float)(x + w) : (float)x;          /* основание, внешний угол */
    float bx = flip ? (float)x : (float)(x + w);          /* основание, внутренний угол */
    float tipx = flip ? (float)(x + w * 0.25f) : (float)(x + w * 0.75f);

    for (int py = y; py <= y + h; py++) {
        for (int px = x - 1; px <= x + w + 1; px++) {
            int hits = 0, inner_hits = 0;
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    float fx = px + 0.25f + sx * 0.5f;
                    float fy = py + 0.25f + sy * 0.5f;
                    float t = (float)(y + h - fy) / (float)h; /* 0 у основания, 1 у кончика */
                    if (t < 0.0f || t > 1.0f) continue;
                    float l = ax + (tipx - ax) * t;
                    float r = bx + (tipx - bx) * t;
                    float lo = l < r ? l : r;
                    float hi = l < r ? r : l;
                    if (fx >= lo && fx <= hi) {
                        hits++;
                        float pad = (hi - lo) * 0.28f + 1.0f;
                        if (fx >= lo + pad && fx <= hi - pad && t < 0.72f) {
                            inner_hits++;
                        }
                    }
                }
            }
            if (hits) {
                uint16_t c = (inner_hits >= 3) ? inner : outer;
                gfx_pixel(px, py, c, (uint8_t)(hits * 255 / 4 * alpha / 255));
            }
        }
    }
}

void gfx_dim(uint8_t level)
{
    if (level >= 255) {
        return;
    }
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        s_fb[i] = gfx_mix(0x0000, s_fb[i], level);
    }
}

/* ---- Текст ---- */

/* Минимальный UTF-8 декодер: хватает ASCII + кириллицы (2 байта). */
static const char *utf8_next(const char *s, uint32_t *out_cp)
{
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) {
        *out_cp = c;
        return s + 1;
    }
    if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *out_cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
        return s + 2;
    }
    if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *out_cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) |
                  (uint32_t)(s[2] & 0x3F);
        return s + 3;
    }
    *out_cp = '?';
    return s + 1;
}

static void draw_glyph(const wackos_font_t *font, const wackos_glyph_t *g,
                       int x, int y, uint16_t color, uint8_t alpha)
{
    if (g->w == 0 || g->h == 0) {
        return;
    }
    int stride = (g->w + 1) / 2; /* 4 бита на пиксель, строка выровнена на байт */
    const uint8_t *src = font->bitmaps + g->offset;

    for (int row = 0; row < g->h; row++) {
        const uint8_t *line = src + row * stride;
        for (int col = 0; col < g->w; col++) {
            uint8_t nib = (col & 1) ? (line[col >> 1] & 0x0F) : (line[col >> 1] >> 4);
            if (!nib) {
                continue;
            }
            uint8_t a = (uint8_t)(nib * 17);          /* 0..15 -> 0..255 */
            a = (uint8_t)((int)a * alpha / 255);
            gfx_pixel(x + g->left + col, y + g->top + row, color, a);
        }
    }
}

int gfx_text_width(const wackos_font_t *font, const char *utf8)
{
    if (!font || !utf8) {
        return 0;
    }
    int w = 0;
    uint32_t cp;
    while (*utf8) {
        utf8 = utf8_next(utf8, &cp);
        const wackos_glyph_t *g = wackos_font_glyph(font, cp);
        if (g) {
            w += g->advance;
        }
    }
    return w;
}

void gfx_text(const wackos_font_t *font, int x, int y, const char *utf8,
              uint16_t color, uint8_t alpha)
{
    if (!font || !utf8 || alpha == 0) {
        return;
    }
    uint32_t cp;
    while (*utf8) {
        utf8 = utf8_next(utf8, &cp);
        const wackos_glyph_t *g = wackos_font_glyph(font, cp);
        if (!g) {
            continue;
        }
        draw_glyph(font, g, x, y, color, alpha);
        x += g->advance;
    }
}

void gfx_text_center(const wackos_font_t *font, int cx, int y, const char *utf8,
                     uint16_t color, uint8_t alpha)
{
    gfx_text(font, cx - gfx_text_width(font, utf8) / 2, y, utf8, color, alpha);
}

void gfx_text_shadow(const wackos_font_t *font, int x, int y, const char *utf8,
                     uint16_t color, uint16_t shadow, uint8_t alpha)
{
    gfx_text(font, x, y + 1, utf8, shadow, (uint8_t)(alpha * 45 / 100));
    gfx_text(font, x, y, utf8, color, alpha);
}

int gfx_text_ellipsis(const wackos_font_t *font, int x, int y, const char *utf8,
                      int max_w, uint16_t color, uint8_t alpha)
{
    if (!font || !utf8) {
        return 0;
    }
    if (gfx_text_width(font, utf8) <= max_w) {
        gfx_text(font, x, y, utf8, color, alpha);
        return gfx_text_width(font, utf8);
    }

    const wackos_glyph_t *dot = wackos_font_glyph(font, '.');
    int dots_w = dot ? dot->advance * 3 : 0;
    int drawn = 0;
    uint32_t cp;
    const char *p = utf8;
    while (*p) {
        const char *next = utf8_next(p, &cp);
        const wackos_glyph_t *g = wackos_font_glyph(font, cp);
        if (!g) {
            p = next;
            continue;
        }
        if (drawn + g->advance + dots_w > max_w) {
            break;
        }
        draw_glyph(font, g, x + drawn, y, color, alpha);
        drawn += g->advance;
        p = next;
    }
    gfx_text(font, x + drawn, y, "...", color, alpha);
    return drawn + dots_w;
}
