#include "settings.h"
#include "theme.h"
#include "display.h"
#include "sound.h"

static uint8_t s_brightness = 80;
static uint8_t s_volume = 6;
static bool s_astolfo = true;

void settings_init(void)
{
    display_set_brightness(s_brightness);
    sound_set_volume(s_volume);
    theme_set(0);
}

uint8_t settings_brightness(void)
{
    return s_brightness;
}

void settings_set_brightness(uint8_t percent)
{
    if (percent < 10) percent = 10;
    if (percent > 100) percent = 100;
    s_brightness = percent;
    display_set_brightness(s_brightness);
}

uint8_t settings_volume(void)
{
    return s_volume;
}

void settings_set_volume(uint8_t level)
{
    if (level > 10) level = 10;
    s_volume = level;
    sound_set_volume(s_volume);
}

size_t settings_theme(void)
{
    return theme_index();
}

void settings_set_theme(size_t index)
{
    theme_set(index);
}

bool settings_astolfo(void)
{
    return s_astolfo;
}

void settings_set_astolfo(bool on)
{
    s_astolfo = on;
}
