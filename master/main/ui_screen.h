/*
 * Внутренний интерфейс экрана. Каждый экран — это четыре функции и
 * заголовок; роутер в ui.c больше про экран ничего не знает, поэтому
 * добавить новый экран = положить файл в screens/ и вписать его в
 * таблицу.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "theme.h"
#include "wackos_gfx.h"

typedef enum {
    UI_IN_NEXT = 0,  /* энкодер по часовой */
    UI_IN_PREV,      /* против часовой */
    UI_IN_PRESS,     /* короткое нажатие */
    UI_IN_BACK,      /* длинное нажатие */
} ui_input_t;

typedef struct {
    const char *title;              /* что написать в шапке */
    void (*enter)(void);            /* вызывается при переходе на экран */
    /*
     * ТОЛЬКО рисование: никаких запросов к slave и смены экрана.
     * draw() вызывается из отрисовки кадра, а запрос к slave сам
     * перерисовывает экран (показать "думаю") — вызов логики отсюда
     * уходит в бесконечную рекурсию.
     */
    void (*draw)(uint32_t t);
    /* Логика экрана "сама по себе": шаги загрузки, опрос по таймеру и
     * т.п. Вызывается раз в кадр, но вне отрисовки. */
    void (*tick)(uint32_t t);
    void (*input)(ui_input_t in);   /* реакция на энкодер */
    const char *hint;               /* строка подсказок внизу */
} ui_screen_t;

extern const ui_screen_t screen_boot;
extern const ui_screen_t screen_menu;
extern const ui_screen_t screen_modules;
extern const ui_screen_t screen_module;
extern const ui_screen_t screen_slave;
extern const ui_screen_t screen_settings;
extern const ui_screen_t screen_about;

/* ---- Общие виджеты (ui_widgets.c) ---- */

typedef enum {
    UI_ICON_NONE = 0,
    UI_ICON_HEART,
    UI_ICON_BOW,
    UI_ICON_SPARK,
    UI_ICON_GEAR,
    UI_ICON_PLUG,
} ui_icon_t;

typedef struct {
    char      title[32];
    char      value[24];  /* правая колонка: значение настройки, статус */
    ui_icon_t icon;
} ui_row_t;

typedef void (*ui_row_fn)(size_t index, ui_row_t *out);

/*
 * Список с выделением, плавной прокруткой и полоской справа.
 * Состояние живёт в экране, рисование — общее: так все списки в
 * системе ведут себя одинаково, а экран занят только содержимым.
 */
typedef struct {
    size_t   count;
    size_t   selected;
    int      visible_rows;
    int      row_h;
    bool     editing;      /* true — крутилка меняет значение, а не выбор */
    float    sel_anim;
    float    scroll_anim;
    uint32_t last_ms;
} ui_list_t;

void ui_list_init(ui_list_t *l, size_t count, int visible_rows, int row_h);
void ui_list_move(ui_list_t *l, int delta);
void ui_list_draw(ui_list_t *l, int y0, ui_row_fn row, uint32_t t);

/* Карточка со скруглениями и тенью — основа всех экранов. */
void ui_card(int x, int y, int w, int h, uint8_t alpha);
/* Иконка размером s с центром в (cx, cy). */
void ui_icon(ui_icon_t icon, int cx, int cy, int s, uint16_t color, uint8_t alpha);
/* Текст с переносом по словам; возвращает число нарисованных строк. */
int  ui_text_block(const wackos_font_t *f, int x, int y, int w, int max_lines,
                   const char *text, uint16_t color);
/*
 * Строка "поле ....... значение" внутри карточки. Значение прижато
 * вправо, а если не влезает — обрезается многоточием: версия прошивки
 * slave может быть любой длины и не должна наезжать на подпись.
 */
void ui_info_line(int x, int y, int w, const char *label, const char *value,
                  uint16_t color);
/* Облачко с репликой (хвостик вниз-влево, к Астольфо). */
void ui_bubble(int x, int y, int w, const char *text);
