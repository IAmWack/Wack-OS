/**
 * @file    sim_window.c
 * @author  IAmWack
 * @brief   Окно симулятора на голом Xlib — заменяет ST7789 X11-окном с "корпусом".
 *
 * Специально без SDL и прочих зависимостей, чтобы "сделал make и
 * запустил", без установки пакетов. Рисует корпус устройства: экран
 * 172x320 (кадр из wackos_gfx, увеличенный в scale раз, без
 * сглаживания — чтобы пиксели были ровно те же, что уедут на ST7789),
 * ручку энкодера и подсказку по клавишам.
 */
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/select.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "wackos_gfx.h"
#include "ui.h"

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define PAD_X       22
#define PAD_TOP     22
#define PANEL_H     104

/* ============================================================================
 *  Static state
 * ============================================================================ */

static Display *s_dpy = NULL;
static Window   s_win = 0;
static GC       s_gc = NULL;
static XImage  *s_img = NULL;
static Atom     s_wm_delete;
static uint32_t *s_pixels = NULL;

static int s_scale = 2;
static int s_win_w = 0, s_win_h = 0;
static bool s_alive = false;

static float s_knob_angle = 0.0f;   /* текущий угол ручки, радианы */
static float s_knob_target = 0.0f;  /* куда она едет */
static uint32_t s_press_at = 0;     /* когда нажали кнопку — для подсветки */
static int s_shot_counter = 0;

/* ============================================================================
 *  Static helpers — вспомогательное рисование прямо в окно (32bpp)
 * ============================================================================ */

/** @brief Записать один пиксель окна (с отсечением по границам). @param x Координата X. @param y Координата Y. @param rgb Цвет 0xRRGGBB. */
static inline void put(int x, int y, uint32_t rgb)
{
    if (x < 0 || y < 0 || x >= s_win_w || y >= s_win_h) {
        return;
    }
    s_pixels[y * s_win_w + x] = rgb;
}

/** @brief Перевести цвет RGB565 (кадровый буфер wackos_gfx) в RGB888 (окно X11). @param c Цвет RGB565. @return Цвет 0xRRGGBB. */
static uint32_t rgb565_to_888(uint16_t c)
{
    uint32_t r = (c >> 11) & 0x1F;
    uint32_t g = (c >> 5) & 0x3F;
    uint32_t b = c & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return (r << 16) | (g << 8) | b;
}

/** @brief Закрасить круг сплошным цветом. @param cx Центр X. @param cy Центр Y. @param r Радиус. @param rgb Цвет. */
static void fill_circle(int cx, int cy, int r, uint32_t rgb)
{
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r) {
                put(x, y, rgb);
            }
        }
    }
}

/** @brief Нарисовать фон-"корпус" устройства (градиент + рамка вокруг экрана). */
static void draw_body(void)
{
    /* Корпус: мягкий розово-сиреневый градиент. */
    for (int y = 0; y < s_win_h; y++) {
        int t = y * 255 / (s_win_h - 1);
        uint32_t r = 250 - t * 20 / 255;
        uint32_t g = 226 - t * 30 / 255;
        uint32_t b = 240 + t * 10 / 255;
        for (int x = 0; x < s_win_w; x++) {
            s_pixels[y * s_win_w + x] = (r << 16) | (g << 8) | b;
        }
    }

    /* Рамка вокруг экрана. */
    int sx = PAD_X, sy = PAD_TOP;
    int sw = GFX_W * s_scale, sh = GFX_H * s_scale;
    for (int i = 1; i <= 6; i++) {
        uint32_t c = (i <= 3) ? 0x5A2A4B : 0xFF9FCB;
        for (int x = sx - i; x <= sx + sw + i - 1; x++) {
            put(x, sy - i, c);
            put(x, sy + sh + i - 1, c);
        }
        for (int y = sy - i; y <= sy + sh + i - 1; y++) {
            put(sx - i, y, c);
            put(sx + sw + i - 1, y, c);
        }
    }
}

