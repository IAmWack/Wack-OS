/**
 * @file    module_encoder.h
 * @author  IAmWack
 * @brief   Модуль энкодера KY-040, временно подключённого к slave.
 *
 * По архитектуре ручка принадлежит master — он рисует интерфейс и
 * слушает ввод. Но платы master пока нет, а ручка есть, поэтому она
 * временно висит на slave и её события уезжают на "master" (сейчас —
 * симулятор на ноутбуке) асинхронными кадрами EVT. Это ровно тот
 * случай, ради которого EVT и заведён в протоколе.
 *
 * Когда приедет master, модуль убирается отсюда без следов: master
 * будет читать свою ручку сам, а интерфейс не заметит разницы —
 * события те же самые (общий декодер components/wackos_encoder).
 */
#pragma once

#include <stdbool.h>

#include "wackos_protocol.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/**
 * @brief   Настроить GPIO энкодера и обработчики прерываний.
 */
void module_encoder_init(void);

/**
 * @brief   Разослать накопленные события кадрами EVT.
 *
 * Зовётся из главного цикла, чтобы отправка не лезла в середину
 * чужого кадра.
 */
void module_encoder_pump(void);

/**
 * @brief   Проверить, относится ли команда к namespace ENC_.
 * @param   cmd  Имя команды из кадра REQ.
 * @return  true, если cmd начинается с "ENC_".
 */
bool module_encoder_owns(const char *cmd);

/**
 * @brief   Обработать команду namespace ENC_ (ENC_GET, ENC_HUNT, ENC_SCAN, ENC_DIAG).
 * @param   request  Входящий запрос.
 */
void module_encoder_handle(const wackos_frame_t *request);
