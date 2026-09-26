#!/usr/bin/env python3
"""Time where a frame goes on one or more fleet boards (milestone 2.9, #67).

Each board built with -D ENABLE_BENCH serves GET /bench on port 80 (see
src/UI/Bench.cpp for what every field means). This runs the standard matrix -

    page 0          full screen, one card
    page 1          full screen, one card
    page 0 + deck   full screen, one card

- takes a /screenshot of each scenario so every number comes with a picture of
what was drawn, puts the board back on the page and deck it started on, and
prints a table. Everything is saved under bench/ (gitignored):

    bench/<host>_<stamp>[_<label>].json     every reply, verbatim
    bench/<host>_<stamp>[_<label>]_p0d1.png what each scenario looked like

    python scripts/bench.py fleet-ws-p4-5 fleet-cyd-s3-3248
    python scripts/bench.py --label ppa --n 30 fleet-ws-p4-5

The UI freezes while each scenario is measured. Standard library only.

--anim runs a different test instead: /bench?what=anim opens one deck panel
while the other closes (the P4_5 stutter, 2026-09-26), --swaps times, and
records every frame LVGL draws meanwhile without forcing any. Printed per swap:
how many frames, how long each took, how much of that was drawing (render) and
how much the flush (copy + present + wait), and the start-to-start interval
between frames - which is what the eye sees. LVGL aims for one frame every
refr_period_ms (33); a longer interval is a late frame.

    python scripts/bench.py --anim fleet-ws-p4-5
    python scripts/bench.py --anim --frames --swaps 2 10.0.0.42
"""

import argparse
import datetime
import http.client
import json
import pathlib
import sys
import time
import urllib.error
import urllib.request

DEV_BOARDS = ["fleet-cyd-s3-3248", "fleet-ws-p4-5"]
SCENARIOS = [(0, 0), (1, 0), (0, 1)]   # (page, deck)
WHATS = ["full", "card"]
FIELDS = ["total", "render", "copy", "present", "wait"]

OUT_DIR = pathlib.Path(__file__).resolve().parent.parent / "bench"


def get(url, timeout):
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=timeout) as resp:
                return resp.read()
        except urllib.error.HTTPError as e:
            # 503 = a bench or screenshot already running on that board.
            if e.code == 503 and attempt < 2:
                time.sleep(5)
                continue
            raise RuntimeError(f"HTTP {e.code}: {e.read().decode(errors='replace').strip()}")
        except (OSError, http.client.HTTPException) as e:
            # A dropped connection. Retried once, and said out loud: on the CYD
            # this has meant low internal heap, and uptime_s in the next reply
            # tells whether the board rebooted.
            if attempt == 0:
                print(f"  (connection dropped: {e!r}; retrying once)")
                time.sleep(10)
                continue
            raise
    raise RuntimeError("busy after 3 attempts")


def bench(host, timeout, **q):
    query = "&".join(f"{k}={v}" for k, v in q.items())
    return json.loads(get(f"http://{host}/bench?{query}", timeout))


def ms(us):
    return f"{us / 1000:7.1f}"


def run_host(host, args, stamp):
    base = f"{host}_{stamp}" + (f"_{args.label}" if args.label else "")
    replies = []
    first = None
    print(f"\n== {host}")
    for page, deck in SCENARIOS:
        for what in WHATS:
            r = bench(host, args.timeout, n=args.n, what=what, page=page, deck=deck, keep=1)
            first = first or r
            replies.append(r)
            u = r["us"]
            print(f"  p{page}{'+deck' if deck else '     '} {r['scenario'].get('scheme', '?')[:8]:<8} {what:<4} "
                  + " ".join(f"{f} {ms(u[f]['avg'])}" for f in FIELDS)
                  + f"  ms | {r['chunks']:4.1f} chunks {r['px']:>7} px  <= {r['fps_ceiling']:5.1f} fps")
        # The board is still on this page/deck (keep=1): photograph it.
        png = get(f"http://{host}/screenshot", args.timeout)
        (OUT_DIR / f"{base}_p{page}d{deck}.png").write_bytes(png)

    # Put it back as we found it.
    was = first["was"]
    bench(host, args.timeout, n=1, page=was["page"], deck=1 if was["deck"] else 0)

    (OUT_DIR / f"{base}.json").write_text(json.dumps(replies, indent=1))
    c = first
    print(f"  {c['board']} / {c['panel']} {c['bus']} {c['res'][0]}x{c['res'][1]} rot {c['rotation']}, "
          f"{c['buf']['count']} x {c['buf']['bytes'] // 1024} KB {c['buf']['where']} ({c['buf']['lines']} lines), "
          f"LV_USE_PPA {c['lv_use_ppa']}, fw {c['fw']}")
    up = [r.get("uptime_s") for r in replies]
    if None not in up:
        rebooted = any(b < a for a, b in zip(up, up[1:]))
        print(f"  uptime {up[0]} s -> {up[-1]} s, last reset reason {replies[-1].get('reset_reason')}"
              + ("  ** REBOOTED DURING THE RUN **" if rebooted else ""))
    print(f"  saved {base}.json + {len(SCENARIOS)} screenshots in bench/")


def anim_rows(reply):
    """The frame rows as dicts, with render and the start-to-start interval."""
    a = reply["anim"]
    rows = [dict(zip(a["cols"], r)) for r in a["rows"]]
    prev = None
    for r in rows:
        r["render"] = r["total"] - r["copy"] - r["present"] - r["wait"]
        r["flush"] = r["copy"] + r["present"] + r["wait"]
        same_swap = prev is not None and prev["swap"] == r["swap"]
        r["interval"] = r["start"] - prev["start"] if same_swap else None
        prev = r
    return rows


