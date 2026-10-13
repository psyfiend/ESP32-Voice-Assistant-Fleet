#!/usr/bin/env python3
"""The UI refactor's safety net: golden trees and screenshots (milestone 2.12, P0).

docs/design/ui-foundation.md is the brief. A board built with -D DEBUG_POPUP serves GET /popup,
which opens, drives and closes card windows from a PC; ?tree=1 (the open window) and ?tree=page
(the whole screen) print LVGL's object tree, one line per object - class, place, size, flags,
events, text and style (CardPopup.cpp, "GET /popup?tree=").

    capture   walk a fixed list of states on one board and save, for each, the tree and a
              /screenshot:   test/golden/<board>/<label>/<state>.txt and .png, plus run.json
              (every request, every reply, LVGL's pool and the build times)
    compare   two captures, state by state: every object whose class, place, size, flags,
              events, text or style changed. Values and times that change on their own (a
              temperature, "3m ago", the clock) are told apart from what code changed: a text
              change is "volatile" when it differs only in digits or time words, and a place or
              size change is excused only where such a text changed size, inside the nearest
              container that kept its own size. --pixels also compares the screenshots and
              lists the regions no volatile object explains.
    measure   open a few windows N times each: the build time (median) and what each costs
              in LVGL's pool, and whether closing gives it all back
    bootlog   reset a board through its serial port and print its [Heap] lines

    python scripts/ui_golden.py capture fleet-ws-p4-5 --board WS_P4_5 --label baseline
    python scripts/ui_golden.py compare WS_P4_5 baseline p1 --pixels
    python scripts/ui_golden.py measure fleet-ws-p4-5 --board WS_P4_5 --label p1 --n 7
    python scripts/ui_golden.py bootlog COM15

NOTHING HERE COMMANDS A LIGHT OR WRITES THE SETTINGS FILE. Windows are opened, their views and
panels switched, and closed again - no power, set, scene, pause, look, show or addcard. The
Remove dialog is shown with removecard=3 (any card's window, nothing removed). The sparks are
turned off for the run (they are random) and back on after; the page shown is put back.

Standard library only; bootlog needs pyserial, which PlatformIO's own Python has
(~/.platformio/penv/Scripts/python.exe). test/golden/ is gitignored.
"""

import argparse
import datetime
import difflib
import json
import pathlib
import re
import statistics
import struct
import sys
import time
import urllib.error
import urllib.request
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "test" / "golden"

HIST = "hist"   # a step's wait: until History's answer has arrived and been drawn

# ---------------------------------------------------------------------------------------------
# THE STATES. A group opens one card's window (card = (label, area, n): the n-th card with that
# label and area on the page) or none (the dashboard), then walks its steps: (state, queries,
# wait). A state of None acts without capturing. tree "page" captures the whole screen.
# Cards a board does not show (its grid drops them) are recorded as absent.
# ---------------------------------------------------------------------------------------------
PLAN = [
    {"page": 0, "card": None, "tree": "page", "steps": [("house_page", [], 0.5)]},
    {"page": 0, "card": ("Desk", "Office", 0), "steps": [
        ("desk_main", [], 0.6),
        ("desk_settings", ["deck=1"], 0.9),
        (None, ["deck=0"], 0.6),
        ("desk_temp", ["ctl=1"], 0.6),
        ("desk_color", ["ctl=2"], 0.6),
        (None, ["ctl=0"], 0.5),
        ("desk_scenes", ["view=4"], 0.9),
        ("desk_scenes_panel", ["deck=2"], 0.9),
        (None, ["deck=0"], 0.6),
        ("desk_history", ["view=1"], HIST),
        ("desk_history_panel", ["deck=2"], 0.9),
        (None, ["deck=0"], 0.6),
        ("desk_props", ["view=5"], 0.6),
        ("desk_members", ["view=2"], 0.6),
        ("desk_member", ["member=0"], 0.6),
        ("desk_member_settings", ["deck=1"], 0.9),
        (None, ["deck=0"], 0.6),
        ("desk_member_history", ["view=1"], HIST),
    ]},
    {"page": 0, "card": ("Desk", "Office", 0), "steps": [("desk_dialog", ["removecard=3"], 0.6)]},
    {"page": 0, "card": ("Desk", "Office", 0), "tree": "page", "steps": [("desk_open_page", [], 0.6)]},
    {"page": 0, "card": ("Deck", "Outside", 0), "steps": [
        ("deck_main", [], 0.6),
        ("deck_settings", ["deck=1"], 0.9),
        (None, ["deck=0"], 0.6),
        ("deck_history", ["view=1"], HIST),
        ("deck_chart_panel", ["deck=2"], 0.9),
    ]},
    {"page": 0, "card": ("North", "Garage", 0), "steps": [
        ("north_main", [], 0.6),
        ("north_history", ["view=1"], HIST),
        ("north_activity_panel", ["deck=2"], 0.9),
    ]},
    {"page": 0, "card": ("Overhead", "Living", 0), "steps": [
        ("living_overhead_main", [], 0.6),
        ("living_overhead_settings", ["deck=1"], 0.9),
    ]},
    {"page": 0, "card": ("Overhead", "Office", 0), "steps": [("office_overhead_main", [], 0.6)]},
    {"page": 0, "card": ("Table", "Kitchen", 0), "steps": [("table_main", [], 0.6)]},
    {"page": 0, "card": ("Porch", "Outside", 0), "steps": [("porch_main", [], 0.6)]},
    {"page": 0, "card": ("Thermo", "Front", 0), "steps": [("thermo_main", [], 0.6)]},
    {"page": 0, "card": ("Kitchen", "Kitchen", 0), "steps": [("kitchen_occ_main", [], 0.6)]},
    {"page": 1, "card": None, "tree": "page", "steps": [("fleet_page", [], 0.5)]},
    {"page": 1, "card": ("All Lamps", "Kitchen", 0), "steps": [
        ("lamps_main", [], 0.6),
        ("lamps_settings", ["deck=1"], 0.9),
        (None, ["deck=0"], 0.6),
        ("lamps_history", ["view=1"], HIST),
        ("lamps_props", ["view=5"], 0.6),
        ("lamps_members", ["view=2"], 0.6),
        ("lamps_member", ["member=0"], 0.6),
        ("lamps_member_settings", ["deck=1"], 0.9),
    ]},
    {"page": 1, "card": ("Lamp 1", "Kitchen", 0), "steps": [
        ("lamp1_main", [], 0.6),
        ("lamp1_temp", ["ctl=1"], 0.6),
        ("lamp1_color", ["ctl=2"], 0.6),
    ]},
    {"page": 1, "card": ("Lamp 2", "Kitchen", 0), "steps": [("lamp2_main", [], 0.6)]},
    {"page": 1, "card": ("Lamp 3", "Lounge", 0), "steps": [("lamp3_main", [], 0.6)]},
    {"page": 1, "card": ("Switch", "Office", 0), "steps": [("switch_main", [], 0.6)]},
    {"page": 1, "card": ("Uptime", "Panel", 0), "steps": [
        ("uptime_main", [], 0.6),
        ("uptime_history", ["view=1"], HIST),
    ]},
]

