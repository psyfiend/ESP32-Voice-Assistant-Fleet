"""THE COMPILED CARDS, RETIRED INTO A SETTINGS FILE (2.13 round 5, DECISIONS K171).

The firmware from round 5 no longer builds the House page's compiled cards
(Outside, Kitchen, Office, Living's Overhead, the thermostat, Eric, the garage's
temperature) or the Fleet page's (everything but the Panel group). This makes
them cards made on the board, in a saved settings file, WITH THE IDS THEY HAD -
so what the owner saved on them (taps, spans, slots, hidden) stays theirs.

    python scripts/release/retire_compiled.py <saved.json> <out.json> [--mqtt] [--tests]

  --mqtt   the dev boards' MQTT group on the Fleet page: the deck's temperature,
           light level and occupancy (they were compiled Fleet cards)
  --tests  the test cards too (scripts/release/test_cards.json): Avail, Reading,
           Refuse, Battery (two), Lamps 1-3, Switch, Stuck - for a test that needs them

Put the result on a board with restore_settings.ps1 BEFORE it boots round 5's
firmware (-NoReboot, then flash), and compare with setdiff.py. A card entry the
file already has keeps every key it had; this only adds what a made card needs.
Specs below are Dashboard_HA.h / Dashboard_Fleet.h as they were at 5c03eaa.
"""
import json
import os
import sys

HOUSE = "house"
FLEET = "home"
DOORS = "garage_doors_261019_1050"   # stays compiled until 2.14
PANEL = "panel_261105_0500"          # the Fleet page's built-in group

PRI_DOORS, PRI_OUTDOOR, PRI_PRESENCE, PRI_LIGHT, PRI_INDOOR, PRI_EXTRA = 230, 200, 170, 140, 100, 60

# (id, entity, name, area, priority) - a group's members.
OUTSIDE = [
    ("outside_porch_261008_0310", "outside_front_light", "Porch", "Outside", PRI_LIGHT),
    ("outside_front_261008_0310", "outside_front_temp", "Front", "Outside", PRI_OUTDOOR),
    ("outside_deck_261008_0310", "outside_deck_temp", "Deck", "Outside", PRI_OUTDOOR),
    ("outside_deck_261008_0310b", "outside_deck_lux", "Deck", "Outside", PRI_EXTRA),
]
KITCHEN = [
    ("kitchen_sink_261008_0310", "kitchen_sink", "Sink", "Kitchen", PRI_LIGHT),
    ("kitchen_table_261008_0310", "kitchen_table", "Table", "Kitchen", PRI_LIGHT),
    ("kitchen_261008_0310", "kitchen_occupancy", "Kitchen", "Kitchen", PRI_PRESENCE),
    ("kitchen_261008_0310b", "kitchen_temp", "Kitchen", "Kitchen", PRI_INDOOR),
]
OFFICE = [
    ("office_desk_261008_0310", "office_desk", "Desk", "Office", PRI_LIGHT),
    ("office_overhead_261008_0310", "office_overhead", "Overhead", "Office", PRI_LIGHT),
    ("office_261008_0310", "office_occupancy", "Office", "Office", PRI_PRESENCE),
    ("office_261008_0310b", "office_temp", "Office", "Office", PRI_INDOOR),
]

# The House page in its compiled order (after Garage - Doors): a group is
# (id, name, area, priority, members, slots); a card is (id, entity, name, area, priority).
HOUSE_PAGE = [
    ("group", "outside_261019_0500", "Outside", "Outside", PRI_OUTDOOR, OUTSIDE,
     ["outside_deck_261008_0310", "outside_deck_261008_0310b"]),
    ("group", "kitchen_261019_0500", "Kitchen", "Kitchen", PRI_PRESENCE, KITCHEN,
     ["kitchen_261008_0310b", "kitchen_261008_0310"]),
    ("group", "office_261019_0500", "Office", "Office", PRI_PRESENCE, OFFICE,
     ["office_261008_0310b", "office_261008_0310"]),
    ("card", "living_overhead_261008_0310", "living_overhead", "Overhead", "Living", PRI_LIGHT),
    ("card", "front_thermo_261008_0310", "front_thermo", "Thermo", "Front", PRI_INDOOR),
    ("card", "bedroom_eric_261008_0310", "bedroom_temp", "Eric", "Bedroom", PRI_INDOOR),
    ("card", "garage_261008_0310", "garage_temp", "Garage", "Garage", PRI_EXTRA),
]

# The dev boards' MQTT group (owner, 2026-11-05): the deck sensor, over the broker.
MQTT_ID = "mqtt_261105_0500"
MQTT = [
    ("outdoor_deck_261008_0310", "deck_temp", "Deck", "Outdoor", 220),
    ("outdoor_deck_261008_0310c", "deck_lux", "Deck", "Outdoor", 100),
    ("outdoor_deck_261008_0310b", "deck_motion", "Deck", "Outdoor", 160),
]


def put(cards, cid, fields):
    """The made card's keys under what the entry already holds (its own win)."""
    have = cards.get(cid, {})
    merged = dict(fields)
    merged.update(have)
    cards[cid] = merged


def group(cards, page, after, gid, name, area, pri, members, slots):
    put(cards, gid, {"page": page, "after": after, "kind": "cluster", "name": name, "area": area,
                     "pri": str(pri)})
    if slots and "slots" not in cards[gid]:
        cards[gid]["slots"] = ",".join(slots)
    # A compiled card's words were Custom (K49); a made card inherits HA's name (K65)
    # unless its entry says Custom - so each says so, as its compiled self did.
    for mid, ent, mname, marea, _ in members:
        put(cards, mid, {"in": gid, "entity": ent, "area": marea, "name": mname, "label": "custom"})


def card(cards, page, after, cid, ent, name, area, pri):
    put(cards, cid, {"page": page, "after": after, "entity": ent, "area": area, "pri": str(pri), "name": name,
                     "label": "custom"})


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    if len(args) != 2:
        print(__doc__)
        sys.exit(2)
    doc = json.load(open(args[0], encoding="utf-8"))
    cards = doc.setdefault("cards", {})
    after = DOORS
    for row in HOUSE_PAGE:
        if row[0] == "group":
            _, gid, name, area, pri, members, slots = row
            group(cards, HOUSE, after, gid, name, area, pri, members, slots)
            after = gid
        else:
            _, cid, ent, name, area, pri = row
            card(cards, HOUSE, after, cid, ent, name, area, pri)
            after = cid
    if "--mqtt" in flags:
        group(cards, FLEET, PANEL, MQTT_ID, "MQTT", "MQTT", 160, MQTT, [])
    if "--tests" in flags:
        here = os.path.dirname(os.path.abspath(__file__))
        tests = json.load(open(os.path.join(here, "test_cards.json"), encoding="utf-8"))
        for cid, fields in tests["cards"].items():
            put(cards, cid, fields)
    with open(args[1], "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
    print("written", args[1], len(cards), "card entries")


if __name__ == "__main__":
    main()
