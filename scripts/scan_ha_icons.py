#!/usr/bin/env python3
"""
scan_ha_icons.py - ask Home Assistant which icons it is actually serving.

WHY THIS EXISTS
---------------
`gen_icon_font.py`'s GLYPHS list used to be curated by hand from what the card
types looked like they would need. That was right for 2.4, when no real entity
existed. It is the wrong method now: Home Assistant ships the icon for every
entity, including the state-dependent variant, so the correct list is not a
design decision at all - it is a measurement.

Getting it wrong is silent. A glyph the font does not carry renders as tofu,
exactly like the ':' and 'd' that were missing from the numeric subset in #51,
and nothing in the build says a word about it.

RUN IT BEFORE REGENERATING THE FONT
-----------------------------------
    python scripts/scan_ha_icons.py

Read-only - it issues get_states and config/entity_registry/list and nothing
else. Needs `websockets` (the PlatformIO venv has it):

    ~/.platformio/penv/Scripts/python.exe scripts/scan_ha_icons.py

WHAT IT REPORTS
---------------
  1. our registered entities, their current icon, and whether we ship it
  2. what is missing, and whether the glyph exists in the vendored TTF at all
  3. the whole instance, so the cost of future-proofing is a number

A NOTE ON STATE VARIANTS
------------------------
`attributes.icon` is what HA would draw RIGHT NOW. A binary sensor that is
currently `off` reports the off glyph, and the on glyph never appears in a
single snapshot. Where a pair is known - motion-sensor/-off, garage/garage-open
- ship BOTH; PAIRS below records the ones that matter, because discovering the
other half at 2am when a door opens is not a good way to find out.
"""

import asyncio
import collections
import json
import os
import pathlib
import re
import sys

try:
    import websockets
except ImportError:
    sys.exit("needs `websockets` - run with ~/.platformio/penv/Scripts/python.exe")

