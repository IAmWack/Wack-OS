/**
 * @file    esp_err.h
 * @author  IAmWack
 * @brief   Заглушка esp_err.h для сборки прошивочного кода в sim/ обычным gcc.
 *
 * Минимальный подменыш типа ошибок ESP-IDF (components/sim не тянет
 * реальный ESP-IDF) — см. docs/ARCHITECTURE.md, раздел "Проверка
 * интерфейса без железа".
 */
#pragma once

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERROR_CHECK(x) ((void)(x))
