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
    # The number on a CRAMPED card. #62, milestone 2.7: one type scale per board
    # was right for the board and wrong for the card - on a dense page the
    # value overflowed cells it would have fitted a size smaller. A card now
    # picks VALUE or VALUE_SM by what its own cell can seat (Card::
    # resolveVariant). About three quarters of VALUE; a generated digits-only
    # face costs 6-10 KB, measured, so this is cheap on every board.
    "VALUE_SM": 4.40,
    # The glyph in an actor card's disc. It is its own role and not just
    # "VALUE size" because the VALUE face is a DIGITS-ONLY subset - it has no
    # LV_SYMBOL range at all, so drawing an icon with it renders nothing. This
    # one therefore always resolves to a full built-in face.
    "ICON":  4.50,
}

# A PER-BOARD, OPTIONALLY PER-ROLE SCALE ON TOP OF THOSE TARGETS.
#
# The mm targets keep text the same PHYSICAL size fleet-wide, which is the
# right default and was worth building. It is not an absolute rule, and the
# owner said so after living with it (2026-09-17): "These devices are primarily
# desktop dashboards not wall mounted... I'm not overly concerned about using
# an appropriately smaller size on smaller screens."
#
# The reason it bites on WS_P4_5 is arithmetic, not taste. It is the fleet's
# densest panel at 294 PPI, so 5.89 mm is a 68 px face - and its CARDS are not
# proportionally bigger, because the board is only 5 inches. A 68 px line in a
# 182 px cell is 41% of the card before anything else is drawn, which is what
# forced every card on it to the compact variant.
#
# So: a multiplier, per board, default 1.0. This is a preference and belongs
# here rather than in Fleet_BSP, which holds hardware facts.
# A value may be a plain float (every role) or a dict keyed by role, where "*"
# is the fallback for roles it does not name. Per-role exists because a single
# multiplier provably cannot express what these boards need - see the note on
# TAG below.
SCALE = {
    # TAG IS NOT SCALED ON ANY BOARD, and the reason generalises.
    #
    # What makes a card cramped is the VERTICAL STACK - hero, name, status -
    # and VALUE dominates it. TAG is a small label in the area header; shrinking
    # it reclaims almost no height and costs legibility immediately. The owner,
    # on both 4B boards at 0.85: "the header text is simply too small on both of
    # the boards, it is not viable as it stands right now."
    #
    # A global multiplier could not have fixed it, which is worth recording
    # because it is not obvious. Sizes quantise to even pixels, and on
    # WS_S3_4B TAG is ~12.7 px at 1.0 - so 0.85, 0.88, 0.90 and 0.92 ALL land
    # on 12. The first scale that returns TAG to 14 is 1.00, which also drags
    # VALUE from 34 back to 40 and undoes the entire change. Measured, not
    # reasoned:
    #
    #   WS_S3_4B   x0.85  12/14/16/34     WS_P4_4B  x0.85  16/22/24/50
    #              x0.92  12/16/16/36               x0.92  18/24/26/54
    #              x1.00  14/18/18/40               x1.00  20/26/28/60
    #
    # Hence per-role. "*" shrinks what actually costs height; TAG keeps the mm
    # target, which was right about it all along.
    "WS_P4_5": {"*": 0.88, "TAG": 1.00},

    # The 4B pair, 2026-09-19. Same value for both, and that is the point.
    #
    # HARDWARE_STATUS.md: "WS_P4_4B and WS_S3_4B are the same layout problem in
    # different pixels - 720x720 at 1.5x is the same effective UI space as
    # 480x480 at 1.0x. If the scaling approach is right they should be visually
    # indistinguishable apart from sharpness." They are now flashed as a pair
    # and running the same 3x4 grid, so anything that cramps one cramps the
    # other, and a different multiplier on each would break that property for
    # no reason.
    #
    # Why they need one at all: both are FOUR INCH panels being asked for 12
    # cells. That is the densest cards-per-inch in the fleet - the 7B gets 18
    # cells across seven inches - so the mm-based targets, which are right
    # about physical size, produce type that is correct and still too big for
    # the box it has to sit in. Observed by the owner on both boards: the icon
    # disc is barely larger than its glyph on WS_P4_4B and not visible at all
    # on WS_S3_4B, because Card::midHeight() clamps the disc to whatever band
    # is left once the faces have taken theirs.
    #
    # 0.85 is a STARTING POINT, not a measured optimum - the same status 0.88
    # had on WS_P4_5 before it was looked at. The glass decides.
    "WS_P4_4B": {"*": 0.85, "TAG": 1.00},
    "WS_S3_4B": {"*": 0.85, "TAG": 1.00},
}


