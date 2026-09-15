#!/usr/bin/env python3
"""
gen_icon_font.py - generate this fleet's Material Design Icons subset.

WHY NOW AND NOT EARLIER
-----------------------
docs/design/cards.md section 9 set one precondition: "generate the subset only
once the card types have settled which glyphs they need, because regenerating
means regenerating every board's font blob." Milestone 2.4 settled them -
sensor, binary_sensor, switch, light, button - so this is the moment.

Until this script ran, every icon on a card was an LVGL built-in symbol standing
in for the real thing, several of them frankly wrong (a droplet for temperature).

WHAT DECIDES THE GLYPH
----------------------
cards.md section 5: "Prefer provider-supplied. We already publish an `icon` in
our own MQTT discovery payloads, so the inbound direction should be
symmetrical." Every EntityDescriptor already carries one - "mdi:thermometer",
"mdi:motion-sensor", "mdi:wifi". Those names are the primary key; device_class
is the fallback for entities that arrive without one.

So the list below is not a guess about what looks nice. It is the union of:
  - every icon named in SystemEntities.h, ExternalEntities.h, VirtualEntities.h
  - the device_class fallbacks CardIcons.cpp resolves
  - the domains in CardCatalog.h that have no entity yet but will

LICENSING - checked, and not obvious
------------------------------------
The TTF is vendored at assets/fonts/ and is **Apache 2.0** (Pictogrammers Free
License: "Fonts: Apache 2.0"). It happens to sit inside reference/espcontrol,
which is PolyForm Noncommercial - but espcontrol did not author it and its own
licence travels with it, which is why the licence file is vendored beside it.
docs/REFERENCE_PROJECTS.md's "do not copy code from espcontrol" stands and is
not in tension with this.

The name-to-codepoint map is read from esphome-modular-lvgl-buttons, which is
**MIT**, reusable with attribution. It is read at generation time only; nothing
from it ships.

HOW TO RUN IT
-------------
    python scripts/gen_icon_font.py

Same deal as gen_type_scale.py: not an extra_script, because generating needs
npx to fetch lv_font_conv and an ordinary build must not depend on the network.
Output is committed.
"""

import io
import os
import re
import subprocess
import sys
import math

ROOT     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TTF      = os.path.join(ROOT, "assets", "fonts", "materialdesignicons-webfont-7.4.47.ttf")
MAPYAML  = os.path.join(ROOT, "reference", "Examples and related projects",
                        "esphome-modular-lvgl-buttons", "common", "theme",
                        "mdi_glyph_substitutions.yaml")
BSP_DIR  = os.path.join(ROOT, "components", "Fleet_BSP", "include")
OUT_HDR  = os.path.join(ROOT, "include", "UI", "UIIcons.h")
FONT_DIR = os.path.join(ROOT, "src", "UI", "fonts")

# Two sizes, in millimetres on glass, matching the two places a card draws an
# icon. gen_type_scale.py's targets are the reference:
#   LG  the disc glyph on a state card   - paired with its 4.50 mm ICON role
#   SM  the tinted glyph on a value card - paired with its 2.66 mm NAME role
#
# cards.md section 9 says "one icon size, generous glyph count", on the estimate
# that a face costs ~96 KB. That figure was measured on a 95-glyph FULL ASCII
# face; this is a ~70-glyph subset, so the second size costs a fraction of it.
# Both are generated and the real cost is reported below - if it turns out not
# to be worth it, deleting SM is one line here and one fallback in CardIcons.
SIZES_MM = {"LG": 4.50, "SM": 2.66}

# ---------------------------------------------------------------------------
# The glyph list. Every name here is an MDI icon name as Home Assistant spells
# it, minus the "mdi:" prefix.
#
# Keep this list SORTED and grouped by what it is for. Adding one is cheap;
# what is expensive is regenerating, so add in batches rather than one at a time.
# ---------------------------------------------------------------------------
GLYPHS = [
    # --- named directly by an entity we already register -------------------
    "thermometer", "brightness-5", "motion-sensor", "battery", "wifi",
    "toggle-switch", "toggle-switch-off", "memory", "ip-network", "clock-outline",

    # --- measurements: the device_class fallbacks --------------------------
    "water-percent", "gauge", "flash", "lightning-bolt", "weather-windy",
    "molecule-co2", "air-filter", "volume-high", "speedometer",
    "thermometer-low", "thermometer-high", "sun-thermometer",

    # --- battery levels, so a card can show charge rather than a number ----
    "battery-10", "battery-30", "battery-50", "battery-70", "battery-90",
    "battery-outline", "battery-alert", "battery-charging",

    # --- binary sensors ----------------------------------------------------
    "motion-sensor-off", "account", "account-off", "door-open", "door-closed",
    "window-open", "window-closed", "lock", "lock-open-variant",
    "water-alert", "smoke-detector", "shield-check", "shield-alert",

    # --- lights and switches ----------------------------------------------
    "lightbulb", "lightbulb-outline", "lightbulb-group",
    "lightbulb-group-outline", "ceiling-light", "lamp", "led-strip-variant",
    "power-plug", "power-plug-off", "power",

    # --- buttons, scenes, actions -----------------------------------------
    "gesture-tap-button", "play-circle", "movie-open", "palette", "script-text",
    "home-automation",

    # --- climate and weather ----------------------------------------------
    "thermostat", "fan", "fan-off", "snowflake", "fire", "weather-sunny",
    "weather-night", "weather-cloudy", "weather-rainy", "weather-pouring",
    "weather-snowy", "weather-fog", "weather-partly-cloudy",

    # --- status and chrome -------------------------------------------------
    "alert", "alert-circle", "check-circle", "close-circle", "help-circle",
    "pause-circle", "wifi-off", "lan-disconnect", "sync", "sync-alert",
    "chevron-right", "dots-horizontal",
]


