#!/usr/bin/env python3
"""
gen_clock_fonts.py - the clock card's faces (#100), converted from the foundry's
own files with lv_font_conv, the way gen_type_scale.py makes the value faces.

NINA's clock faces (ESP32-P4-NINA-Display) ship converted lv_font_*.c files of up
to ~1 MB each; those are never copied. The two faces the Classic face uses are
Google Fonts under the SIL Open Font License, downloaded from Google's own font
service and kept in assets/fonts/ with their licences:

    PlayfairDisplay-Bold.woff   Playfair Display 700 (OFL, Reserved Font Name
                                "Playfair Display") - the time
    OverpassMono-Light.woff     Overpass Mono 300 (OFL) - the day, date, year, AM/PM

Only the glyphs the card draws, at the sizes it picks between (ClockCard.cpp):

    fleet_clock_digits_<px>   '-', '0'-'9', ':'           80, 104, 132 px
    fleet_clock_label_<px>    space, '0'-'9', 'A'-'Z'      16, 22 px

Playfair Display's figures are OLD-STYLE by default (3, 4, 5, 7 and 9 drop below
the line); NINA's Classic face uses lining figures from a modified copy. Lining
figures need the font's `lnum` feature applied before converting - fontTools,
an install the owner has not agreed to - so these are the foundry's figures.

HOW TO RUN IT
    python scripts/gen_clock_fonts.py

Not wired into the build (npx fetches lv_font_conv from the network); its output
in src/UI/fonts/ is committed. An existing face is kept: delete its .c to remake it.
"""

import os
import subprocess

ROOT     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS    = os.path.join(ROOT, "assets", "fonts")
FONT_DIR = os.path.join(ROOT, "src", "UI", "fonts")

FACES = [
    # (symbol stem, font file, glyph ranges, sizes)
    ("fleet_clock_digits", "PlayfairDisplay-Bold.woff", "0x2D,0x30-0x3A", (80, 104, 132)),
    ("fleet_clock_label",  "OverpassMono-Light.woff",   "0x20,0x30-0x39,0x41-0x5A", (16, 22)),
]


def generate(stem, font, ranges, px):
    sym = "%s_%d" % (stem, px)
    out = os.path.join(FONT_DIR, sym + ".c")
    if os.path.exists(out):
        print("    have %s" % sym)
        return True
    os.makedirs(FONT_DIR, exist_ok=True)
    cmd = ["npx", "--yes", "lv_font_conv",
           "--font", os.path.join(FONTS, font),
           "-r", ranges,
           "--size", str(px),
           "--bpp", "4",
           "--format", "lvgl",
           "--no-compress",
           "--lv-include", "lvgl.h",
           "-o", out]
    print("    gen  %s ..." % sym)
    # shell=True on Windows: npx is a .cmd/.ps1 shim, not an executable.
    r = subprocess.run(cmd, cwd=ROOT, shell=(os.name == "nt"), capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        print("      FAILED: %s" % (r.stderr.strip() or r.stdout.strip())[:400])
        return False
    return True


def main():
    ok = True
    for stem, font, ranges, sizes in FACES:
        for px in sizes:
            ok = generate(stem, font, ranges, px) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
