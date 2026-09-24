/*
 * Слой логики master. Единственное место, которое знает имена команд
 * протокола (SYS_PING, UI_MENU_GET и т.д.) и что с ними делать.
 * Экраны про протокол не знают — они дёргают функции отсюда и
 * показывают то, что вернулось.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SLAVE_ITEM_MAX   8
#define SLAVE_TITLE_LEN  28

typedef struct {
    char id[16];                 /* id для UI_MENU_SELECT */
    char title[SLAVE_TITLE_LEN]; /* что показать пользователю */
} slave_item_t;

void app_init(void);

/*
 * Один шаг загрузки (пинг, версия, список модулей...). Возвращает
 * false, когда загрузка закончена. Разбито по шагам, чтобы экран
 * загрузки показывал реальный прогресс, а не бутафорский прогресс-бар.
 */
bool app_boot_step(const char **out_label, int *out_percent);

/* ---- Что мы знаем о slave ---- */

bool        app_online(void);
const char *app_slave_fw(void);     /* версия прошивки slave */
const char *app_slave_proto(void);  /* версия протокола, о которой он сказал */
const char *app_slave_caps(void);   /* список команд через ';' */
uint32_t    app_rtt_ms(void);       /* время последнего ответа */

size_t              app_item_count(void);
const slave_item_t *app_item(size_t index);

/* ---- Действия (блокирующие, с тайм-аутом и ретраями протокола) ---- */

bool app_ping(void);
bool app_read_version(void);
bool app_read_caps(void);
bool app_reset_slave(void);
bool app_refresh_items(void);
bool app_run_item(size_t index);

/* Результат последнего действия — то, что показывать на экране. */
const char *app_last_result(void);
