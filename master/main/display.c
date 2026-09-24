/**
 * @file    display.c
 * @author  IAmWack
 * @brief   Слой дисплея master: инициализация ST7789 (SPI) и вывод кадра.
 *
 * Знает про ST7789/SPI — и больше ни про что. Рисование сюда не входит:
 * весь рендер идёт в кадровый буфер wackos_gfx (components/wackos_gfx),
 * а display отвечает только за то, чтобы готовый кадр оказался на
 * матрице. По этой же границе проходит подмена в симуляторе:
 * sim/sim_display.c реализует те же функции, выводя кадр в окно.
 */
#include "display.h"
#include "board_pins.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "display";

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define LCD_SPI_HOST        SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)

/*
 * Матрица 172x320 обычно "сидит" в середине памяти контроллера ST7789
 * (он рассчитан на 240x320), поэтому по X нужен сдвиг: (240-172)/2 = 34.
 * Если на вашей плате картинка съехала вбок — правится здесь.
 */
#define LCD_X_OFFSET 34
#define LCD_Y_OFFSET 0

/* Подсветка — на ШИМ, чтобы ползунок яркости в настройках что-то менял. */
#define BL_TIMER    LEDC_TIMER_0
#define BL_CHANNEL  LEDC_CHANNEL_0

/* ============================================================================
 *  Static state
 * ============================================================================ */

static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Инициализировать SPI-шину, панель ST7789 и подсветку (ШИМ).
 */
void display_init(void)
{
    /* Подсветка через ШИМ (LEDC), а не просто "включить". */
    ledc_timer_config_t bl_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = BL_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&bl_timer));

    ledc_channel_config_t bl_ch = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = 1023,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&bl_ch));

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        /* Кадр гоним построчными блоками, буфер под 40 строк с запасом. */
        .max_transfer_sz = LCD_WIDTH * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &s_io));

    /*
     * ВНИМАНИЕ: поля esp_lcd_panel_dev_config_t менялись между версиями
     * ESP-IDF (color_space / rgb_endian / rgb_ele_order). Приведено под
     * актуальный на сентябрь 2026 API — если Espressif-IDE ругается на
     * это поле, смотрите esp_lcd_panel_vendor.h своей версии IDF и
     * поправьте имя поля здесь же.
     */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_endian = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel));

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true); /* типично для матриц ST7789 */
    esp_lcd_panel_swap_xy(s_panel, false);
    esp_lcd_panel_mirror(s_panel, false, false);
    esp_lcd_panel_set_gap(s_panel, LCD_X_OFFSET, LCD_Y_OFFSET);
    esp_lcd_panel_disp_on_off(s_panel, true);

    ESP_LOGI(TAG, "ST7789 инициализирован: %dx%d", LCD_WIDTH, LCD_HEIGHT);
}

/**
 * @brief   Установить яркость подсветки.
 * @param   percent  Яркость 0..100 % (значения выше 100 обрезаются).
 */
void display_set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL, 1023 * percent / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL);
}

/**
 * @brief   Отправить текущий кадровый буфер gfx на экран целиком.
 */
void display_flush(void)
{
    if (!s_panel) {
        return;
    }

    /*
     * Гоним кадр полосами по 40 строк: одна транзакция на весь кадр не
     * влезет в max_transfer_sz, а построчно — слишком много накладных
     * расходов на каждую передачу.
     */
    const int band = 40;
    const uint16_t *fb = gfx_framebuffer();
    for (int y = 0; y < LCD_HEIGHT; y += band) {
        int h = (y + band > LCD_HEIGHT) ? (LCD_HEIGHT - y) : band;
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_WIDTH, y + h, fb + (size_t)y * LCD_WIDTH);
    }
}