# measure: the windows opened again and again (page, card).
MEASURE = [
    (0, ("Desk", "Office", 0)),
    (0, ("Deck", "Outside", 0)),
    (0, ("North", "Garage", 0)),
    (0, ("Overhead", "Living", 0)),
    (1, ("All Lamps", "Kitchen", 0)),
    (1, ("Lamp 1", "Kitchen", 0)),
]

# Queries that would command something or write the settings file. Never sent.
FORBIDDEN = ("power", "set", "scene", "pause", "look", "show", "addcard", "scheme", "tap", "clabel")


# ---------------------------------------------------------------------------------------------
# Talking to a board
# ---------------------------------------------------------------------------------------------
class Board:
    def __init__(self, host, verbose=False):
        self.host = host
        self.verbose = verbose
        self.log = []   # every request and reply, for run.json

    def get(self, path, timeout=30.0, binary=False, record=True):
        url = f"http://{self.host}{path}"
        last = None
        for attempt in range(6):
            try:
                with urllib.request.urlopen(url, timeout=timeout) as r:
                    data = r.read()
                break
            except urllib.error.HTTPError as e:
                last = e
                if e.code == 503:   # the UI thread is busy with another request: try again
                    time.sleep(0.5 + attempt * 0.5)
                    continue
                raise
            except OSError as e:  # timeouts, refused connections: the board may be busy
                last = e
                time.sleep(1.0 + attempt)
        else:
            raise RuntimeError(f"{url}: {last}")
        if binary:
            return data
        text = data.decode("utf-8", errors="replace")
        if record:
            self.log.append({"t": round(time.time(), 2), "get": path, "reply": text if len(text) < 4000 else
                             text[:4000] + "..."})
        if self.verbose:
            print(f"    {path} -> {text.strip()[:160]}")
        return text

    def popup(self, query=""):
        key = query.split("=")[0]
        if key in FORBIDDEN:
            raise ValueError(f"/popup?{query} could command something or write settings; not sent")
        return self.get("/popup" + (f"?{query}" if query else ""))

    def tree(self, page=False):
        return self.get("/popup?tree=" + ("page" if page else "1"), record=False)

    def screenshot(self):
        return self.get("/screenshot", timeout=60.0, binary=True)

    def page(self, index):
        """Go to page `index` and stay there (a one-frame /bench run with keep=1). Returns the JSON."""
        text = self.get(f"/bench?n=1&page={index}&keep=1", timeout=60.0)
        try:
            return json.loads(text)
        except ValueError:
            raise RuntimeError(f"/bench did not answer with JSON: {text[:200]}")

    def cards(self):
        """The cards on the page showing: [(index, label, area)]."""
        out = []
        for line in self.popup().splitlines():
            m = re.match(r"^\s*(\d+)\s+(.+?)\s+(\S+)\s+\d+ entit(?:y|ies)\s*$", line)
            if m:
                out.append((int(m.group(1)), m.group(2), m.group(3)))
        return out

    def find(self, label, area, nth=0):
        hits = [i for i, lb, ar in self.cards() if lb == label and ar == area]
        return hits[nth] if nth < len(hits) else None

    def close(self):
        return self.popup("close=1")

    def entities(self):
        """What the open window's entities say - on/off and a light's levels - from view=3, which
        only lists. Compared between captures: when a light changed between them (the owner, an
        automation), the window's differences are the house's, not the code's."""
        keep = []
        for ln in self.popup("view=3").splitlines():
            if ln.startswith("view ") or ln.lstrip().startswith("failed"):
                continue
            keep.append(re.sub(r" (pending|levels-pending)", "", ln.rstrip()))
        return keep

    def wait_history(self, limit=10.0):
        """Until History's answer has come (or failed), then a moment for the window to draw it."""
        end = time.time() + limit
        while time.time() < end:
            text = self.popup("hist=1")
            m = re.search(r"history: (\S*) (?:chart|activity), state (\d+) fail \d+ gen (\d+)/(\d+)", text)
            if not m:
                break
            ref, state, gen_view, gen_hist = m.group(1), int(m.group(2)), m.group(3), m.group(4)
            if ref in ("", "-"):
                break                            # not Home Assistant's: nothing is asked
            if state in (2, 3) and gen_view == gen_hist:
                break                            # HIST_READY or HIST_FAILED, for this view
            time.sleep(0.4)
        time.sleep(0.9)                          # the window's tick draws it (every 250 ms)


