#!/usr/bin/env python3
"""Icona di Old Machine Rebirth, stile MBStudio: telaio nero e griglia di pad.

Si rigenera con:  python3 Assets/brand/make_icon.py
"""

import os
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = 1024
S = 4                      # si disegna in grande e si rimpicciolisce
SIDE = OUT * S

CHASSIS_TOP = (21, 22, 23)
CHASSIS_BOTTOM = (10, 10, 11)
RECESS = (5, 5, 6)
RECESS_EDGE = (38, 40, 43)
PAD = (18, 19, 21)
PAD_EDGE = (52, 54, 58)
LED_BLUE = (61, 139, 255)
LED_AMBER = (255, 176, 32)
LED_GREEN = (62, 224, 92)

# quali pad sono accesi, riga 0 in alto
LIT = {(0, 1): LED_BLUE, (1, 3): LED_AMBER, (2, 0): LED_GREEN, (3, 2): LED_BLUE}


def rounded(size, radius, colour):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(img).rounded_rectangle([0, 0, size[0] - 1, size[1] - 1], radius, fill=colour)
    return img


def vertical_gradient(size, top, bottom):
    img = Image.new("RGBA", size)
    draw = ImageDraw.Draw(img)
    for y in range(size[1]):
        t = y / max(1, size[1] - 1)
        draw.line([(0, y), (size[0], y)],
                  fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    return img


def main():
    img = Image.new("RGBA", (SIDE, SIDE), (0, 0, 0, 0))

    # telaio
    radius = round(SIDE * 0.22)
    chassis = vertical_gradient((SIDE, SIDE), CHASSIS_TOP, CHASSIS_BOTTOM)
    mask = rounded((SIDE, SIDE), radius, (255, 255, 255, 255))
    img.paste(chassis, (0, 0), mask)

    # incasso della griglia
    margin = round(SIDE * 0.17)
    well = [margin, margin, SIDE - margin, SIDE - margin]
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle(well, round(SIDE * 0.05), fill=RECESS + (255,),
                           outline=RECESS_EDGE + (255,), width=round(SIDE * 0.004))

    # 4x4 di pad, come la macchina vera
    inner = round(SIDE * 0.035)
    x0, y0 = margin + inner, margin + inner
    span = SIDE - 2 * (margin + inner)
    gap = round(span * 0.045)
    cell = (span - 3 * gap) // 4
    pad_radius = round(cell * 0.16)

    glow = Image.new("RGBA", (SIDE, SIDE), (0, 0, 0, 0))

    for row in range(4):
        for col in range(4):
            x = x0 + col * (cell + gap)
            y = y0 + row * (cell + gap)
            colour = LIT.get((row, col))
            img.alpha_composite(rounded((cell, cell), pad_radius, PAD + (255,)), (x, y))
            ImageDraw.Draw(img).rounded_rectangle([x, y, x + cell, y + cell], pad_radius,
                                                  outline=PAD_EDGE + (255,), width=round(SIDE * 0.0028))
            if colour:
                glow.alpha_composite(rounded((cell, cell), pad_radius, colour + (170,)), (x, y))

    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(round(SIDE * 0.012))))

    for (row, col), colour in LIT.items():
        x = x0 + col * (cell + gap)
        y = y0 + row * (cell + gap)
        bright = tuple(min(255, round(v * 1.15)) for v in colour)
        img.alpha_composite(vertical_gradient((cell, cell), bright, colour).convert("RGBA").resize((cell, cell)),
                            (x, y))
        ImageDraw.Draw(img).rounded_rectangle([x, y, x + cell, y + cell], pad_radius,
                                              outline=(243, 243, 241, 60), width=round(SIDE * 0.002))

    # il marchio impresso in basso
    mark_path = os.path.join(HERE, "mbstudio91_wordmark.png")
    if os.path.exists(mark_path):
        mark = Image.open(mark_path).convert("RGBA")
        w = round(SIDE * 0.34)
        h = max(1, round(w * mark.height / mark.width))
        mark = mark.resize((w, h), Image.LANCZOS)
        white = Image.new("RGBA", mark.size, (255, 255, 255, 36))
        white.putalpha(mark.getchannel("A").point(lambda a: round(a * 0.14)))
        pos = ((SIDE - w) // 2, SIDE - margin + round(SIDE * 0.03))
        if pos[1] + h < SIDE:
            img.alpha_composite(white, pos)

    out = os.path.join(HERE, "icon.png")
    img.resize((OUT, OUT), Image.LANCZOS).save(out)
    print("scritto", out)


if __name__ == "__main__":
    main()
