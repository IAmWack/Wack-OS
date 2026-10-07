/**
 * @file    theme.c
 * @author  IAmWack
 * @brief   Палитры интерфейса (розовая, мята, гроза).
 *
 * Каждая тема несёт в себе и маскота (.mascot, см. mascot.h) — розовая
 * идёт с Астольфо, "гроза" — с Разором (под его же фирменный фиолетовый
 * электро-акцент, см. razor.c), "мята" — с Венти (под его бирюзово-
 * зелёную палитру, см. venti.c). Добавить тему = дописать строку в
 * этот массив, больше нигде ничего менять не нужно.
 */
#include "theme.h"

/* ============================================================================
 *  Static state
 * ============================================================================ */

static const wackos_theme_t THEMES[] = {
    {
        .name = "розовая",
        .mascot = THEME_MASCOT_ASTOLFO,
        .bg_top = GFX_RGB(255, 193, 224), .bg_mid = GFX_RGB(228, 198, 255),
        .bg_bot = GFX_RGB(186, 232, 255), .deco = GFX_RGB(255, 150, 200),
        .card = GFX_RGB(255, 252, 254), .card_edge = GFX_RGB(255, 255, 255),
        .text = GFX_RGB(104, 42, 86), .text_dim = GFX_RGB(163, 118, 150),
        .text_on_sel = GFX_RGB(255, 255, 255), .shadow = GFX_RGB(90, 40, 75),
        .sel_top = GFX_RGB(255, 126, 190), .sel_bot = GFX_RGB(238, 74, 152),
        .sel_glow = GFX_RGB(255, 170, 215),
        .accent = GFX_RGB(255, 92, 158), .accent_soft = GFX_RGB(255, 178, 210),
        .ok = GFX_RGB(140, 232, 200), .alert = GFX_RGB(255, 120, 120),
    },
    {
        .name = "мята",
        .mascot = THEME_MASCOT_VENTI,
        .bg_top = GFX_RGB(196, 244, 226), .bg_mid = GFX_RGB(214, 240, 255),
        .bg_bot = GFX_RGB(255, 222, 240), .deco = GFX_RGB(150, 226, 200),
        .card = GFX_RGB(252, 255, 254), .card_edge = GFX_RGB(255, 255, 255),
        .text = GFX_RGB(42, 98, 88), .text_dim = GFX_RGB(122, 166, 158),
        .text_on_sel = GFX_RGB(255, 255, 255), .shadow = GFX_RGB(40, 88, 80),
        .sel_top = GFX_RGB(120, 222, 192), .sel_bot = GFX_RGB(70, 190, 168),
        .sel_glow = GFX_RGB(170, 238, 218),
        .accent = GFX_RGB(255, 132, 184), .accent_soft = GFX_RGB(255, 190, 218),
        .ok = GFX_RGB(110, 218, 180), .alert = GFX_RGB(255, 130, 130),
    },
    {
        /*
         * "Гроза" — под Разора: та же палитра логики, что у его
         * портрета (razor.c) — холодное серебро + фиолетовая молния,
         * но фон тёмный (грозовое небо), а не пастельный, как у
         * остальных трёх тем — иначе персонаж на нём потеряется.
         */
        .name = "гроза",
        .mascot = THEME_MASCOT_RAZOR,
        .bg_top = GFX_RGB(42, 38, 64), .bg_mid = GFX_RGB(58, 48, 92),
        .bg_bot = GFX_RGB(34, 44, 72), .deco = GFX_RGB(150, 90, 230),
        .card = GFX_RGB(60, 54, 86), .card_edge = GFX_RGB(150, 90, 230),
        .text = GFX_RGB(232, 228, 244), .text_dim = GFX_RGB(170, 164, 196),
        .text_on_sel = GFX_RGB(255, 255, 255), .shadow = GFX_RGB(18, 16, 30),
        .sel_top = GFX_RGB(150, 90, 230), .sel_bot = GFX_RGB(96, 56, 168),
        .sel_glow = GFX_RGB(196, 160, 255),
        .accent = GFX_RGB(180, 130, 255), .accent_soft = GFX_RGB(120, 100, 170),
        .ok = GFX_RGB(120, 220, 200), .alert = GFX_RGB(255, 110, 130),
    },
};

#define THEME_COUNT (sizeof(THEMES) / sizeof(THEMES[0]))

static size_t s_index = 0;
const wackos_theme_t *TH = &THEMES[0];

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Количество доступных тем. */
size_t theme_count(void)
{
    return THEME_COUNT;
}

/** @brief  Индекс текущей темы. */
size_t theme_index(void)
{
    return s_index;
}

/**
 * @brief   Переключить текущую тему.
 * @param   index  Индекс темы (за пределами — берётся по модулю THEME_COUNT).
 */
void theme_set(size_t index)
{
    s_index = index % THEME_COUNT;
    TH = &THEMES[s_index];
}

/**
 * @brief   Название темы по индексу (для экрана настроек).
 * @param   index  Индекс темы (за пределами — берётся по модулю THEME_COUNT).
 * @return  Человекочитаемое имя темы.
 */
const char *theme_name(size_t index)
{
    return THEMES[index % THEME_COUNT].name;
}