/** @brief Нарисовать ручку энкодера (положение угла + подсветка при нажатии). */
static void draw_knob(void)
{
    int cx = s_win_w - PAD_X - 34;
    int cy = PAD_TOP + GFX_H * s_scale + PANEL_H / 2;
    int r = 30;

    bool pressed = (sim_now_ms() - s_press_at) < 140;

    fill_circle(cx + 2, cy + 3, r, 0xE0AFC8);                      /* тень */
    fill_circle(cx, cy, r, pressed ? 0xFF5C9E : 0xFFFFFF);         /* тело */
    fill_circle(cx, cy, r - 5, pressed ? 0xFF8CBB : 0xFFE3F0);

    /* Насечка, показывающая поворот. */
    for (int i = 0; i < r - 8; i++) {
        int x = cx + (int)(sinf(s_knob_angle) * i);
        int y = cy - (int)(cosf(s_knob_angle) * i);
        fill_circle(x, y, 2, 0xE84A98);
    }
    fill_circle(cx, cy, 4, 0xE84A98);
}

/** @brief Индикатор состояния провода: зелёный/красный/жёлтый кружок у левого края панели. */
static void draw_link_led(void)
{
    int cx = PAD_X + 14;
    int cy = PAD_TOP + GFX_H * s_scale + 24;
    /* С живой платой состояние знает только сам интерфейс (ответила
     * она на последний запрос или нет), с фальшивой — мы сами. */
    uint32_t c;
    if (sim_serial_active()) {
        c = ui_link_online() ? 0x4ED8A0 : 0xFF6B6B;
    } else {
        c = sim_slave_connected() ? (sim_slave_muted() ? 0xFFC04A : 0x4ED8A0) : 0xFF6B6B;
    }
    fill_circle(cx, cy, 7, 0xFFFFFF);
    fill_circle(cx, cy, 5, c);
}

