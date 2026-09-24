/**
 * @file    esp_log.h
 * @author  IAmWack
 * @brief   Заглушка esp_log.h для сборки прошивочного кода на ноутбуке.
 *
 * Логи ESP_LOGx уезжают в stderr — чтобы их было видно рядом с окном
 * симулятора, но они не мешали самой картинке.
 */
#pragma once

#include <stdio.h>

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "\033[31mE (%s)\033[0m " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "\033[33mW (%s)\033[0m " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stderr, "\033[32mI (%s)\033[0m " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)
