/**
 * @file    clock.c
 * @author  IAmWack
 * @brief   Программные "часы" поверх системного времени ESP-IDF (см. clock.h).
 */
#include "clock.h"

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/** @brief  Индекс месяца (0..11) по трёхбуквенному имени из __DATE__. */
static int month_index(const char *name)
{
    static const char *const MONTHS[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    for (int i = 0; i < 12; i++) {
        if (strncmp(name, MONTHS[i], 3) == 0) {
            return i;
        }
    }
    return 0;
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Выставить системное время = время сборки прошивки. */
void clock_init(void)
{
    char mon[4] = "";
    int day = 1, year = 1970, hh = 0, mm = 0, ss = 0;

    /* __DATE__: "Mon DD YYYY", __TIME__: "HH:MM:SS" — формат компилятора,
     * не зависит от локали. */
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);

    struct tm tm = {
        .tm_year = year - 1900,
        .tm_mon  = month_index(mon),
        .tm_mday = day,
        .tm_hour = hh,
        .tm_min  = mm,
        .tm_sec  = ss,
    };

    struct timeval tv = { .tv_sec = mktime(&tm), .tv_usec = 0 };
    settimeofday(&tv, NULL);

    /*
     * Разовая ручная поправка: часы сборочной машины были в другом
     * поясе, чем реальное время пользователя (см. git-историю этого
     * файла/переписку) — выставлено по факту на момент добавления этой
     * строки. Подправить/убрать через "Настройки -> Время", когда
     * разъедется снова — оно разъедется, RTC тут нет.
     */
    clock_adjust_hours(21 - tm.tm_hour);
    clock_adjust_minutes(22 - tm.tm_min);
}

/** @brief  Сдвинуть текущее время на delta часов. */
void clock_adjust_hours(int delta)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tv.tv_sec += (time_t)delta * 3600;
    settimeofday(&tv, NULL);
}

/** @brief  Сдвинуть текущее время на delta минут. */
void clock_adjust_minutes(int delta)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tv.tv_sec += (time_t)delta * 60;
    settimeofday(&tv, NULL);
}

/*
 * День/месяц/год — через struct tm + mktime(), а не прибавлением секунд:
 * у месяцев/годов переменная длина (mktime сам нормализует "32 января"
 * в "1 февраля" и т.п.), в отличие от часов/минут, где секунды работают
 * верно напрямую.
 */

/** @brief  Сдвинуть текущую дату на delta дней. */
void clock_adjust_days(int delta)
{
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_mday += delta;
    struct timeval tv = { .tv_sec = mktime(&lt), .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

/** @brief  Сдвинуть текущий месяц на delta (год переносится сам через mktime). */
void clock_adjust_months(int delta)
{
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_mon += delta;
    struct timeval tv = { .tv_sec = mktime(&lt), .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

/** @brief  Сдвинуть текущий год на delta. */
void clock_adjust_years(int delta)
{
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_year += delta;
    struct timeval tv = { .tv_sec = mktime(&lt), .tv_usec = 0 };
    settimeofday(&tv, NULL);
}