/** @brief Нарисовать две строки подсказки: состояние связи и управление клавиатурой/мышью. */
static void draw_hints(void)
{
    char line1[128];
    char line2[192];

    if (sim_serial_active()) {
        snprintf(line1, sizeof(line1), "живая плата: %s   %s",
                 sim_serial_port(), ui_link_online() ? "отвечает" : "молчит");
    } else {
        const char *state = !sim_slave_connected() ? "отключён"
                            : (sim_slave_muted() ? "перезагружается" : "на связи");
        snprintf(line1, sizeof(line1), "фальшивый slave: %s   задержка провода: %u мс",
                 state, sim_transport_latency());
    }
    snprintf(line2, sizeof(line2),
             "колесо/стрелки - крутить, Enter/ЛКМ - выбрать, Backspace/ПКМ - назад,"
             " O - отключить slave, L - лаг, S - снимок, Q - выход");

    int base_y = PAD_TOP + GFX_H * s_scale + 22;
    XSetForeground(s_dpy, s_gc, 0x5A2A4B);
    XDrawString(s_dpy, s_win, s_gc, PAD_X + 30, base_y + 4, line1, (int)strlen(line1));
    XDrawString(s_dpy, s_win, s_gc, PAD_X, base_y + 26, line2, (int)strlen(line2));
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Открыть X11-окно симулятора.
 * @param   scale  Масштаб пикселя (0 — подобрать под высоту экрана, 1..4).
 * @return  false при ошибке (нет X-дисплея или не создалось изображение).
 */
bool sim_window_open(int scale)
{
    s_dpy = XOpenDisplay(NULL);
    if (!s_dpy) {
        fprintf(stderr, "не открылся X-дисплей (DISPLAY=%s)\n", getenv("DISPLAY"));
        return false;
    }

    int screen = DefaultScreen(s_dpy);

    /* Масштаб по умолчанию — максимальный, который влезает в экран. */
    if (scale <= 0) {
        int avail = DisplayHeight(s_dpy, screen) - 160;
        scale = avail / GFX_H;
        if (scale < 1) scale = 1;
        if (scale > 4) scale = 4;
    }
    s_scale = scale;

    s_win_w = GFX_W * s_scale + 2 * PAD_X;
    s_win_h = GFX_H * s_scale + PAD_TOP + PANEL_H;

    s_win = XCreateSimpleWindow(s_dpy, RootWindow(s_dpy, screen), 0, 0,
                                (unsigned)s_win_w, (unsigned)s_win_h, 0,
                                BlackPixel(s_dpy, screen), WhitePixel(s_dpy, screen));
    XStoreName(s_dpy, s_win, "Wack-OS  |  симулятор экрана master");
    XSelectInput(s_dpy, s_win, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);

    s_wm_delete = XInternAtom(s_dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(s_dpy, s_win, &s_wm_delete, 1);

    /* Запретить менять размер окна: масштаб целочисленный, тянуть нечего. */
    XSizeHints *hints = XAllocSizeHints();
    hints->flags = PMinSize | PMaxSize;
    hints->min_width = hints->max_width = s_win_w;
    hints->min_height = hints->max_height = s_win_h;
    XSetWMNormalHints(s_dpy, s_win, hints);
    XFree(hints);

    XMapWindow(s_dpy, s_win);
    s_gc = XCreateGC(s_dpy, s_win, 0, NULL);

    s_pixels = calloc((size_t)s_win_w * s_win_h, sizeof(uint32_t));
    s_img = XCreateImage(s_dpy, DefaultVisual(s_dpy, screen), 24, ZPixmap, 0,
                         (char *)s_pixels, (unsigned)s_win_w, (unsigned)s_win_h, 32, 0);
    if (!s_img) {
        fprintf(stderr, "не создалось изображение X11\n");
        return false;
    }

    s_alive = true;
    return true;
}

/** @brief Закрыть окно и освободить ресурсы X11. */
void sim_window_close(void)
{
    if (!s_dpy) {
        return;
    }
    if (s_img) {
        XDestroyImage(s_img); /* освобождает и s_pixels */
        s_img = NULL;
        s_pixels = NULL;
    }
    XCloseDisplay(s_dpy);
    s_dpy = NULL;
    s_alive = false;
}

/** @brief Открыто ли сейчас окно. @return true, пока окно живо. */
bool sim_window_alive(void)
{
    return s_alive;
}

/** @brief Повернуть нарисованную ручку энкодера (анимация доезжает плавно). @param dir +1/-1 — направление щелчка. */
void sim_window_note_turn(int dir)
{
    s_knob_target += dir * 0.52f; /* примерно щелчок KY-040 */
}

/** @brief Отметить момент нажатия кнопки энкодера (для подсветки в draw_knob()). */
void sim_window_note_press(void)
{
    s_press_at = sim_now_ms();
}

/** @brief Показать кадр (172x320 RGB565) с "корпусом" вокруг него. @param fb Кадровый буфер wackos_gfx. */
void sim_window_present(const uint16_t *fb)
{
    if (!s_alive) {
        return;
    }

    /* Ручка доезжает до целевого угла плавно. */
    s_knob_angle += (s_knob_target - s_knob_angle) * 0.35f;

    draw_body();

    /* Экран: честный nearest-neighbour, без сглаживания. */
    int sx = PAD_X, sy = PAD_TOP;
    for (int y = 0; y < GFX_H; y++) {
        for (int x = 0; x < GFX_W; x++) {
            uint32_t c = rgb565_to_888(fb[y * GFX_W + x]);
            int px = sx + x * s_scale;
            int py = sy + y * s_scale;
            for (int dy = 0; dy < s_scale; dy++) {
                uint32_t *row = &s_pixels[(py + dy) * s_win_w + px];
                for (int dx = 0; dx < s_scale; dx++) {
                    row[dx] = c;
                }
            }
        }
    }

    draw_knob();
    draw_link_led();

    XPutImage(s_dpy, s_win, s_gc, s_img, 0, 0, 0, 0, (unsigned)s_win_w, (unsigned)s_win_h);
    draw_hints();
    XFlush(s_dpy);
}

/** @brief Сохранить текущий кадр в PPM рядом с бинарником (клавиша S). @param fb Кадровый буфер wackos_gfx. */
void sim_window_screenshot(const uint16_t *fb)
{
    char name[64];
    snprintf(name, sizeof(name), "wackos-screen-%02d.ppm", ++s_shot_counter);
    FILE *f = fopen(name, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        uint32_t c = rgb565_to_888(fb[i]);
        unsigned char px[3] = { (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    printf("[sim] снимок экрана: %s\n", name);
}

/* ============================================================================
 *  Static helpers — обработка событий
 * ============================================================================ */

/** @brief Обработать одно нажатие клавиши (стрелки/Enter/Backspace/O/L/S/Q). @param key Код клавиши X11. */
static void handle_key(KeySym key)
{
    switch (key) {
        case XK_Up:
        case XK_Left:
            sim_input_push(WACKOS_ENC_CCW);
            sim_window_note_turn(-1);
            break;

        case XK_Down:
        case XK_Right:
            sim_input_push(WACKOS_ENC_CW);
            sim_window_note_turn(1);
            break;

        case XK_Return:
        case XK_KP_Enter:
        case XK_space:
            sim_input_push(WACKOS_ENC_BTN);
            sim_window_note_press();
            break;

        /* Длинное удержание кнопки энкодера = "назад". Держать клавишу
         * в X11 неудобно (автоповтор), поэтому для него отдельная
         * клавиша и правая кнопка мыши. */
        case XK_BackSpace:
            sim_input_push(WACKOS_ENC_BTN_LONG);
            sim_window_note_press();
            break;

        /* O и L управляют фальшивым проводом; с живой платой обрыв
         * изображается выдёргиванием USB, а не клавишей. */
        case XK_o:
            if (sim_serial_active()) {
                printf("[sim] говорю с живой платой — выдерни USB, если нужен обрыв\n");
                break;
            }
            sim_slave_set_connected(!sim_slave_connected());
            printf("[sim] slave %s\n", sim_slave_connected() ? "подключён" : "отключён");
            break;

        case XK_l:
            if (sim_serial_active()) {
                printf("[sim] задержка задаётся проводом, а не клавишей\n");
                break;
            }
            sim_transport_set_latency(sim_transport_latency() > 50 ? 6 : 120);
            printf("[sim] задержка провода: %u мс\n", sim_transport_latency());
            break;

        case XK_s:
        case XK_S:
            sim_window_screenshot(gfx_framebuffer());
            break;

        case XK_q:
        case XK_Escape:
            s_alive = false;
            break;

        default:
            break;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Обработать события окна (клавиатура/мышь/закрытие) не дольше timeout_ms.
 * @param   timeout_ms  Верхняя граница ожидания.
 * @return  false — окно закрыли.
 */
bool sim_window_pump(uint32_t timeout_ms)
{
    if (!s_alive) {
        return false;
    }

    uint32_t deadline = sim_now_ms() + timeout_ms;
    for (;;) {
        while (XPending(s_dpy) > 0) {
            XEvent ev;
            XNextEvent(s_dpy, &ev);
            switch (ev.type) {
                case KeyPress: {
                    KeySym key = XLookupKeysym(&ev.xkey, (ev.xkey.state & ShiftMask) ? 1 : 0);
                    handle_key(key);
                    break;
                }
                case ButtonPress:
                    /* Колесо мыши — самый естественный способ крутить энкодер. */
                    if (ev.xbutton.button == 4) {
                        sim_input_push(WACKOS_ENC_CCW);
                        sim_window_note_turn(-1);
                    } else if (ev.xbutton.button == 5) {
                        sim_input_push(WACKOS_ENC_CW);
                        sim_window_note_turn(1);
                    } else if (ev.xbutton.button == 1) {
                        sim_input_push(WACKOS_ENC_BTN);
                        sim_window_note_press();
                    } else if (ev.xbutton.button == 3) {
                        sim_input_push(WACKOS_ENC_BTN_LONG);
                        sim_window_note_press();
                    }
                    break;
                case Expose:
                    draw_hints();
                    break;
                case ClientMessage:
                    if ((Atom)ev.xclient.data.l[0] == s_wm_delete) {
                        s_alive = false;
                        return false;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!s_alive) {
            return false;
        }
        uint32_t now = sim_now_ms();
        if (now >= deadline) {
            return true;
        }

        /* Ждём либо события X, либо конца кадра — без busy-wait. */
        int fd = ConnectionNumber(s_dpy);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        uint32_t left = deadline - now;
        struct timeval tv = { .tv_sec = left / 1000, .tv_usec = (long)(left % 1000) * 1000 };
        select(fd + 1, &fds, NULL, NULL, &tv);
    }
}
