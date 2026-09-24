/**
 * @file    sim_shot.c
 * @author  IAmWack
 * @brief   Точка входа инструмента снимков экрана (./wackos-shot).
 *
 * Гоняет интерфейс по сценарию (загрузка, меню, модули, настройки,
 * смена темы, обрыв провода) и сохраняет кадры в shots/ (.ppm). Удобно
 * смотреть и пересылать, как сейчас выглядит интерфейс, не запуская
 * окно (и работает по ssh).
 *
 *   make shots                       # с фальшивым slave
 *   ./wackos-shot -p /dev/ttyUSB0    # с живой платой
 */
#include <stdio.h>

#include "sim_headless.h"

#include "wackos_protocol.h"
#include "ui.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Прогнать сценарий по экранам интерфейса и сохранить снимки в shots/.
 * @param   argc  Число аргументов.
 * @param   argv  Аргументы: -p порт, -b скорость, -v verbose (см. headless_connect()).
 * @return  0 при штатном выходе, 1 при ошибке подключения.
 */
int main(int argc, char **argv)
{
    const char *port = NULL;
    const wackos_transport_t *transport = headless_connect(argc, argv, &port);
    if (!transport) {
        return 1;
    }
    wackos_proto_init(transport);
    ui_init();

    /* Экран загрузки: снимаем на середине, пока шаги идут. */
    headless_idle(12);
    headless_shoot("01-boot");

    /* Досматриваем загрузку до главного меню. */
    while (ui_current() != SCREEN_MENU) {
        headless_idle(10);
    }
    headless_shoot("02-menu");

    /* Живая навигация: меню -> модули -> модуль -> назад. */
    headless_act(WACKOS_ENC_BTN);
    headless_shoot("03-modules");
    headless_act(WACKOS_ENC_BTN);
    headless_shoot("04-module");
    headless_act(WACKOS_ENC_BTN_LONG);
    headless_act(WACKOS_ENC_BTN_LONG);

    /*
     * Дальше по экранам ходим напрямую: сценарий "покрутить энкодер
     * столько-то раз" ломается от любой правки меню, а снимки нужны
     * предсказуемые.
     */
    ui_goto(SCREEN_SLAVE);
    headless_act(WACKOS_ENC_BTN);        /* Пинг */
    headless_shoot("05-slave");
    headless_act(WACKOS_ENC_BTN_LONG);

    /* Настройки: правка значения и перебор тем. */
    ui_goto(SCREEN_SETTINGS);
    headless_act(WACKOS_ENC_BTN);        /* войти в правку яркости */
    headless_act(WACKOS_ENC_CCW);
    headless_shoot("06-settings");
    headless_act(WACKOS_ENC_BTN);
    headless_act(WACKOS_ENC_CW);
    headless_act(WACKOS_ENC_CW);
    headless_act(WACKOS_ENC_BTN);        /* правка темы */
    headless_act(WACKOS_ENC_CW);
    headless_shoot("07-theme-lavender");
    headless_act(WACKOS_ENC_CW);
    headless_shoot("08-theme-mint");
    headless_act(WACKOS_ENC_CW);         /* обратно на розовую */
    headless_act(WACKOS_ENC_BTN);
    headless_act(WACKOS_ENC_BTN_LONG);

    ui_goto(SCREEN_ABOUT);
    headless_shoot("09-about");
    headless_act(WACKOS_ENC_BTN_LONG);

    /* Обрыв провода: master уходит в тайм-аут, Астольфо грустит.
     * С живой платой этот кусок пропускаем — там провод настоящий. */
    if (port) {
        return 0;
    }
    sim_slave_set_connected(false);
    ui_goto(SCREEN_SLAVE);
    headless_act(WACKOS_ENC_BTN);        /* Пинг в пустоту */
    headless_shoot("10-offline");

    /* И как выглядит меню, когда slave отвалился. */
    headless_act(WACKOS_ENC_BTN_LONG);
    headless_shoot("11-menu-offline");

    return 0;
}