def built_us(reply):
    m = re.search(r"built in (\d+) us", reply)
    return int(m.group(1)) if m else None


def mem_used(reply):
    m = re.search(r"lv_mem used (\d+)", reply)
    return int(m.group(1)) if m else None


def tree_mem(tree_text):
    m = re.search(r"lv_mem_used=(\d+)", tree_text)
    return int(m.group(1)) if m else None


# ---------------------------------------------------------------------------------------------
# capture
# ---------------------------------------------------------------------------------------------
def capture(args):
    b = Board(args.host, args.verbose)
    label = args.label or datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = pathlib.Path(args.out) / args.board / label
    out.mkdir(parents=True, exist_ok=True)
    only = set(args.only.split(",")) if args.only else None
    run = {"host": args.host, "board": args.board, "label": label,
           "started": datetime.datetime.now().isoformat(timespec="seconds"), "states": {}}

    print(f"capture {args.host} -> {out}")
    b.close()
    b.popup("sparks=2")
    # Page 1 and back: the toast a page change leaves behind (hidden, on the top layer) is then
    # the same in every capture, whatever the board was doing before.
    first = b.page(1)
    b.page(0)
    run["was_page"] = first.get("was", {}).get("page", 0)
    run["page_count"] = first.get("page_count")
    run["heap_idle"] = first.get("heap")
    cur_page = 0
    t0 = time.time()
    try:
        for group in PLAN:
            names = [s for s, _, _ in group["steps"] if s]
            if only and not (only & set(names)):
                continue
            if group["page"] != cur_page:
                b.close()
                b.page(group["page"])
                cur_page = group["page"]
            b.close()
            whole = group.get("tree") == "page"
            build = None
            if group["card"]:
                label_, area, nth = group["card"]
                idx = b.find(label_, area, nth)
                if idx is None:
                    for s in names:
                        run["states"][s] = {"absent": f"{label_} ({area}) not on page {cur_page}"}
                        print(f"  {s:<26} absent: {label_} ({area}) is not on this board's page")
                    continue
                reply = b.popup(f"card={idx}")
                build = built_us(reply)
                if "open card" not in reply:
                    for s in names:
                        run["states"][s] = {"absent": f"card {idx} did not open: {reply.strip()[:120]}"}
                    print(f"  {names[0]:<26} card {idx} did not open: {reply.strip()[:100]}")
                    continue
                ents_open = b.entities()
            group_states = []
            for state, queries, wait in group["steps"]:
                step_build = None
                for q in queries:
                    r = b.popup(q)
                    step_build = built_us(r) or step_build
                if wait == HIST:
                    b.wait_history()
                elif wait:
                    time.sleep(wait)
                if not state or (only and state not in only):
                    continue
                tree = b.tree(page=whole)
                (out / f"{state}.txt").write_text(tree, encoding="utf-8", newline="\n")
                info = {"page": cur_page, "queries": queries, "lv_mem_used": tree_mem(tree),
                        "objects": sum(1 for ln in tree.splitlines() if ln and ln[0] not in "#@")}
                if group["card"]:
                    info["card"] = list(group["card"])
                if build is not None and state == names[0]:
                    info["open_us"] = build
                if step_build is not None:
                    info["view_us"] = step_build
                if "# TRUNCATED" in tree:
                    info["truncated"] = True
                if not args.no_png:
                    (out / f"{state}.png").write_bytes(b.screenshot())
                run["states"][state] = info
                group_states.append(state)
                print(f"  {state:<26} {info['objects']:4d} objects, lv_mem {info['lv_mem_used']}"
                      + (f", opened in {build / 1000:.1f} ms" if "open_us" in info else "")
                      + (" TRUNCATED" if info.get("truncated") else ""))
            if group["card"]:
                # The entities as the window opened and as it closes: what every state between
                # was showing.
                ents = ents_open + ["--"] + b.entities()
                for s in group_states:
                    run["states"][s]["entities"] = ents
                b.close()
                time.sleep(0.4)
    finally:
        try:
            b.close()
            b.popup("sparks=0")
            if cur_page != run.get("was_page", 0):
                b.page(run.get("was_page", 0))
        except Exception as e:  # the capture itself is what matters; say so and go on
            print(f"  (putting the board back failed: {e})")
        run["seconds"] = round(time.time() - t0, 1)
        run["log"] = b.log
        (out / "run.json").write_text(json.dumps(run, indent=1), encoding="utf-8")
    print(f"done in {run['seconds']} s: {len([s for s in run['states'].values() if 'absent' not in s])} states")
    return 0


