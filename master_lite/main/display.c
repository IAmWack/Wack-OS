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
/* 12 МГц — как в офиц. демо Waveshare для этой платы; выше поднимем
 * отдельно, когда базовая картинка подтверждена на реальном железе. */
#define LCD_PIXEL_CLOCK_HZ  (12 * 1000 * 1000)

/*
 * Матрица 172x320 "сидит" внутри GRAM контроллера ST7789 (240x320) со
 * сдвигом (240-172)/2=34 по X. Сверено по офиц. демо-прошивке Waveshare
 * для именно этой платы (ESP32-S3-LCD-1.47-Demo, LCD_Driver/Vernon_ST7789T)
 * — тот же офсет, что был изначально; более ранняя правка на (0,34) по
 * стороннему источнику была ошибочной, откачена.
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
    /*
     * Подсветка через ШИМ (LEDC). Явный gpio_config() на пине ДО настройки
     * LEDC — как в офиц. демо Waveshare для этой платы; без него после
     * наших более ранних правок (см. git-историю этого файла) подсветка
     * не загоралась вовсе, хотя LEDC формально конфигурировался без ошибок.
     */
    gpio_config_t bl_gpio_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_gpio_cfg));

    ledc_timer_config_t bl_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = BL_TIMER,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&bl_timer));

    ledc_channel_config_t bl_ch = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = (1 << 13) - 1,
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
        /* BGR, не RGB — сверено по офиц. демо Waveshare для этой платы. */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel));

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);

    /*
     * Generic-инициализация esp_lcd_new_panel_st7789 не знает фирменных
     * регистров питания/гаммы этой конкретной матрицы (VCOM, gate control,
     * gamma curve и т.п.) — без них панель остаётся небиасированной и
     * визуально пустой при ЛЮБОМ содержимом кадра, что мы и наблюдали.
     * Блок ниже — дословно из офиц. демо-прошивки Waveshare для этой платы
     * (ESP32-S3-LCD-1.47-Demo, LCD_Driver/Vernon_ST7789T/Vernon_ST7789T.c,
     * функция panel_st7789t_init) — значения не трогать, это заводская
     * калибровка конкретной матрицы, не подбираются на глаз.
     */
    esp_lcd_panel_io_tx_param(s_io, 0xB0, (uint8_t[]){0x00, 0xE8}, 2);
    esp_lcd_panel_io_tx_param(s_io, 0xB2, (uint8_t[]){0x0c, 0x0c, 0x00, 0x33, 0x33}, 5);
    esp_lcd_panel_io_tx_param(s_io, 0xB7, (uint8_t[]){0x75}, 1);
    esp_lcd_panel_io_tx_param(s_io, 0xBB, (uint8_t[]){0x1A}, 1);
    esp_lcd_panel_io_tx_param(s_io, 0xC0, (uint8_t[]){0x80}, 1);
    esp_lcd_panel_io_tx_param(s_io, 0xC2, (uint8_t[]){0x01, 0xff}, 2);
    esp_lcd_panel_io_tx_param(s_io, 0xC3, (uint8_t[]){0x13}, 1);
    esp_lcd_panel_io_tx_param(s_io, 0xC4, (uint8_t[]){0x20}, 1);
    esp_lcd_panel_io_tx_param(s_io, 0xC6, (uint8_t[]){0x0F}, 1);
    esp_lcd_panel_io_tx_param(s_io, 0xD0, (uint8_t[]){0xA4, 0xA1}, 2);
    esp_lcd_panel_io_tx_param(s_io, 0xE0,
        (uint8_t[]){0xD0, 0x0D, 0x14, 0x0D, 0x0D, 0x09, 0x38, 0x44, 0x4E, 0x3A, 0x17, 0x18, 0x2F, 0x30}, 14);
    esp_lcd_panel_io_tx_param(s_io, 0xE1,
        (uint8_t[]){0xD0, 0x09, 0x0F, 0x08, 0x07, 0x14, 0x37, 0x44, 0x4D, 0x38, 0x15, 0x16, 0x2C, 0x2E}, 14);

    /* Полярность/зеркало/офсет — тоже сверены по той же офиц. демо. */
    esp_lcd_panel_invert_color(s_panel, true);
    esp_lcd_panel_swap_xy(s_panel, false);
    esp_lcd_panel_mirror(s_panel, true, false);
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
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL, ((1 << 13) - 1) * percent / 100);
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
