#!/usr/bin/env python3
"""
gen_type_scale.py - derive each board's type scale from its own pixel density.

WHY THIS EXISTS
---------------
tokens.md derived UI *scale* from real density and retired the 1.0/1.5 split,
but UIType stayed a fixed shortlist of Montserrat faces. So every board drew
text at the same PIXEL size, which means the denser the panel the smaller the
text physically got - montserrat_12 is 1.85 mm on CYD_S3_3248 at 165 PPI and
1.04 mm on WS_P4_5 at 294 PPI. The better panel was the harder one to read,
which is the exact inversion of what the derivation was for.

Fonts cannot be scaled at runtime: LVGL compiles fixed bitmaps. So the
derivation has to happen at BUILD time, and this is where.

HOW TO RUN IT
-------------
    python scripts/gen_type_scale.py

Deliberately NOT wired into extra_scripts. Generating fonts needs `npx` to
fetch lv_font_conv, so hooking it into every build would make an ordinary
compile depend on the network. It is a tool you run when the targets below
change; its output is committed.

WHAT IT EMITS
-------------
    include/UI/UITypeScale.h     a per-board #if ladder of four faces
    src/UI/fonts/*.c             only the faces LVGL has no built-in for

Sizes that exist as a built-in Montserrat (even, 8..48) are used directly -
no generation, no flash beyond what referencing any face costs. Only the
oversized VALUE faces are generated, and they are subset to DIGITS ONLY,
which is what makes a 68 px face affordable: ~16 glyphs against full ASCII's
95. That is the same trick cards.md section 9 relies on for the MDI subset.
"""

import os
import re
import subprocess
import sys
import math

ROOT     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BSP_DIR  = os.path.join(ROOT, "components", "Fleet_BSP", "include")
TTF      = os.path.join(ROOT, "components", "lvgl", "scripts",
                        "built_in_font", "Montserrat-Medium.ttf")
OUT_HDR  = os.path.join(ROOT, "include", "UI", "UITypeScale.h")
FONT_DIR = os.path.join(ROOT, "src", "UI", "fonts")

# ---------------------------------------------------------------------------
# The targets, in MILLIMETRES ON GLASS. These are the only numbers to tune.
#
# TAG is anchored to something measured rather than chosen: the owner pointed
# at the "AUDIO" / "DISPLAY" panel titles and said that is as small as text in
# a card should ever be. Those use UIToolkit::Font_PanelHeader, which is
# montserrat_22 on WS_P4_5 at 294 PPI - i.e. 1.90 mm. Everything else keeps the
# proportions from the design bench screenshot, where value : name : unit : tag
# measured about 3.1 : 1.4 : 1.3 : 1.
# ---------------------------------------------------------------------------
TARGETS_MM = {
    "TAG":   1.90,   # header band, status row, units of measure
    "UNIT":  2.47,   # the unit beside a value, deliberately smaller than it
    "NAME":  2.66,   # the card's name - which is the LOCATION, per cards.md
    "VALUE": 5.89,   # the number, dominant
    # The glyph in an actor card's disc. It is its own role and not just
    # "VALUE size" because the VALUE face is a DIGITS-ONLY subset - it has no
    # LV_SYMBOL range at all, so drawing an icon with it renders nothing. This
    # one therefore always resolves to a full built-in face.
    "ICON":  4.50,
}

# Glyphs each role actually draws. A face is only as expensive as its range.
RANGES = {
    # Full printable ASCII plus the degree sign, which CLAUDE.md notes is the
    # one non-ASCII character stock Montserrat covers and the fleet relies on.
    "text": "0x20-0x7E,0xB0",
    # A value label draws digits and nothing else. The unit moved to its own
    # label precisely so this subset could be this small - see MeasureCard.
    "num":  "0x20,0x2B,0x2D,0x2E,0x30-0x39,0xB0",
}

BUILTIN_MIN, BUILTIN_MAX = 8, 48


def builtin_available(px):
    return px % 2 == 0 and BUILTIN_MIN <= px <= BUILTIN_MAX


def parse_boards():
    """Pull board macro, resolution and diagonal out of each BSP header."""
    boards = []
    for fn in sorted(os.listdir(BSP_DIR)):
        if not fn.startswith("BSP_") or not fn.endswith(".h"):
            continue
        path = os.path.join(BSP_DIR, fn)
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()

        guard = "BSP_" + fn[4:-2].upper() + "_H"
        macro = None
        for m in re.finditer(r"^#define\s+([A-Z0-9_]+)\s*$", text, re.M):
            if m.group(1) != guard:
                macro = m.group(1)
                break

        def field(name):
            m = re.search(r"\.\s*" + name + r"\s*=\s*(\d+)", text)
            return int(m.group(1)) if m else None

        w, h, diag = field("WIDTH"), field("HEIGHT"), field("DIAGONAL_IN")
        if not (macro and w and h and diag):
            print("  skip %s (macro=%s w=%s h=%s diag=%s)" % (fn, macro, w, h, diag))
            continue

        # DIAGONAL_IN is in TENTHS of an inch - same unit bspUiScale() reads.
        ppi = math.hypot(w, h) / (diag / 10.0)
        boards.append({"file": fn, "macro": macro, "w": w, "h": h,
                       "diag": diag / 10.0, "ppi": int(round(ppi))})
    return boards


