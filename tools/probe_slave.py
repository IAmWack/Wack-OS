#!/usr/bin/env python3
"""
Быстрая проверка живого slave по USB: шлём несколько кадров протокола
и печатаем, что он ответил. Нужна, чтобы отделить "плата молчит" от
"симулятор неправильно говорит" — до того, как лезть в UI.

    python3 tools/probe_slave.py [/dev/ttyUSB0]
    python3 tools/probe_slave.py /dev/ttyUSB0 --listen   # слушать EVT с энкодера
"""
import sys
import time

import serial


def csum(core: str) -> str:
    x = 0
    for b in core.encode():
        x ^= b
    return "%02X" % x


def frame(cmd: str, seq: int, payload: str = "-", addr: int = 1) -> bytes:
    core = "REQ %02d %s %04X %s" % (addr, cmd, seq, payload)
    return (core + " " + csum(core) + "\n").encode()


def listen(ser):
    """Слушать асинхронные кадры EVT — проверка энкодера без интерфейса."""
    print("слушаю EVT (крути ручку, Ctrl+C — выход)")
    try:
        while True:
            line = ser.readline()
            if line:
                print("<-", line.decode(errors="replace").rstrip())
    except KeyboardInterrupt:
        pass


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    port = args[0] if args else "/dev/ttyUSB0"
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.5
    # Не дёргаем DTR/RTS: на платах с автосбросом это перезагружает ESP.
    ser.dtr = False
    ser.rts = False
    ser.open()

    time.sleep(0.3)
    ser.reset_input_buffer()

    if "--listen" in sys.argv:
        listen(ser)
        ser.close()
        return

    seq = 1
    for cmd, payload in [("SYS_PING", "-"), ("SYS_VERSION", "-"),
                         ("SYS_CAPS", "-"), ("UI_MENU_GET", "-"),
                         ("UI_MENU_SELECT", "hello")]:
        req = frame(cmd, seq, payload)
        ser.write(req)
        print("->", req.decode().rstrip())

        deadline = time.time() + 1.0
        got = False
        while time.time() < deadline:
            line = ser.readline()
            if not line:
                continue
            print("<-", line.decode(errors="replace").rstrip())
            got = True
            break
        if not got:
            print("<- (тишина)")
        seq += 1
        time.sleep(0.1)

    ser.close()


if __name__ == "__main__":
    main()
