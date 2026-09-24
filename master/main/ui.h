/*
 * Слой интерфейса: экраны, навигация, анимация, общие виджеты.
 *
 * Знает про меню и рисование, но ничего не знает про UART, протокол и
 * то, что физически умеет slave. С логикой общается только через
 * app_logic.h (спросить данные, выполнить действие).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SCREEN_BOOT = 0,
    SCREEN_MENU,
    SCREEN_MODULES,
    SCREEN_MODULE,
    SCREEN_SLAVE,
    SCREEN_SETTINGS,
    SCREEN_ABOUT,
    SCREEN_COUNT,
} screen_id_t;

void ui_init(void);

/*
 * Один шаг интерфейса: кадр (анимация живёт даже без действий
 * пользователя) + разбор события энкодера, если оно было.
 */
void ui_tick(void);

/* Перейти на экран / вернуться на предыдущий (длинное нажатие). */
void ui_goto(screen_id_t id);
/* Заменить текущий экран, не наращивая стек (нужно загрузке: с неё
 * возвращаться некуда). */
void ui_replace(screen_id_t id);
void ui_back(void);
screen_id_t ui_current(void);

/* Короткое сообщение пользователю (реплика в облачке у Астольфо). */
void ui_toast(const char *text);
const char *ui_toast_text(void);

/* Состояние связи со slave — индикатор в шапке. */
void ui_set_link_online(bool online);
bool ui_link_online(void);

/*
 * Пока master ждёт ответа slave, запрос блокирующий. Чтобы "думаю" было
 * видно, ui_set_busy(true) сразу рисует кадр.
 */
void ui_set_busy(bool busy);