def px_for(ppi, mm):
    px = int(round(mm / 25.4 * ppi))
    if px % 2:
        px += 1          # even sizes only: it is what the built-ins offer
    return max(px, BUILTIN_MIN)


def generate(px, kind):
    """Run lv_font_conv for one face. Returns the C symbol name."""
    sym = "fleet_font_%s_%d" % (kind, px)
    out = os.path.join(FONT_DIR, sym + ".c")
    if os.path.exists(out):
        print("    have %s" % sym)
        return sym

    os.makedirs(FONT_DIR, exist_ok=True)
    cmd = ["npx", "--yes", "lv_font_conv",
           "--font", TTF,
           "-r", RANGES[kind],
           "--size", str(px),
           "--bpp", "4",
           "--format", "lvgl",
           "--no-compress",
           "--lv-include", "lvgl.h",
           "-o", out]
    print("    gen  %s ..." % sym)
    # shell=True on Windows: npx is a .cmd/.ps1 shim, not an executable.
    r = subprocess.run(cmd, cwd=ROOT, shell=(os.name == "nt"),
                       capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        print("      FAILED: %s" % (r.stderr.strip() or r.stdout.strip())[:400])
        return None
    return sym


def main():
    if not os.path.exists(TTF):
        print("Montserrat TTF not found at %s" % TTF)
        print("The lvgl submodule is probably not checked out.")
        return 1

    boards = parse_boards()
    if not boards:
        print("No BSP headers parsed.")
        return 1

    print("\n%-14s %5s %6s  %s" % ("BOARD", "PPI", "", "TAG / UNIT / NAME / VALUE (px)"))
    print("-" * 74)

    generated = {}
    for b in boards:
        b["px"] = {role: px_for(b["ppi"], mm) for role, mm in TARGETS_MM.items()}
        print("%-14s %5d %6s  %3d / %3d / %3d / %3d" % (
            b["macro"], b["ppi"], "%.1f\"" % b["diag"],
            b["px"]["TAG"], b["px"]["UNIT"], b["px"]["NAME"], b["px"]["VALUE"]))

    # Only what has no built-in needs generating. In practice that is the
    # VALUE face on the dense boards and nothing else.
    print("\nFaces without a built-in:")
    any_gen = False
    for b in boards:
        for role, px in b["px"].items():
            if builtin_available(px):
                continue
            any_gen = True
            if role == "ICON":
                continue   # always a built-in; see below
            kind = "num" if role == "VALUE" else "text"
            key = (px, kind)
            if key not in generated:
                generated[key] = generate(px, kind)
    if not any_gen:
        print("    (none)")

    # --- emit the header ---------------------------------------------------
    decls = sorted({s for s in generated.values() if s})
    L = []
    L.append("#pragma once")
    L.append("#ifndef UI_TYPE_SCALE_H")
    L.append("#define UI_TYPE_SCALE_H")
    L.append("")
    L.append("// GENERATED by scripts/gen_type_scale.py - do not edit by hand.")
    L.append("//")
    L.append("// Each board's four faces, sized from ITS OWN pixel density so that text")
    L.append("// is the same physical size on every panel in the fleet. Targets, in mm")
    L.append("// on glass: " + ", ".join("%s %.2f" % (k, v) for k, v in TARGETS_MM.items()) + ".")
    L.append("//")
    L.append("// Resolved with #if rather than at runtime on purpose: LVGL links every")
    L.append("// font a translation unit REFERENCES, at roughly 96 KB for a full ASCII")
    L.append("// face, so choosing at runtime would make every board carry the whole")
    L.append("// fleet's type scale to use a quarter of it.")
    L.append("")
    L.append("#include <lvgl.h>")
    L.append("#include \"bsp_loader.h\"")
    L.append("")
    for d in decls:
        L.append("LV_FONT_DECLARE(%s);" % d)
    if decls:
        L.append("")

    first = True
    for b in boards:
        L.append("%s defined(%s)" % ("#if" if first else "#elif", b["macro"]))
        first = False
        L.append("    // %s - %d PPI, %.1f\"" % (b["macro"], b["ppi"], b["diag"]))
        for role in ("VALUE", "NAME", "UNIT", "TAG", "ICON"):
            px = b["px"][role]
            if role == "ICON":
                # Always a built-in, clamped to the largest one. Icons are
                # placeholders until the MDI subset is generated (cards.md
                # section 9), and that is where icon sizing gets solved
                # properly. What matters here is only that it is a FULL face:
                # VALUE is a digits-only subset and cannot draw a symbol.
                sym = "lv_font_montserrat_%d" % min(px, BUILTIN_MAX)
            else:
                kind = "num" if role == "VALUE" else "text"
                sym = ("lv_font_montserrat_%d" % px) if builtin_available(px) \
                      else (generated.get((px, kind)) or "lv_font_montserrat_%d" % min(px, BUILTIN_MAX))
            L.append("    #define FLEET_FONT_%-6s (&%s)" % (role, sym))
    L.append("#else")
    L.append("    #error \"No type scale for this board - rerun scripts/gen_type_scale.py\"")
    L.append("#endif")
    L.append("")
    L.append("#endif // UI_TYPE_SCALE_H")

    os.makedirs(os.path.dirname(OUT_HDR), exist_ok=True)
    with open(OUT_HDR, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(L) + "\n")
    print("\nWrote %s" % os.path.relpath(OUT_HDR, ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
