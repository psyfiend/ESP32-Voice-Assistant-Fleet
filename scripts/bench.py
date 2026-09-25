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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("hosts", nargs="*", help=f"hostnames or IPs (default: {' '.join(DEV_BOARDS)})")
    ap.add_argument("--n", type=int, default=20, help="frames per scenario, 1-100 (default 20)")
    ap.add_argument("--label", default="", help="tag for the saved files, e.g. 'ppa'")
    ap.add_argument("--timeout", type=float, default=60.0, help="seconds per request (default 60)")
    args = ap.parse_args()

    OUT_DIR.mkdir(exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    failed = 0
    print("times are ms per frame, averaged; render = total - copy - present - wait")
    for host in args.hosts or DEV_BOARDS:
        try:
            run_host(host, args, stamp)
        except Exception as e:  # report and carry on to the next board
            print(f"FAIL  {host}: {e}")
            failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
