#!/usr/bin/env python3
"""
Поиск ноги энкодера.

Опрашивает у slave все свободные GPIO и печатает те, которые кто-то
притягивает. Нужен, когда сигнал не доходит и непонятно, куда воткнут
провод: вместо гадания по шелкографии сразу видно номер GPIO.

    python3 tools/enc_hunt.py /dev/ttyUSB1

Держи кнопку нажатой или крути ручку — в строке появятся номера ног.
Пусто в обеих полярностях значит, что замыкание до платы не доходит
вообще: чаще всего не соединён GND либо провод не сидит в гнезде.
"""
import sys
import time

import serial


def csum(core: str) -> str:
    x = 0
    for b in core.encode():
        x ^= b
    return "%02X" % x


def frame(cmd: str, seq: int, payload: str = "-") -> bytes:
    core = "REQ 01 %s %04X %s" % (cmd, seq, payload)
    return (core + " " + csum(core) + "\n").encode()


def ask(ser, cmd, seq, payload="-"):
    ser.write(frame(cmd, seq, payload))
    for _ in range(8):
        line = ser.readline().decode(errors="replace").rstrip()
        if " OK " in line:
            return line.split(" OK ", 1)[1].rsplit(" ", 1)[0].split(" ", 1)[1]
    return "(нет ответа)"


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 1.0
    ser.dtr = False   # не дёргаем автосброс платы
    ser.rts = False
    ser.open()
    time.sleep(0.6)
    ser.reset_input_buffer()

    print("крути ручку / жми кнопку, Ctrl+C — выход\n")
    seq = 1
    try:
        while True:
            down = ask(ser, "ENC_HUNT", seq); seq += 1
            up = ask(ser, "ENC_HUNT", seq, "DOWN"); seq += 1
            print("  тянут вниз: %-18s тянут вверх: %s" % (down, up))
            time.sleep(0.3)
    except KeyboardInterrupt:
        pass
    ser.close()


if __name__ == "__main__":
    main()
