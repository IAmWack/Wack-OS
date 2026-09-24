/**
 * @file    tool_sheet.c
 * @author  IAmWack
 * @brief   Точка входа инструмента "лист персонажа" (./wackos-sheet, make sheet).
 *
 * Рисует Астольфо во всех настроениях и в трёх размерах, чтобы
 * подгонять пропорции, не запуская весь интерфейс.
 *
 *   make sheet && ls shots/astolfo-*.ppm
 */
#include <stdio.h>
#include <stdlib.h>

#include "wackos_gfx.h"
#include "theme.h"
#include "astolfo.h"

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/** @brief Сохранить текущий кадровый буфер в shots/<name>.ppm. @param name Имя файла без расширения. */
static void save(const char *name)
{
    char path[128];
    snprintf(path, sizeof(path), "shots/%s.ppm", name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
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
    printf("%s\n", path);
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief Прогнать все настроения Астольфо и сохранить снимки в shots/. @return 0 при успехе, 1 если не создалась shots/. */
int main(void)
{
    if (system("mkdir -p shots") != 0) {
        return 1;
    }

    static const struct { astolfo_mood_t mood; const char *name; } MOODS[] = {
        { ASTOLFO_IDLE,     "idle" },
        { ASTOLFO_HAPPY,    "happy" },
        { ASTOLFO_SAD,      "sad" },
        { ASTOLFO_THINKING, "thinking" },
        { ASTOLFO_SLEEP,    "sleep" },
    };

    for (size_t i = 0; i < sizeof(MOODS) / sizeof(MOODS[0]); i++) {
        astolfo_tick(1000);
        astolfo_set_mood(MOODS[i].mood);

        gfx_gradient_v(0, GFX_H / 2, TH->bg_top, TH->bg_mid);
        gfx_gradient_v(GFX_H / 2, GFX_H, TH->bg_mid, TH->bg_bot);

        astolfo_draw(GFX_W / 2, 14, 96, 1000);   /* крупный — как на загрузке */
        astolfo_draw(40, 236, 34, 1000);          /* мелкий — как в углу меню */
        astolfo_draw(126, 236, 24, 1000);         /* совсем мелкий — предел читаемости */

        char name[64];
        snprintf(name, sizeof(name), "astolfo-%s", MOODS[i].name);
        save(name);
    }
    return 0;
}