def scale_for(macro, role):
    """Per-board, per-role multiplier. Default 1.0 - i.e. trust the mm target."""
    s = SCALE.get(macro, 1.0)
    if isinstance(s, dict):
        return s.get(role, s.get("*", 1.0))
    return s

# Glyphs each role actually draws. A face is only as expensive as its range.
RANGES = {
    # Full printable ASCII plus the degree sign, which CLAUDE.md notes is the
    # one non-ASCII character stock Montserrat covers and the fleet relies on.
    "text": "0x20-0x7E,0xB0",
    # A value label draws digits and nearly nothing else. The unit moved to its
    # own label precisely so this subset could be this small - see MeasureCard.
    #
    # COLON (0x3A) and 'd' (0x64) are here for #51's durations, and their
    # absence was a real bug rather than a theoretical one. A VALUE face is
    # either a built-in Montserrat (full ASCII) or one of these generated
    # subsets, so "01:25" rendered correctly on CYD_S3_3248, WS_S3_4B,
    # WS_P4_7B, CYD_P4_1060 and CYD_S3_8048 - and as tofu boxes on exactly the
    # three boards using a generated face: WS_P4_4B (50), WS_P4_5 (60) and
    # WS_S3_5B (56). The owner saw it as "boxes between the units" and
    # reasonably suspected the connectivity fault; it was a missing glyph.
    #
    # Two extra glyphs is a rounding error against a ~96 KB face, and the
    # alternative - keeping the subset pure and never printing a duration in
    # the VALUE role - gives up the feature to protect the budget.
    "num":  "0x20,0x2B,0x2D,0x2E,0x30-0x39,0x3A,0x64,0xB0",
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

    print("\n%-14s %5s %6s  %s" % ("BOARD", "PPI", "", "TAG / UNIT / NAME / VALUE / VALUE_SM (px)"))
    print("-" * 80)

    generated = {}
    for b in boards:
        b["px"] = {role: px_for(b["ppi"], mm * scale_for(b["macro"], role))
                   for role, mm in TARGETS_MM.items()}
        print("%-14s %5d %6s  %3d / %3d / %3d / %3d / %3d" % (
            b["macro"], b["ppi"], "%.1f\"" % b["diag"],
            b["px"]["TAG"], b["px"]["UNIT"], b["px"]["NAME"], b["px"]["VALUE"],
            b["px"]["VALUE_SM"]))

    # Only what has no built-in needs generating. In practice that is the
    # VALUE face on the dense boards and nothing else.
    print("\nFaces without a built-in:")
    any_gen = False
    for b in boards:
        for role, px in b["px"].items():
            # VALUE_SM IS ALWAYS GENERATED, even where a built-in exists. A
            # built-in is full ASCII - 30-60 KB at these sizes - while the
            # digits-only subset is 6-10 KB, and VALUE_SM draws nothing else.
            # Preferring the built-in would have made #62's second face cost
            # several times what cards.md section 13 says it costs.
            if builtin_available(px) and role != "VALUE_SM":
                continue
            any_gen = True
            if role == "ICON":
                continue   # always a built-in; see below
            kind = "num" if role in ("VALUE", "VALUE_SM") else "text"
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
        for role in ("VALUE", "VALUE_SM", "NAME", "UNIT", "TAG", "ICON"):
            px = b["px"][role]
            if role == "ICON":
                # Always a built-in, clamped to the largest one. Icons are
                # placeholders until the MDI subset is generated (cards.md
                # section 9), and that is where icon sizing gets solved
                # properly. What matters here is only that it is a FULL face:
                # VALUE is a digits-only subset and cannot draw a symbol.
                sym = "lv_font_montserrat_%d" % min(px, BUILTIN_MAX)
            elif role == "VALUE_SM" and generated.get((px, "num")):
                sym = generated[(px, "num")]    # always the subset; see above
            else:
                kind = "num" if role in ("VALUE", "VALUE_SM") else "text"
                sym = ("lv_font_montserrat_%d" % px) if builtin_available(px) \
                      else (generated.get((px, kind)) or "lv_font_montserrat_%d" % min(px, BUILTIN_MAX))
            L.append("    #define FLEET_FONT_%-8s (&%s)" % (role, sym))
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