def load_codepoints():
    """mdi name -> integer codepoint, from the MIT esphome substitution map."""
    if not os.path.exists(MAPYAML):
        print("MDI name map not found at %s" % MAPYAML)
        return None
    table = {}
    pat = re.compile(r'^\s*mdi_([a-z0-9_]+):\s*"\\U([0-9A-Fa-f]{8})"')
    with io.open(MAPYAML, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = pat.match(line)
            if m:
                # esphome replaces dashes and slashes with underscores; HA's own
                # names use dashes, which is what an EntityDescriptor carries.
                table[m.group(1).replace("_", "-")] = int(m.group(2), 16)
    return table


def parse_boards():
    """Board macro and real PPI, the same derivation gen_type_scale.py uses."""
    boards = []
    for fn in sorted(os.listdir(BSP_DIR)):
        if not fn.startswith("BSP_") or not fn.endswith(".h"):
            continue
        with io.open(os.path.join(BSP_DIR, fn), encoding="utf-8", errors="replace") as f:
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
            continue
        ppi = math.hypot(w, h) / (diag / 10.0)
        boards.append({"macro": macro, "ppi": int(round(ppi))})
    return boards


def px_for(ppi, mm):
    px = int(round(mm / 25.4 * ppi))
    return max(px + (px % 2), 8)


def c_ident(name):
    return name.upper().replace("-", "_")


def generate(px, ranges):
    """One MDI subset face at px. Returns the C symbol, or None on failure."""
    sym = "fleet_icons_%d" % px
    out = os.path.join(FONT_DIR, sym + ".c")
    if os.path.exists(out):
        print("    have %s" % sym)
        return sym

    os.makedirs(FONT_DIR, exist_ok=True)
    cmd = ["npx", "--yes", "lv_font_conv",
           "--font", TTF,
           "-r", ranges,
           "--size", str(px),
           "--bpp", "4",
           "--format", "lvgl",
           "--no-compress",
           "--lv-include", "lvgl.h",
           "-o", out]
    print("    gen  %s ..." % sym)
    r = subprocess.run(cmd, cwd=ROOT, shell=(os.name == "nt"),
                       capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        print("      FAILED: %s" % (r.stderr.strip() or r.stdout.strip())[:400])
        return None
    return sym


def main():
    if not os.path.exists(TTF):
        print("MDI font not found at %s" % TTF)
        return 1

    table = load_codepoints()
    if not table:
        return 1

    missing = [g for g in GLYPHS if g not in table]
    if missing:
        print("Not in the MDI name map (typo, or renamed upstream):")
        for g in missing:
            print("    %s" % g)
        return 1

    wanted = sorted({table[g] for g in GLYPHS})
    ranges = ",".join("0x%X" % cp for cp in wanted)
    print("%d glyphs requested, %d distinct codepoints\n" % (len(GLYPHS), len(wanted)))

    boards = parse_boards()
    sizes = {}
    print("%-14s %5s  %s" % ("BOARD", "PPI", "ICON px  LG / SM"))
    print("-" * 46)
    for b in boards:
        b["px"] = {k: px_for(b["ppi"], mm) for k, mm in SIZES_MM.items()}
        print("%-14s %5d  %11d / %d" % (b["macro"], b["ppi"], b["px"]["LG"], b["px"]["SM"]))
        for px in b["px"].values():
            sizes[px] = None

    print("\nFaces:")
    for px in sorted(sizes):
        sizes[px] = generate(px, ranges)
    if any(v is None for v in sizes.values()):
        print("\nAt least one face failed; header not written.")
        return 1

    # --- emit -------------------------------------------------------------
    L = []
    L.append("#pragma once")
    L.append("#ifndef UI_ICONS_H")
    L.append("#define UI_ICONS_H")
    L.append("")
    L.append("// GENERATED by scripts/gen_icon_font.py - do not edit by hand.")
    L.append("//")
    L.append("// The fleet's Material Design Icons subset: %d glyphs, at two sizes" % len(GLYPHS))
    L.append("// derived per board from real pixel density. MDI is Apache 2.0; see")
    L.append("// assets/fonts/materialdesignicons-LICENSE.txt, vendored beside the font.")
    L.append("//")
    L.append("// Glyphs are UTF-8 string literals so they can be handed straight to")
    L.append("// lv_label_set_text(). The name of each constant is the Home Assistant")
    L.append("// icon name, which is what an EntityDescriptor already carries.")
    L.append("")
    L.append("#include <lvgl.h>")
    L.append("#include \"bsp_loader.h\"")
    L.append("")
    for px in sorted(sizes):
        L.append("LV_FONT_DECLARE(%s);" % sizes[px])
    L.append("")

    first = True
    for b in boards:
        L.append("%s defined(%s)" % ("#if" if first else "#elif", b["macro"]))
        first = False
        L.append("    #define FLEET_ICONS_LG (&%s)" % sizes[b["px"]["LG"]])
        L.append("    #define FLEET_ICONS_SM (&%s)" % sizes[b["px"]["SM"]])
    L.append("#else")
    L.append("    #error \"No icon font for this board - rerun scripts/gen_icon_font.py\"")
    L.append("#endif")
    L.append("")
    L.append("// --- the glyphs -------------------------------------------------------")
    L.append("// Sorted by name. MDI codepoints live in the private use area, so these")
    L.append("// are meaningless to any other font - which is why cardIconFor() must")
    L.append("// always resolve through this table rather than passing a name through.")
    L.append("")
    for g in sorted(GLYPHS):
        cp = table[g]
        ch = chr(cp).encode("utf-8")
        esc = "".join("\\x%02X" % b for b in ch)
        L.append("#define MDI_%-26s \"%s\"   // U+%04X" % (c_ident(g), esc, cp))
    L.append("")
    L.append("// --- name lookup ------------------------------------------------------")
    L.append("//")
    L.append("// cards.md section 5: \"Prefer provider-supplied. We already publish an")
    L.append("// `icon` in our own MQTT discovery payloads, so the inbound direction")
    L.append("// should be symmetrical.\" Every EntityDescriptor already carries one, so")
    L.append("// this is what turns that string into something drawable.")
    L.append("//")
    L.append("// A linear scan over %d entries, run once per card per repaint. Sorting" % len(GLYPHS))
    L.append("// and bisecting would be faster and is not worth the generated code - a")
    L.append("// repaint already costs orders of magnitude more than this.")
    L.append("struct MdiEntry { const char *name; const char *glyph; };")
    L.append("")
    L.append("inline const MdiEntry MDI_TABLE[] = {")
    for g in sorted(GLYPHS):
        L.append("    { \"%s\", MDI_%s }," % (g, c_ident(g)))
    L.append("};")
    L.append("inline constexpr unsigned MDI_TABLE_N = sizeof(MDI_TABLE) / sizeof(MDI_TABLE[0]);")
    L.append("")
    L.append("// Accepts either \"mdi:thermometer\" or plain \"thermometer\". Returns")
    L.append("// nullptr when the name is unknown, so a caller can fall through to its")
    L.append("// device_class mapping rather than drawing a wrong glyph confidently.")
    L.append("inline const char *mdiGlyph(const char *name) {")
    L.append("    if (!name || !name[0]) return nullptr;")
    L.append("    if (name[0] == 'm' && name[1] == 'd' && name[2] == 'i' && name[3] == ':') name += 4;")
    L.append("    for (unsigned i = 0; i < MDI_TABLE_N; i++) {")
    L.append("        const char *a = MDI_TABLE[i].name, *b = name;")
    L.append("        while (*a && *a == *b) { a++; b++; }")
    L.append("        if (!*a && !*b) return MDI_TABLE[i].glyph;")
    L.append("    }")
    L.append("    return nullptr;")
    L.append("}")
    L.append("")
    L.append("#endif // UI_ICONS_H")

    os.makedirs(os.path.dirname(OUT_HDR), exist_ok=True)
    with io.open(OUT_HDR, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(L) + "\n")

    print("\nWrote %s" % os.path.relpath(OUT_HDR, ROOT))
    for px in sorted(sizes):
        path = os.path.join(FONT_DIR, sizes[px] + ".c")
        print("   %-22s %7.1f KB of C source" % (sizes[px], os.path.getsize(path) / 1024.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
