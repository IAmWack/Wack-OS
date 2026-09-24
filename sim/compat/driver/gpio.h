/*
 * Заглушка driver/gpio.h. Светодиоды slave в симуляторе просто
 * печатаются в лог — увидеть их всё равно негде.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

#define GPIO_MODE_OUTPUT 2
#define GPIO_MODE_INPUT  1

#define GPIO_PULLUP_ENABLE   1
#define GPIO_PULLUP_DISABLE  0
#define GPIO_INTR_ANYEDGE    3
#define GPIO_INTR_NEGEDGE    2

typedef struct {
    uint64_t pin_bit_mask;
    int      mode;
    int      pull_up_en;
    int      pull_down_en;
    int      intr_type;
} gpio_config_t;

typedef void (*gpio_isr_t)(void *arg);

esp_err_t gpio_config(const gpio_config_t *cfg);
esp_err_t gpio_install_isr_service(int flags);
esp_err_t gpio_isr_handler_add(int pin, gpio_isr_t handler, void *arg);
esp_err_t gpio_set_level(int pin, uint32_t level);
int       gpio_get_level(int pin);
