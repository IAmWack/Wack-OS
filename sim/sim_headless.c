/*
 * Безоконная обвязка: тот же код прошивки, но экран пишется в файл,
 * энкодер кормится из скрипта, а окна нет вовсе.
 *
 * Используется и снимками экранов (sim_shot.c), и тестовым прогоном по
 * живой плате (sim_test.c) — чтобы заглушки не расползались копиями.
 */
#include "sim_headless.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wackos_protocol.h"
#include "wackos_gfx.h"
#include "display.h"
#include "encoder.h"
#include "ui.h"

static const char *s_shot_name = NULL;
static wackos_encoder_event_t s_pending;
static bool s_has_pending = false;

/* ---- экран: кадр уходит в файл, когда его попросили ---- */

void display_init(void) {}
void display_set_brightness(uint8_t percent) { (void)percent; }

void display_flush(void)
{
    if (!s_shot_name) {
        return;
    }
    char path[128];
    snprintf(path, sizeof(path), "shots/%s.ppm", s_shot_name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        s_shot_name = NULL;
        return;
    }
    const uint16_t *fb = gfx_framebuffer();
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        int r = (fb[i] >> 11) & 0x1F, g = (fb[i] >> 5) & 0x3F, b = fb[i] & 0x1F;
        unsigned char px[3] = { (unsigned char)((r << 3) | (r >> 2)),
                                (unsigned char)((g << 2) | (g >> 4)),
                                (unsigned char)((b << 3) | (b >> 2)) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    printf("   снимок: shots/%s.ppm\n", s_shot_name);
    s_shot_name = NULL; /* один кадр на запрос */
}

/* ---- энкодер: события по скрипту ---- */

void encoder_init(void) {}

bool encoder_poll(wackos_encoder_event_t *out_event, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (!s_has_pending) {
        return false;
    }
    *out_event = s_pending;
    s_has_pending = false;
    return true;
}

/* ---- окно: его нет, но транспорт умеет его прокручивать ---- */

bool sim_window_alive(void) { return true; }
bool sim_window_pump(uint32_t timeout_ms) { sim_sleep_ms(timeout_ms); return true; }

/* ---- управление сценарием ---- */

void headless_idle(int frames)
{
    for (int i = 0; i < frames; i++) {
        sim_sleep_ms(16);
        ui_tick();
    }
}

void headless_act(wackos_encoder_event_t ev)
{
    /* Пауза между событиями: интерфейс ускоряет прокрутку, когда крутят
     * быстро, и без паузы сценарий листал бы по три пункта за щелчок. */
    sim_sleep_ms(180);
    s_pending = ev;
    s_has_pending = true;
    ui_tick();
    headless_idle(6);
}

void headless_shoot(const char *name)
{
    headless_idle(20);
    s_shot_name = name;
    ui_tick();
}

const wackos_transport_t *headless_connect(int argc, char **argv,
                                           const char **out_port)
{
    const char *port = NULL;
    int baud = 115200;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = argv[++i];
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            baud = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            g_sim_verbose = true;
        }
    }

    if (out_port) {
        *out_port = port;
    }

    if (system("mkdir -p shots") != 0) {
        return NULL;
    }

    if (port) {
        return sim_serial_open(port, baud);
    }
    sim_slave_init();
    return sim_transport_master();
}
