/*
 * Реализация display.h для ноутбука: те же функции, что и в
 * master/main/display.c, только кадр уезжает не в ST7789 по SPI, а в
 * окно X11. Весь рендер до этого момента — общий код wackos_gfx.
 */
#include "display.h"
#include "sim.h"

static uint8_t s_brightness = 100;

void display_init(void)
{
    /* Окно открывается в sim_main.c до запуска логики прошивки —
     * ui_init() зовёт display_init() уже "по факту". */
}

void display_set_brightness(uint8_t percent)
{
    s_brightness = (percent > 100) ? 100 : percent;
}

void display_flush(void)
{
    /*
     * Подсветки у окна нет, поэтому яркость изображаем затемнением
     * кадра. На плате этим занимается ШИМ на PIN_LCD_BL, и кадр
     * остаётся нетронутым — разница только здесь.
     */
    gfx_dim((uint8_t)(255 * s_brightness / 100));
    sim_window_present(gfx_framebuffer());
}
