#!/usr/bin/env python3
"""
Генератор растровых шрифтов Wack-OS.

Берёт TTF/OTF с машины разработчика и запекает его в C-таблицу:
по глифу на символ, 4 бита альфы на пиксель (16 градаций сглаживания).
Результат — components/wackos_gfx/wackos_font_data.c, который одинаково
собирается и под ESP32-S3, и под симулятор на ноутбуке.

Запускать вручную, только когда меняется сам шрифт/размеры:

    python3 tools/gen_font.py

Зачем 4 бита, а не 1: на 172x320 однобитный шрифт выглядит рвано, а
полноценный 8-битный вдвое жирнее по флешу. 16 градаций глазом
неотличимы от 256, но таблица влезает в ~20 КБ.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_C = os.path.join(ROOT, "components", "wackos_gfx", "wackos_font_data.c")

# Основной шрифт — URW Gothic Demi (клон Avant Garde Gothic): круглые
# геометрические формы, ровно та самая "милая" пластика.
PRIMARY = "/usr/share/fonts/opentype/urw-base35/URWGothic-Demi.otf"
# Запасной — для символов, которых нет в основном (кириллица и т.п.).
FALLBACK = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

# Наборы символов: латиница+цифры+пунктуация и кириллица (для статусов).
CHARSET = [chr(c) for c in range(0x20, 0x7F)]
CHARSET += [chr(c) for c in range(0x410, 0x450)] + ["Ё", "ё"]
# Пиктограммы, которых нет в ASCII, но которые нужны интерфейсу.
CHARSET += list("♥♡★☆✿•·→←↑↓—…«»")

FACES = [
    # (имя в C, размер в px, доп. трекинг между буквами)
    ("wackos_font_small", 11, 1),
    ("wackos_font_body", 14, 0),
    ("wackos_font_title", 22, 1),
]


def has_glyph(path, ch):
    """Есть ли у шрифта настоящий глиф для символа (а не .notdef)."""
    try:
        font = ImageFont.truetype(path, 32)
    except OSError:
        return False
    img = Image.new("L", (64, 64), 0)
    ImageDraw.Draw(img).text((8, 8), ch, 255, font=font)
    if ch == " ":
        return True
    return img.getbbox() is not None


def render_glyph(font, ch, pad=4):
    """Вернуть (w, h, left, top, advance, alpha-битмап) для символа."""
    box = (96, 96)
    img = Image.new("L", box, 0)
    draw = ImageDraw.Draw(img)
    draw.text((pad, pad), ch, 255, font=font)
    advance = int(round(draw.textlength(ch, font=font)))
    bbox = img.getbbox()
    if bbox is None:  # пробел и прочие пустые
        return 0, 0, 0, 0, advance, b""
    x0, y0, x1, y1 = bbox
    crop = img.crop(bbox)
    return x1 - x0, y1 - y0, x0 - pad, y0 - pad, advance, crop.tobytes()


def pack4(data, w, h):
    """Упаковать 8-битную альфу в 4 бита на пиксель, по строкам."""
    out = bytearray()
    for y in range(h):
        row = data[y * w:(y + 1) * w]
        for x in range(0, w, 2):
            hi = row[x] >> 4
            lo = (row[x + 1] >> 4) if x + 1 < w else 0
            out.append((hi << 4) | lo)
    return bytes(out)


def build_face(name, size, tracking):
    primary = ImageFont.truetype(PRIMARY, size)
    fallback = ImageFont.truetype(FALLBACK, size)

    glyphs = []
    blob = bytearray()
    ascent = 0
    for ch in CHARSET:
        font = primary if has_glyph(PRIMARY, ch) else fallback
        w, h, left, top, advance, bitmap = render_glyph(font, ch)
        packed = pack4(bitmap, w, h)
        glyphs.append({
            "cp": ord(ch),
            "w": w, "h": h,
            "left": left, "top": top,
            "advance": advance + tracking,
            "offset": len(blob),
        })
        blob += packed
        ascent = max(ascent, -top if top < 0 else 0, h + top if top >= 0 else h)

    asc, desc = primary.getmetrics()
    return {
        "name": name,
        "size": size,
        "line_height": asc + desc,
        "baseline": asc,
        "glyphs": glyphs,
        "blob": bytes(blob),
    }


def emit(faces, path):
    lines = []
    lines.append("/*")
    lines.append(" * СГЕНЕРИРОВАННЫЙ ФАЙЛ — не править руками.")
    lines.append(" * Источник: tools/gen_font.py (URW Gothic Demi + DejaVu Sans Bold")
    lines.append(" * для кириллицы). Перегенерировать: python3 tools/gen_font.py")
    lines.append(" */")
    lines.append('#include "wackos_font.h"')
    lines.append("")

    for face in faces:
        n = face["name"]
        lines.append("/* %s: %dpx, %d глифов, %d байт битмапов */"
                     % (n, face["size"], len(face["glyphs"]), len(face["blob"])))
        lines.append("static const uint8_t %s_bitmaps[] = {" % n)
        blob = face["blob"]
        for i in range(0, len(blob), 16):
            chunk = ",".join("0x%02X" % b for b in blob[i:i + 16])
            lines.append("    %s," % chunk)
        lines.append("};")
        lines.append("")
        lines.append("static const wackos_glyph_t %s_glyphs[] = {" % n)
        for g in face["glyphs"]:
            lines.append("    {%5d,%6d,%3d,%3d,%4d,%4d,%3d}," % (
                g["cp"], g["offset"], g["w"], g["h"],
                g["left"], g["top"], g["advance"]))
        lines.append("};")
        lines.append("")
        lines.append("const wackos_font_t %s = {" % n)
        lines.append("    .glyphs = %s_glyphs," % n)
        lines.append("    .glyph_count = sizeof(%s_glyphs) / sizeof(%s_glyphs[0])," % (n, n))
        lines.append("    .bitmaps = %s_bitmaps," % n)
        lines.append("    .line_height = %d," % face["line_height"])
        lines.append("    .baseline = %d," % face["baseline"])
        lines.append("};")
        lines.append("")

    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    print("записан %s (%d байт)" % (path, os.path.getsize(path)))


def main():
    for p in (PRIMARY, FALLBACK):
        if not os.path.exists(p):
            sys.exit("нет шрифта: %s" % p)
    CHARSET.sort(key=ord)
    faces = [build_face(*f) for f in FACES]
    os.makedirs(os.path.dirname(OUT_C), exist_ok=True)
    emit(faces, OUT_C)


if __name__ == "__main__":
    main()
