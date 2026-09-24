/**
 * @file    sim.h
 * @author  IAmWack
 * @brief   Общий внутренний интерфейс симулятора (sim/) — объявления,
 *          которые связывают между собой sim_window.c, sim_encoder.c,
 *          sim_transport.c, sim_serial.c, sim_slave.c и sim_platform.c.
 *
 * Наружу (в код прошивки master/slave) ничего из этого не торчит:
 * прошивка видит только display.h, encoder.h и wackos_transport_t —
 * ровно те же интерфейсы, что и на железе (см. docs/ARCHITECTURE.md,
 * раздел "Проверка интерфейса без железа"). Этот файл — граница между
 * "настоящим" кодом верхних слоёв и его подменой для ноутбука.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "encoder.h"
#include "wackos_transport.h"

/* ============================================================================
 *  Public API
 * ============================================================================ */

/* ---- окно (sim_window.c) ---- */

/** @brief Открыть X11-окно симулятора. @param scale Масштаб пикселя (0 — подобрать под высоту экрана). @return false при ошибке (нет X-дисплея). */
bool sim_window_open(int scale);
/** @brief Закрыть окно и освободить ресурсы X11. */
void sim_window_close(void);
/** @brief Показать кадр (172x320 RGB565) с "корпусом" вокруг него. @param fb Кадровый буфер wackos_gfx. */
void sim_window_present(const uint16_t *fb);
/** @brief Обработать события окна не дольше timeout_ms. @param timeout_ms Верхняя граница ожидания. @return false — окно закрыли. */
bool sim_window_pump(uint32_t timeout_ms);
/** @brief Открыто ли окно (и не была ли нажата Q/закрытие). @return true, пока окно живо. */
bool sim_window_alive(void);
/** @brief Повернуть нарисованную ручку энкодера. @param dir +1/-1 — направление щелчка. */
void sim_window_note_turn(int dir);
/** @brief Мигнуть подсветкой кнопки энкодера (визуальная реакция на нажатие). */
void sim_window_note_press(void);
/** @brief Сохранить текущий кадр в PPM рядом с бинарником (клавиша S). @param fb Кадровый буфер wackos_gfx. */
void sim_window_screenshot(const uint16_t *fb);

/* ---- ввод (sim_encoder.c) ---- */

/** @brief Положить событие энкодера в очередь ввода (с клавиатуры/мыши). @param ev Событие для encoder_poll(). */
void sim_input_push(wackos_encoder_event_t ev);

/* ---- провод master <-> slave (sim_transport.c) ---- */

/** @brief Транспорт стороны master для внутрипроцессного "провода". @return Указатель на статический wackos_transport_t. */
const wackos_transport_t *sim_transport_master(void);
/** @brief Транспорт стороны slave для внутрипроцессного "провода". @return Указатель на статический wackos_transport_t. */
const wackos_transport_t *sim_transport_slave(void);
/** @brief Есть ли в проводе кадр, уже "долетевший" до slave. @return true, если sim_slave_pump() есть что разобрать. */
bool sim_wire_slave_has_frame(void);
/** @brief Печатать ли каждый кадр протокола в терминал (ключ -v). */
extern bool g_sim_verbose;
/** @brief Задержка "провода" в миллисекундах — переключается клавишей L. @param ms Новая задержка. */
void sim_transport_set_latency(uint32_t ms);
/** @brief Текущая задержка "провода" в миллисекундах. @return Значение, заданное sim_transport_set_latency(). */
uint32_t sim_transport_latency(void);

/* ---- настоящий UART к живой плате (sim_serial.c) ---- */

/** @brief Открыть порт и отдать транспорт для протокола. @param port Путь устройства (например /dev/ttyUSB0). @param baud Скорость порта. @return NULL при ошибке. */
const wackos_transport_t *sim_serial_open(const char *port, int baud);
/** @brief Закрыть открытый ранее serial-порт. */
void sim_serial_close(void);
/** @brief true — симулятор говорит с живой платой, а не с фальшивым slave. */
bool sim_serial_active(void);
/** @brief Путь открытого порта. @return Строка порта либо пустая строка, если он не открыт. */
const char *sim_serial_port(void);

/* ---- фальшивый slave (sim_slave.c) ---- */

/** @brief Поднять фальшивый slave: своя копия слоя протокола на внутрипроцессном транспорте. */
void sim_slave_init(void);
/** @brief Дать slave обработать всё, что прилетело по проводу. */
void sim_slave_pump(void);
/** @brief Имитировать физическое (от)подключение slave (клавиша O). @param connected false — кадры пропадают в проводе. */
void sim_slave_set_connected(bool connected);
/** @brief Подключён ли сейчас фальшивый slave. @return Текущее состояние, заданное sim_slave_set_connected(). */
bool sim_slave_connected(void);
/** @brief "Перезагрузка": slave молчит REBOOT_SILENCE_MS (вызывается esp_restart в sim_platform.c). */
void sim_slave_reboot(void);
/** @brief В процессе ли имитация перезагрузки прямо сейчас. @return true, пока не истекло время тишины после sim_slave_reboot(). */
bool sim_slave_muted(void);

/* ---- время (sim_platform.c) ---- */

/** @brief Монотонное время в миллисекундах (аналог esp_timer для ноутбука). @return Миллисекунды с произвольной точки отсчёта. */
uint32_t sim_now_ms(void);
/** @brief Заснуть на ms миллисекунд. @param ms Длительность паузы. */
void     sim_sleep_ms(uint32_t ms);