# ---------------------------------------------------------------------------------------------
# Reading a tree
# ---------------------------------------------------------------------------------------------
class Node:
    __slots__ = ("depth", "cls", "x", "y", "w", "h", "fields", "style", "text", "children", "parent",
                 "root", "line", "vol", "volkids")

    def __init__(self):
        self.depth, self.cls = -1, ""
        self.x = self.y = self.w = self.h = 0
        self.fields, self.style = {}, {}
        self.text = None
        self.children, self.parent = [], None
        self.root, self.line, self.vol, self.volkids = "", "", False, False

    def rect(self):
        return (self.x, self.y, self.x + self.w - 1, self.y + self.h - 1)

    def describe(self):
        t = f' "{self.text[:40]}"' if self.text else ""
        return f"{self.cls} @{self.x},{self.y} {self.w}x{self.h}{t}"


OBJ_RE = re.compile(r"^( *)(\S+) @(-?\d+),(-?\d+) (-?\d+)x(-?\d+) ?(.*)$")
BLOCK_RE = re.compile(r"(?:^| )(\d+\.[0-9a-f]+)?\{([^}]*)\}")


def parse_tree(text):
    """{'header': {k: v}, 'roots': {name: Node}} - each root a container whose children are the
    objects at depth 0."""
    header, roots = {}, {}
    cur, stack = None, []
    for line in text.splitlines():
        if not line:
            continue
        if line.startswith("#"):
            body = line[1:].strip()
            for kv in body.split():
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    header[k] = v
                elif kv in ("window", "no", "TRUNCATED"):
                    header.setdefault("_kind", kv)
            if "win=" in body:   # "win=102,83 1076x545": the size is the next token
                m = re.search(r"win=(-?\d+,-?\d+) (\d+x\d+)", body)
                if m:
                    header["win"] = f"{m.group(1)} {m.group(2)}"
            continue
        if line.startswith("@"):
            cur = Node()
            cur.cls, cur.root = line[1:], line[1:]
            roots[cur.root] = cur
            stack = [cur]
            continue
        if cur is None:
            continue
        stripped = line.lstrip(" ")
        depth = (len(line) - len(stripped)) // 2
        n = Node()
        n.depth, n.root, n.line = depth, cur.root, line
        if stripped == "(list)":
            n.cls = "(list)"
        else:
            m = OBJ_RE.match(line)
            if not m:
                continue
            n.cls = m.group(2)
            n.x, n.y, n.w, n.h = (int(m.group(i)) for i in range(3, 7))
            rest = m.group(7)
            i = rest.find(" t=")   # the text is always last, after "fl=" and the rest
            if i >= 0:
                n.text, head = rest[i + 3:], rest[:i]
            else:
                head = rest
            for bm in BLOCK_RE.finditer(head):
                block = bm.group(1) or "main"
                n.style[block] = dict(kv.split("=", 1) for kv in bm.group(2).split() if "=" in kv)
            plain = BLOCK_RE.sub("", head)
            for tok in plain.split():
                if tok == "vol":          # its own place and size follow the clock
                    n.vol = True
                elif tok == "volkids":    # what it holds stands where time and data put it
                    n.volkids = True
                elif "=" in tok:
                    k, v = tok.split("=", 1)
                    n.fields[k] = v
        # Its parent: the nearest open object one level up.
        while len(stack) > depth + 1:
            stack.pop()
        parent = stack[-1]
        n.parent = parent
        parent.children.append(n)
        stack.append(n)
    return {"header": header, "roots": roots}


def walk(node):
    yield node
    for c in node.children:
        yield from walk(c)


def path_of(node):
    parts = []
    while node is not None and node.parent is not None:
        idx = node.parent.children.index(node)
        parts.append(f"{node.cls}[{idx}]")
        node = node.parent
    root = node.root if node is not None else "?"
    return root + "/" + "/".join(reversed(parts))


# ---------------------------------------------------------------------------------------------
# compare
# ---------------------------------------------------------------------------------------------
DIGITS = re.compile(r"\d+")
TIME_WORDS = re.compile(r"\d|\bago\b|just now|yesterday|today|tomorrow|\bnow\b|\bAM\b|\bPM\b", re.I)
# Places and sizes, wherever they show (a style's x/y/w/h, pads that fit a row to its room).
GEOM_STYLE = {"x", "y", "w", "h", "minw", "maxw", "minh", "maxh", "tx", "ty", "pt", "pb", "pl", "pr",
              "prow", "pcol", "mt", "mb", "ml", "mr"}
