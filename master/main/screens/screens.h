/**
 * @file    screens.h
 * @author  IAmWack
 * @brief   Мелкие связки между экранами master/main/screens/.
 *
 * Экран модуля (screen_module.c) должен знать, какой модуль выбрали
 * в списке (screen_modules.c) — этот файл передаёт индекс между ними.
 * Всё остальное экраны узнают из app_logic.
 */
#pragma once

#include <stddef.h>

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Запомнить, какой модуль выбрали, перед переходом на
 *          screen_module.
 * @param   index  Индекс модуля в списке app_logic.
 */
void   screen_module_set(size_t index);

/**
 * @brief   Индекс модуля, выбранного для screen_module.
 * @return  Индекс, ранее установленный screen_module_set().
 */
size_t screen_module_get(void);