ROOT = pathlib.Path(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

HA_HOST = os.environ.get("HA_HOST", "192.168.0.70")
HA_PORT = int(os.environ.get("HA_PORT", "8123"))

SECRETS = ROOT / "components/Fleet_Connectivity/ConnectivityLocalSecrets.h"
ENTITIES = ROOT / "components/Fleet_Providers/ExternalEntities_HA.h"
GENERATOR = ROOT / "scripts/gen_icon_font.py"
GLYPHMAP = (ROOT / "reference/Examples and related projects"
            / "esphome-modular-lvgl-buttons/common/theme"
            / "mdi_glyph_substitutions.yaml")

# Glyphs that only ever appear one at a time. Ship both halves.
PAIRS = {
    "motion-sensor": "motion-sensor-off",
    "garage": "garage-open",
    "door-closed": "door-open",
    "window-closed": "window-open",
    "lock": "lock-open-variant",
    "toggle-switch": "toggle-switch-off",
    "power-plug": "power-plug-off",
    "fan": "fan-off",
}


def token():
    m = re.search(r'define\s+LOCAL_HA_ACCESS_TOKEN\s+"([^"]+)"',
                  SECRETS.read_text())
    if not m:
        sys.exit(f"no LOCAL_HA_ACCESS_TOKEN in {SECRETS}")
    return m.group(1)


def shipped():
    """The GLYPHS list the generator will actually build."""
    src = GENERATOR.read_text()
    start = src.index("GLYPHS = [")
    return set(re.findall(r'"([a-z0-9\-]+)"', src[start:src.index("]", start)]))


def in_ttf():
    """Names present in the vendored substitution map.

    The map spells them `mdi_ceiling_light_outline:` - prefixed and
    snake_cased. Matching MDI's own hyphenated spelling against that raw text
    finds nothing, which once produced a confident and entirely wrong report
    that four ordinary icons were absent from the font.
    """
    if not GLYPHMAP.exists():
        return None
    raw = GLYPHMAP.read_text(errors="ignore")
    return {n.replace("_", "-")
            for n in re.findall(r'^\s*mdi_([a-z0-9_]+):', raw, re.M)}


def our_refs():
    return set(re.findall(r'\.externalRef\s*=\s*"([^"]+)"', ENTITIES.read_text()))


_n = [0]


async def req(ws, payload):
    _n[0] += 1
    rid = _n[0]
    await ws.send(json.dumps(dict(payload, id=rid)))
    while True:
        m = json.loads(await ws.recv())
        if m.get("id") == rid and m.get("type") == "result":
            if not m.get("success", True):
                sys.exit(f"HA refused {payload.get('type')}: {m.get('error')}")
            return m["result"]


def strip(icon):
    return icon.split(":", 1)[1] if icon and ":" in icon else icon


async def main():
    have, ttf, ours = shipped(), in_ttf(), our_refs()
    uri = f"ws://{HA_HOST}:{HA_PORT}/api/websocket"

    async with websockets.connect(uri, max_size=16 * 1024 * 1024) as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": token()}))
        hello = json.loads(await ws.recv())
        if hello.get("type") != "auth_ok":
            sys.exit(f"auth failed: {hello}")

        states = await req(ws, {"type": "get_states"})
        reg = await req(ws, {"type": "config/entity_registry/list"})

    reg_icon = {e["entity_id"]: (e.get("icon") or e.get("original_icon"))
                for e in reg}

    needed, all_icons, prefixes = set(), collections.Counter(), collections.Counter()
    rows = []

    for s in states:
        eid = s["entity_id"]
        attrs = s.get("attributes", {})
        for ic in (attrs.get("icon"), reg_icon.get(eid)):
            if ic:
                prefixes[ic.split(":", 1)[0] if ":" in ic else "(none)"] += 1
                all_icons[ic] += 1
        if eid in ours:
            chosen = attrs.get("icon") or reg_icon.get(eid)
            name = strip(chosen)
            if name:
                needed.add(name)
            rows.append((eid, chosen, attrs.get("device_class") or "-"))

    # Both halves of any pair we touch.
    for a, b in PAIRS.items():
        if a in needed:
            needed.add(b)
        if b in needed:
            needed.add(a)

    print("=" * 78)
    print(f"1. OUR {len(rows)} REGISTERED ENTITIES")
    print("=" * 78)
    for eid, icon, dc in sorted(rows):
        name = strip(icon)
        if not name:
            note = f"(falls back to device_class: {dc})"
        else:
            note = "ok" if name in have else "** MISSING FROM THE FONT **"
        print(f"  {eid:<52} {str(icon or '-'):<28} {note}")

    missing = sorted(n for n in needed if n not in have)
    print("\n" + "=" * 78)
    print("2. TO ADD TO gen_icon_font.py")
    print("=" * 78)
    if not missing:
        print("  nothing - the font already covers every entity we register.")
    for n in missing:
        if ttf is None:
            where = "(glyph map not found; cannot verify)"
        elif n in ttf:
            where = "exists in the TTF"
        else:
            where = "!! NOT IN THE TTF - check the name !!"
        print(f'  "{n}",{"":<{max(0, 26 - len(n))}}{where}')

    mdi_all = {strip(i) for i in all_icons if i.startswith("mdi:")}
    non_mdi = sorted(i for i in all_icons if not i.startswith("mdi:"))

    print("\n" + "=" * 78)
    print("3. THE WHOLE INSTANCE, if the dashboard ever grows")
    print("=" * 78)
    print(f"  {len(all_icons)} distinct icon strings over {len(states)} entities")
    print(f"  prefixes: {dict(prefixes)}")
    print(f"  distinct MDI names : {len(mdi_all)}")
    print(f"  already shipped    : {len(mdi_all & have)}")
    print(f"  not shipped        : {len(mdi_all - have)}")
    if non_mdi:
        print(f"\n  NOT MDI ({len(non_mdi)}) - a custom icon pack. These cannot render")
        print("  from the MDI font at all, whatever we generate:")
        for i in non_mdi:
            print(f"    {i:<36} x{all_icons[i]}")
    print("\n  full not-shipped list:")
    for n in sorted(mdi_all - have):
        print(f"    {n}")


asyncio.run(main())
