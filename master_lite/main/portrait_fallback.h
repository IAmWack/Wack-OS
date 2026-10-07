/**
 * @file    portrait_fallback.h
 * @author  IAmWack
 * @brief   Запасные растровые портреты во флеше (см. portrait_fallback_data.c).
 *
 * Данные — сгенерированы, не редактировать руками (см. докстринг .c).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char *character;
    const char *mood;
    const uint16_t *pixels; /* PORTRAIT_SD_W * PORTRAIT_SD_H, RGB565 (см. portrait_sd.h) */
} portrait_fallback_t;

extern const portrait_fallback_t PORTRAIT_FALLBACKS[];
extern const size_t PORTRAIT_FALLBACK_COUNT;