def avg(xs):
    return sum(xs) / len(xs) if xs else 0.0


def run_anim(host, args, stamp):
    base = f"{host}_{stamp}_anim" + (f"_{args.label}" if args.label else "")
    print(f"\n== {host}  (anim: one deck panel opens while the other closes)")
    replies = []
    for page in args.pages:
        r = bench(host, args.timeout, what="anim", n=args.swaps, page=page, deck=1)
        replies.append(r)
        a = r["anim"]
        period = a["refr_period_ms"] * 1000
        rows = anim_rows(r)
        print(f"  p{page} {r['scenario'].get('scheme', '?')[:8]:<8} {a['frames']} frames recorded"
              + (f", {a['dropped']} DROPPED from the record" if a["dropped"] else "")
              + f"; LVGL aims for one every {a['refr_period_ms']} ms")
        for s in range(a["swaps"]):
            fr = [x for x in rows if x["swap"] == s]
            if not fr:
                print(f"    swap {s}: no frames drawn")
                continue
            # The animation proper: up to the first frame showing both panels
            # where they end up. Later frames are other things (the header's
            # icons, say), not the swap.
            end = fr[-1]["h_a"], fr[-1]["h_b"]
            n_anim = next(i for i, x in enumerate(fr) if (x["h_a"], x["h_b"]) == end) + 1
            an = fr[:n_anim]
            ivs = [x["interval"] for x in an if x["interval"] is not None]
            late = sum(1 for iv in ivs if iv > period * 1.5)
            span = an[-1]["start"] + an[-1]["total"] - an[0]["start"]
            print(f"    swap {s} (opens {'B' if s % 2 == 0 else 'A'}): {n_anim:2d} frames in {span / 1000:6.1f} ms"
                  f" | frame avg {avg([x['total'] for x in an]) / 1000:5.1f} max {max(x['total'] for x in an) / 1000:5.1f}"
                  f" = render {avg([x['render'] for x in an]) / 1000:5.1f} + flush {avg([x['flush'] for x in an]) / 1000:5.1f}"
                  f" | interval avg {avg(ivs) / 1000:5.1f} max {max(ivs, default=0) / 1000:5.1f}"
                  f" | late {late}"
                  f" | first frame {an[0]['start'] / 1000:.1f} ms after the tap"
                  + (f" | +{len(fr) - n_anim} after" if len(fr) > n_anim else ""))
        if args.frames:
            # areas / repair: esp_lcd only (0 on Arduino_GFX). repair = pixels the
            # PPA copied to bring the buffer up to date first; FULL = the whole frame.
            print("      swap   start  interval   total  render    copy present    wait       px  ch   h_a   h_b"
                  "  areas   repair   (ms, px)")
            for x in rows:
                iv = f"{x['interval'] / 1000:8.1f}" if x["interval"] is not None else "       -"
                rep = "FULL" if x.get("repair_full") else f"{x.get('repair_px', 0):8d}"
                print(f"      {x['swap']:4d} {x['start'] / 1000:7.1f} {iv} {x['total'] / 1000:7.1f} {x['render'] / 1000:7.1f}"
                      f" {x['copy'] / 1000:7.1f} {x['present'] / 1000:7.1f} {x['wait'] / 1000:7.1f}"
                      f" {x['px']:8d} {x['chunks']:3d} {x['h_a']:5d} {x['h_b']:5d}"
                      f" {x.get('areas', 0):6d} {rep:>8}")
    c = replies[0]
    print(f"  {c['board']} / {c['panel']} {c['bus']} {c['res'][0]}x{c['res'][1]} rot {c['rotation']}, "
          f"flush {c.get('flush_path', '?')}, {c['buf']['count']} x {c['buf']['bytes'] // 1024} KB {c['buf']['where']}"
          f" ({c['buf']['lines']} lines), fw {c['fw']}")
    up = [r.get("uptime_s") for r in replies]
    print(f"  uptime {up[0]} s -> {up[-1]} s, last reset reason {replies[-1].get('reset_reason')}"
          + ("  ** REBOOTED DURING THE RUN **" if any(b < a for a, b in zip(up, up[1:])) else ""))
    (OUT_DIR / f"{base}.json").write_text(json.dumps(replies, indent=1))
    print(f"  saved {base}.json in bench/")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("hosts", nargs="*", help=f"hostnames or IPs (default: {' '.join(DEV_BOARDS)})")
    ap.add_argument("--n", type=int, default=20, help="frames per scenario, 1-100 (default 20)")
    ap.add_argument("--label", default="", help="tag for the saved files, e.g. 'ppa'")
    ap.add_argument("--timeout", type=float, default=60.0, help="seconds per request (default 60)")
    ap.add_argument("--anim", action="store_true", help="the deck-panel swap test instead of the matrix")
    ap.add_argument("--swaps", type=int, default=4, help="--anim: panel swaps per page, 1-8 (default 4)")
    ap.add_argument("--pages", type=int, nargs="+", default=[0], help="--anim: pages to run on (default 0)")
    ap.add_argument("--frames", action="store_true", help="--anim: also print every frame")
    args = ap.parse_args()

    OUT_DIR.mkdir(exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    failed = 0
    print("times are ms per frame, averaged; render = total - copy - present - wait")
    for host in args.hosts or DEV_BOARDS:
        try:
            if args.anim:
                run_anim(host, args, stamp)
            else:
                run_host(host, args, stamp)
        except Exception as e:  # report and carry on to the next board
            print(f"FAIL  {host}: {e}")
            failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