GEOM_FIELDS = {"sc", "pts", "chart"}
# The header's numbers that are measurements, not state.
HEADER_SKIP = {"lv_mem_used", "lv_mem_free", "biggest", "faces_flat"}


def mask(text):
    return DIGITS.sub("#", text) if text else text


def volatile_text(a, b):
    if a == b:
        return False
    if mask(a) == mask(b):
        return True
    # A value that was not there yet ("--") and then was.
    if {a, b} & {"--"} and any(t and any(ch.isdigit() for ch in t) for t in (a, b)):
        return True
    return bool(a and b and TIME_WORDS.search(a) and TIME_WORDS.search(b))


def key_of(n):
    """What an object is, for lining two lists of siblings up: its class and its words - or, for a
    container, the first words inside it - so a row that came or went is an insertion, and the
    rows after it still meet their twins."""
    if n.text is not None:
        return n.cls + "|" + mask(n.text)
    words = []
    for x in walk(n):
        if x is not n and x.text:
            words.append(mask(x.text))
            if len(words) == 4:
                break
    return n.cls + "|" + "/".join(words)


def match(a_nodes, b_nodes, pairs, dels, adds):
    sm = difflib.SequenceMatcher(None, [key_of(n) for n in a_nodes], [key_of(n) for n in b_nodes],
                                 autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal" or tag == "replace":
            k = min(i2 - i1, j2 - j1)
            for o in range(k):
                pairs.append((a_nodes[i1 + o], b_nodes[j1 + o]))
            dels.extend(a_nodes[i1 + k:i2])
            adds.extend(b_nodes[j1 + k:j2])
        elif tag == "delete":
            dels.extend(a_nodes[i1:i2])
        elif tag == "insert":
            adds.extend(b_nodes[j1:j2])


def compare_trees(ta, tb):
    """Returns (real, volatile): lists of (kind, path, detail)."""
    real, vol = [], []
    for k in sorted(set(ta["header"]) | set(tb["header"])):
        if k in HEADER_SKIP:
            continue
        va, vb = ta["header"].get(k), tb["header"].get(k)
        if va != vb:
            real.append(("HEADER", k, f"{va} -> {vb}"))

    pairs, dels, adds = [], [], []
    for name in sorted(set(ta["roots"]) | set(tb["roots"])):
        ra, rb = ta["roots"].get(name), tb["roots"].get(name)
        if ra is None or rb is None:
            real.append(("ROOT", name, "only in " + ("B" if ra is None else "A")))
            continue
        todo = [(ra, rb)]
        while todo:
            pa, pb = todo.pop()
            p2, d2, a2 = [], [], []
            match(pa.children, pb.children, p2, d2, a2)
            for x, y in p2:
                pairs.append((x, y))
                todo.append((x, y))
            dels.extend((pa, n) for n in d2)
            adds.extend((pb, n) for n in a2)
    to_b = {id(a): b for a, b in pairs}
    to_a = {id(b): a for a, b in pairs}

    # Where a volatile text changed SIZE, everything inside the nearest container that kept its
    # own size may have moved with it. A text that changed but kept its size moves nothing.
    scope = set()   # ids of A's nodes

    def absorb_from(node):
        n = node
        while n is not None and n.parent is not None:
            m = to_b.get(id(n))
            if m is not None and (n.w, n.h) == (m.w, m.h):
                return n
            n = n.parent
        return n

    def mark(node):
        if node is not None:
            for x in walk(node):
                scope.add(id(x))

    def datalike(node):
        return any(x.text and TIME_WORDS.search(x.text) for x in walk(node))

    for a, b in pairs:
        if a.text is not None and volatile_text(a.text, b.text):
            if (a.w, a.h) != (b.w, b.h) and a.parent is not None:
                mark(absorb_from(a.parent))
        if a.vol or b.vol:
            mark(a)
        if a.volkids or b.volkids:   # the chart's area: everything it holds, not the area itself
            for c in a.children:
                mark(c)
    # Objects that came or went, where they look like data (a new activity row, a value): what
    # holds them may have grown or shrunk.
    for pa, n in dels:
        if datalike(n):
            mark(absorb_from(pa))
    for pb, n in adds:
        twin = to_a.get(id(pb))
        if twin is not None and datalike(n):
            mark(absorb_from(twin))

    for a, b in pairs:
        p = path_of(b)
        excused = id(a) in scope
        if a.cls != b.cls:
            real.append(("CLASS", p, f"{a.cls} -> {b.cls}"))
            continue
        if a.text != b.text:
            if volatile_text(a.text, b.text):
                vol.append(("TEXT", p, f"{a.text!r} -> {b.text!r}"))
            else:
                real.append(("TEXT", p, f"{a.text!r} -> {b.text!r}"))
        if (a.x, a.y, a.w, a.h) != (b.x, b.y, b.w, b.h):
            item = ("GEOM", p, f"{a.describe()} -> @{b.x},{b.y} {b.w}x{b.h}")
            (vol if excused else real).append(item)
        for k in sorted(set(a.fields) | set(b.fields)):
            va, vb = a.fields.get(k), b.fields.get(k)
            if va != vb:
                item = ("FIELD", p, f"{k}: {va} -> {vb}  ({b.describe()})")
                (vol if (excused and k in GEOM_FIELDS) else real).append(item)
        for blk in sorted(set(a.style) | set(b.style)):
            sa, sb = a.style.get(blk, {}), b.style.get(blk, {})
            for k in sorted(set(sa) | set(sb)):
                va, vb = sa.get(k), sb.get(k)
                if va != vb:
                    item = ("STYLE", p, f"{blk} {k}: {va} -> {vb}  ({b.describe()})")
                    (vol if (excused and k in GEOM_STYLE) else real).append(item)
    for pa, n in dels:
        item = ("GONE", path_of(n), n.describe())
        (vol if id(n) in scope else real).append(item)
    for pb, n in adds:
        item = ("NEW", path_of(n), n.describe())
        twin = to_a.get(id(pb))   # excused when what holds it is in a volatile scope
        excused = pb.volkids or (twin is not None and (id(twin) in scope or twin.volkids))
        (vol if excused else real).append(item)
    return real, vol, pairs, scope


# --- screenshots: just enough PNG to read the board's own (8-bit RGB / RGBA) and write a diff --
def png_read(path):
    data = pathlib.Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, typ = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if typ == b"IHDR":
            w, h, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", chunk)
            if depth != 8 or ctype not in (2, 6) or inter:
                raise ValueError(f"{path}: only 8-bit RGB/RGBA, not interlaced")
            bpp = 3 if ctype == 2 else 4
        elif typ == b"IDAT":
            idat += chunk
        elif typ == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = w * bpp
    out = bytearray(h * w * 3)
    prev = bytearray(stride)
    i = 0
    for y in range(h):
        f = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        if f == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 255
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride):
                left = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((left + prev[x]) >> 1)) & 255
        elif f == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                b_, c = prev[x], prev[x - bpp] if x >= bpp else 0
                p = a + b_ - c
                pa, pb, pc = abs(p - a), abs(p - b_), abs(p - c)
                pr = a if pa <= pb and pa <= pc else (b_ if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        if bpp == 3:
            out[y * w * 3:(y + 1) * w * 3] = line
        else:
            out[y * w * 3:(y + 1) * w * 3] = bytes(v for k, v in enumerate(line) if k % 4 != 3)
        prev = line
    return w, h, bytes(out)


def png_write(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(typ, body):
        return struct.pack(">I", len(body)) + typ + body + struct.pack(">I", zlib.crc32(typ + body) & 0xFFFFFFFF)

    pathlib.Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                                   + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


TILE = 8


def compare_pixels(pa, pb, tb, scope_rects, window_state, diff_path):
    """Tiles of TILE px that differ, minus those a volatile object explains. Returns a summary."""
    wa, ha, A = png_read(pa)
    wb, hb, B = png_read(pb)
    if (wa, ha) != (wb, hb):
        return {"size": f"{wa}x{ha} -> {wb}x{hb}", "unexplained": -1}
    w, h = wa, ha
    # A window's capture looks only where its own opaque objects are: the page beneath is not
    # what the window's tree describes.
    considered = None
    if window_state:
        considered = []
        for root in tb["roots"].values():
            for n in walk(root):
                if n.parent is None:
                    continue
                if n.style.get("main", {}).get("bgopa") == "255" or n.cls == "image":
                    considered.append(n.rect())

    def overlaps(tx, ty, rects, pad=1):
        """Does the tile at tx,ty touch any of these rectangles?"""
        x2, y2 = tx + TILE - 1, ty + TILE - 1
        return any(r[0] - pad <= x2 and tx <= r[2] + pad and r[1] - pad <= y2 and ty <= r[3] + pad
                   for r in rects)

    diff_tiles, unexplained = 0, []
    for ty in range(0, h, TILE):
        for tx in range(0, w, TILE):
            differs = False
            for y in range(ty, min(ty + TILE, h)):
                o = (y * w + tx) * 3
                e = (y * w + min(tx + TILE, w)) * 3
                if A[o:e] != B[o:e]:
                    differs = True
                    break
            if not differs:
                continue
            if considered is not None and not overlaps(tx, ty, considered, 0):
                continue
            diff_tiles += 1
            if not overlaps(tx, ty, scope_rects):
                unexplained.append((tx, ty))
    # Merge unexplained tiles into boxes, row-runs first.
    boxes = []
    for tx, ty in unexplained:
        for bx in boxes:
            if bx[0] - TILE <= tx <= bx[2] + TILE and bx[1] - TILE <= ty <= bx[3] + TILE:
                bx[0], bx[1] = min(bx[0], tx), min(bx[1], ty)
                bx[2], bx[3] = max(bx[2], tx + TILE - 1), max(bx[3], ty + TILE - 1)
                break
        else:
            boxes.append([tx, ty, tx + TILE - 1, ty + TILE - 1])
    if unexplained and diff_path:
        img = bytearray(v // 3 for v in B)   # B, dimmed; unexplained tiles red, explained amber
        for tx, ty in unexplained:
            for y in range(ty, min(ty + TILE, h)):
                for x in range(tx, min(tx + TILE, w)):
                    o = (y * w + x) * 3
                    if A[o:o + 3] != B[o:o + 3]:
                        img[o:o + 3] = b"\xff\x20\x20"
        png_write(diff_path, w, h, bytes(img))
    return {"diff_tiles": diff_tiles, "unexplained": len(unexplained),
            "boxes": [f"{b[0]},{b[1]}..{b[2]},{b[3]}" for b in boxes[:12]]}


def resolve_dirs(args):
    if args.c is not None:
        return pathlib.Path(args.out) / args.a / args.b, pathlib.Path(args.out) / args.a / args.c
    return pathlib.Path(args.a), pathlib.Path(args.b)


def compare(args):
    da, db = resolve_dirs(args)
    for d in (da, db):
        if not d.is_dir():
            print(f"no capture at {d}")
            return 2
    runa = json.loads((da / "run.json").read_text(encoding="utf-8")) if (da / "run.json").exists() else {}
    runb = json.loads((db / "run.json").read_text(encoding="utf-8")) if (db / "run.json").exists() else {}
    states = sorted({p.stem for p in da.glob("*.txt")} | {p.stem for p in db.glob("*.txt")}, key=order_of)
    total_real, inconclusive, report = 0, [], {}
    print(f"compare {da}  ->  {db}")
    for s in states:
        fa, fb = da / f"{s}.txt", db / f"{s}.txt"
        if not fa.exists() or not fb.exists():
            print(f"  {s:<26} only in {'B' if not fa.exists() else 'A'}")
            continue
        ta = parse_tree(fa.read_text(encoding="utf-8"))
        tb = parse_tree(fb.read_text(encoding="utf-8"))
        real, vol, pairs, scope = compare_trees(ta, tb)
        entry = {"real": len(real), "volatile": len(vol)}
        sa, sb = runa.get("states", {}).get(s) or {}, runb.get("states", {}).get(s) or {}
        if sa.get("lv_mem_used") is not None and sb.get("lv_mem_used") is not None:
            entry["lv_mem"] = sb["lv_mem_used"] - sa["lv_mem_used"]
        # A light the window shows changed between the captures: what differs is the house's.
        ea, eb = sa.get("entities"), sb.get("entities")
        data = [ln for ln in difflib.ndiff(ea, eb) if ln[:1] in "+-"] if ea is not None and eb is not None \
            and ea != eb else []
        px, unexplained = "", 0
        if args.pixels and (da / f"{s}.png").exists() and (db / f"{s}.png").exists():
            rects = []
            for a, b in pairs:
                if id(a) in scope or (a.text is not None and a.text != b.text) or a.vol or b.vol:
                    rects.append(a.rect())
                    rects.append(b.rect())
            window_state = "window" in tb["roots"]
            pr = compare_pixels(da / f"{s}.png", db / f"{s}.png", tb, rects, window_state,
                                db / f"{s}.diff.png")
            entry["pixels"] = pr
            unexplained = pr["unexplained"]
            px = f", pixels: {pr.get('diff_tiles', '?')} tiles differ, {unexplained} unexplained"
        failing = bool(real) or unexplained != 0
        mem = f", lv_mem {entry['lv_mem']:+d} B" if "lv_mem" in entry else ""
        if failing and data:
            status = f"{len(real)} differences, DATA CHANGED between captures - capture again"
            inconclusive.append(s)
            entry["data_changed"] = data
        elif failing:
            status = f"{len(real)} DIFFERENCES"
            total_real += len(real) + (1 if unexplained else 0)
        else:
            status = "OK"
        print(f"  {s:<26} {status} ({len(vol)} volatile){mem}{px}")
        for ln in data[:6]:
            print(f"      data {ln}")
        for kind, p, detail in real[:args.max]:
            print(f"      {kind:<6} {p}\n             {detail}")
        if len(real) > args.max:
            print(f"      ... and {len(real) - args.max} more")
        if args.volatile:
            for kind, p, detail in vol[:args.max]:
                print(f"      (vol) {kind:<6} {p}\n             {detail}")
        if args.pixels and entry.get("pixels", {}).get("boxes"):
            print(f"      unexplained pixels at {', '.join(entry['pixels']['boxes'])}  -> {s}.diff.png")
        entry["details"] = [list(x) for x in real[:200]]
        report[s] = entry
    # Build times, where both have them (one sample each: see measure for medians).
    times = []
    for s in states:
        oa = (runa.get("states", {}).get(s) or {}).get("open_us")
        ob = (runb.get("states", {}).get(s) or {}).get("open_us")
        if oa and ob:
            times.append(f"{s} {oa / 1000:.1f} -> {ob / 1000:.1f} ms")
    if times:
        print("  window opened in (one sample each): " + "; ".join(times))
    (db / f"compare_{da.name}.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    print("RESULT: " + ("no differences" if not total_real else f"{total_real} difference(s)")
          + (f"; {len(inconclusive)} state(s) to capture again, the house changed: "
             + ",".join(inconclusive) if inconclusive else ""))
    return 1 if total_real else (3 if inconclusive else 0)


def order_of(state):
    k = 0
    for g in PLAN:
        for s, _, _ in g["steps"]:
            if s == state:
                return k
            k += 1
    return k


# ---------------------------------------------------------------------------------------------
# measure
# ---------------------------------------------------------------------------------------------
def measure(args):
    b = Board(args.host, args.verbose)
    label = args.label or datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = pathlib.Path(args.out) / args.board / label
    out.mkdir(parents=True, exist_ok=True)
    b.close()
    b.popup("sparks=2")
    first = b.page(0)
    was = first.get("was", {}).get("page", 0)
    cur = 0
    res = {"host": args.host, "board": args.board, "n": args.n, "cards": {}}
    print(f"measure {args.host}, {args.n} opens each (sparks off)")
    try:
        for page, card in MEASURE:
            if page != cur:
                b.close()
                b.page(page)
                cur = page
            idx = b.find(*card)
            name = f"{card[0]} ({card[1]})"
            if idx is None:
                print(f"  {name:<22} absent")
                continue
            opens, costs, leaks = [], [], []
            for _ in range(args.n):
                before = mem_used(b.popup("mem=1"))
                r = b.popup(f"card={idx}")
                time.sleep(0.6)
                after = mem_used(b.popup("mem=1"))
                b.close()
                time.sleep(0.5)
                closed = mem_used(b.popup("mem=1"))
                if built_us(r) is not None:
                    opens.append(built_us(r))
                if before is not None and after is not None:
                    costs.append(after - before)
                if before is not None and closed is not None:
                    leaks.append(closed - before)
            row = {"open_us_median": int(statistics.median(opens)) if opens else None,
                   "open_us_min": min(opens) if opens else None, "open_us": opens,
                   "lv_mem_cost_median": int(statistics.median(costs)) if costs else None,
                   "lv_mem_after_close": leaks}
            res["cards"][name] = row
            if not opens:
                print(f"  {name:<22} did not open")
                continue
            print(f"  {name:<22} open median {row['open_us_median'] / 1000:6.1f} ms (min "
                  f"{row['open_us_min'] / 1000:.1f}), costs {row['lv_mem_cost_median']} B of LVGL's pool,"
                  f" after close {leaks}")
    finally:
        b.close()
        b.popup("sparks=0")
        if cur != was:
            b.page(was)
        (out / "measure.json").write_text(json.dumps(res, indent=1), encoding="utf-8")
    return 0


# ---------------------------------------------------------------------------------------------
# bootlog
# ---------------------------------------------------------------------------------------------
def bootlog(args):
    try:
        import serial  # PlatformIO's own Python has pyserial
    except ImportError:
        print("pyserial is missing: run this with ~/.platformio/penv/Scripts/python.exe")
        return 2
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = args.port, 115200, 0.2
    s.dtr, s.rts = False, False   # opening must not reset or hold the chip in its bootloader
    s.open()
    # A normal boot: EN pulsed low through RTS, with IO0 (DTR) left high. Boards whose Serial is
    # the chip's own USB (the 1060, the 7B) drop off the bus on reset; read those through HA.
    s.rts = True
    time.sleep(0.15)
    s.rts = False
    end, buf, lines = time.time() + args.seconds, b"", []
    while time.time() < end:
        buf += s.read(4096)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode("utf-8", errors="replace").rstrip("\r")
            lines.append(text)
            if "[Heap]" in text or args.all:
                print(text)
    s.close()
    if args.out:
        pathlib.Path(args.out).write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("capture", help="save every state's tree and screenshot")
    c.add_argument("host")
    c.add_argument("--board", required=True, help="a folder name: WS_P4_5, WS_P4_4B, CYD_P4_1060")
    c.add_argument("--label", help="the capture's name (default: the time)")
    c.add_argument("--out", default=str(OUT_DIR))
    c.add_argument("--only", help="comma-separated states")
    c.add_argument("--no-png", action="store_true", help="trees only")
    c.add_argument("-v", "--verbose", action="store_true")
    c.set_defaults(func=capture)
    k = sub.add_parser("compare", help="two captures: BOARD LABEL_A LABEL_B, or DIR_A DIR_B")
    k.add_argument("a")
    k.add_argument("b")
    k.add_argument("c", nargs="?")
    k.add_argument("--out", default=str(OUT_DIR))
    k.add_argument("--pixels", action="store_true", help="compare the screenshots too")
    k.add_argument("--volatile", action="store_true", help="list the excused differences as well")
    k.add_argument("--max", type=int, default=12, help="differences listed per state")
    k.set_defaults(func=compare)
    m = sub.add_parser("measure", help="build time and LVGL pool per window, N opens each")
    m.add_argument("host")
    m.add_argument("--board", required=True)
    m.add_argument("--label")
    m.add_argument("--out", default=str(OUT_DIR))
    m.add_argument("--n", type=int, default=7)
    m.add_argument("-v", "--verbose", action="store_true")
    m.set_defaults(func=measure)
    s = sub.add_parser("bootlog", help="reset a board by its serial port; print its [Heap] lines")
    s.add_argument("port")
    s.add_argument("--seconds", type=float, default=25.0)
    s.add_argument("--all", action="store_true", help="print every line")
    s.add_argument("--out", help="save the whole log here")
    s.set_defaults(func=bootlog)
    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
