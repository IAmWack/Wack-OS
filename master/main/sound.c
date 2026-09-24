#include "sound.h"
#include "board_pins.h"

#include "esp_log.h"

static const char *TAG = "sound";

#if PIN_BUZZER >= 0
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUZZER_TIMER   LEDC_TIMER_1
#define BUZZER_CHANNEL LEDC_CHANNEL_1

/* Ноты каждого события: частота в Гц и длительность в мс. */
typedef struct { uint16_t hz; uint16_t ms; } note_t;

static const note_t SND_TICK[]   = { { 1800, 12 }, { 0, 0 } };
static const note_t SND_SELECT[] = { { 1400, 30 }, { 2100, 45 }, { 0, 0 } };
static const note_t SND_BACK[]   = { { 1600, 30 }, { 1100, 40 }, { 0, 0 } };
static const note_t SND_ERROR[]  = { {  420, 90 }, {  320, 120 }, { 0, 0 } };
static const note_t SND_BOOT[]   = { { 1046, 90 }, { 1318, 90 }, { 1568, 90 },
                                     { 2093, 160 }, { 0, 0 } };
#endif

static uint8_t s_volume = 6;

void sound_init(void)
{
#if PIN_BUZZER >= 0
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = BUZZER_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BUZZER_CHANNEL,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
    };
    ledc_channel_config(&ch);
#else
    ESP_LOGI(TAG, "пищалка не подключена (PIN_BUZZER = -1) — звук выключен");
#endif
}

void sound_set_volume(uint8_t level)
{
    s_volume = (level > 10) ? 10 : level;
}

void sound_play(sound_id_t id)
{
#if PIN_BUZZER >= 0
    if (s_volume == 0) {
        return;
    }
    const note_t *seq;
    switch (id) {
        case SOUND_SELECT: seq = SND_SELECT; break;
        case SOUND_BACK:   seq = SND_BACK;   break;
        case SOUND_ERROR:  seq = SND_ERROR;  break;
        case SOUND_BOOT:   seq = SND_BOOT;   break;
        default:           seq = SND_TICK;   break;
    }
    /*
     * Пищим прямо здесь, блокирующе: звуки короткие (до ~0.4 с у boot),
     * а отдельная задача ради писка — лишняя сущность. Если начнёт
     * мешать анимации, вынести в таймер.
     */
    for (; seq->ms; seq++) {
        if (seq->hz) {
            ledc_set_freq(LEDC_LOW_SPEED_MODE, BUZZER_TIMER, seq->hz);
            /* Скважность = громкость; 50% — предел, дальше только громче не станет. */
            ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, 512 * s_volume / 10);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
        }
        vTaskDelay(pdMS_TO_TICKS(seq->ms));
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
#else
    (void)id;
    (void)s_volume;
#endif
}
