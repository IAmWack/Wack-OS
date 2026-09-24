/*
 * Внутренние связки симулятора. Наружу (в код прошивки) ничего из
 * этого не торчит: прошивка видит только display.h, encoder.h и
 * wackos_transport_t — ровно те же интерфейсы, что и на железе.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "encoder.h"
#include "wackos_transport.h"

/* ---- окно (sim_window.c) ---- */

bool sim_window_open(int scale);
void sim_window_close(void);
/* Показать кадр (172x320 RGB565) с "корпусом" вокруг него. */
void sim_window_present(const uint16_t *fb);
/* Обработать события не дольше timeout_ms. false — окно закрыли. */
bool sim_window_pump(uint32_t timeout_ms);
bool sim_window_alive(void);
/* Повернуть нарисованную ручку энкодера (+1/-1) и мигнуть кнопкой. */
void sim_window_note_turn(int dir);
void sim_window_note_press(void);
/* Сохранить текущий кадр в PNG-подобный PPM рядом с бинарником. */
void sim_window_screenshot(const uint16_t *fb);

/* ---- ввод (sim_encoder.c) ---- */

void sim_input_push(wackos_encoder_event_t ev);

/* ---- провод master <-> slave (sim_transport.c) ---- */

const wackos_transport_t *sim_transport_master(void);
const wackos_transport_t *sim_transport_slave(void);
/* Есть ли в проводе кадр, уже "долетевший" до slave. */
bool sim_wire_slave_has_frame(void);
/* Печатать ли каждый кадр протокола в терминал (ключ -v). */
extern bool g_sim_verbose;
/* Задержка "провода" в миллисекундах — переключается клавишей L. */
void sim_transport_set_latency(uint32_t ms);
uint32_t sim_transport_latency(void);

/* ---- настоящий UART к живой плате (sim_serial.c) ---- */

/* Открыть порт и отдать транспорт для протокола; NULL при ошибке. */
const wackos_transport_t *sim_serial_open(const char *port, int baud);
void sim_serial_close(void);
/* true — симулятор говорит с живой платой, а не с фальшивым slave. */
bool sim_serial_active(void);
const char *sim_serial_port(void);

/* ---- фальшивый slave (sim_slave.c) ---- */

void sim_slave_init(void);
/* Дать slave обработать всё, что прилетело по проводу. */
void sim_slave_pump(void);
void sim_slave_set_connected(bool connected);
bool sim_slave_connected(void);
/* "Перезагрузка": slave молчит полторы секунды (вызывается esp_restart). */
void sim_slave_reboot(void);
bool sim_slave_muted(void);

/* ---- время (sim_platform.c) ---- */

uint32_t sim_now_ms(void);
void     sim_sleep_ms(uint32_t ms);
