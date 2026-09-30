#!/usr/bin/env python3
"""Soak one or more fleet boards through /bench (milestone 2.9, #67).

The pattern the 2026-09-26 soaks ran by hand, as a script: for --minutes, each
board cycles through

    full screen x5, page 0          (the whole flush, every frame)
    one card x10, page 1            (odd block sizes - the PPA freeze's trigger)
    deck-panel swaps x2             (what=anim)
    page changes x2                 (what=page)
    verify x10 with deck swaps      (esp_lcd only: panel buffer vs LVGL's render)
    verify x10 with page changes    (esp_lcd only)

and counts what matters: failed requests, bad verify checks, reboots (uptime
going backwards), a UI thread that stops answering (503 "did not respond" -
what a PPA freeze looks like: the screen stops, the network keeps going), the
worst frame seen and the lowest internal heap. Boards run in parallel, one
thread each. Every reply is kept, one JSON per line, in
bench/soak_<host>_<stamp>.jsonl (gitignored).

    python scripts/soak.py fleet-ws-p4-7b fleet-cyd-p4-1060
    python scripts/soak.py --minutes 120 fleet-ws-p4-5

The UI freezes while each run is measured, so a soaking board is not usable.
Standard library only.
"""

import argparse
import datetime
import json
import pathlib
import sys
import threading
import time
import urllib.error
import urllib.request

CYCLE = [
    ("full",        "what=full&n=5&page=0&deck=0"),
    ("card",        "what=card&n=10&page=1&deck=0"),
    ("anim",        "what=anim&n=2"),
    ("page",        "what=page&n=2"),
    ("verify_anim", "what=verify&n=10&act=anim&gap=100"),
    ("verify_page", "what=verify&n=10&act=page&gap=100"),
]

print_lock = threading.Lock()


def say(msg):
    with print_lock:
        print(msg, flush=True)


def fetch(host, query, timeout):
    """(status, body): status is 'ok', 'busy', 'frozen', 'refused' or 'error'."""
    try:
        with urllib.request.urlopen(f"http://{host}/bench?{query}", timeout=timeout) as r:
            return "ok", r.read().decode(errors="replace")
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace").strip()
        if e.code == 503 and "did not respond" in body:
            return "frozen", body
        if e.code == 503:
            return "busy", body
        return "refused", f"HTTP {e.code}: {body}"   # e.g. verify on an Arduino_GFX board
    except Exception as e:                           # timeouts, dropped connections
        return "error", repr(e)


def soak(host, minutes, timeout, stamp, results):
    out = pathlib.Path("bench") / f"soak_{host}_{stamp}.jsonl"
    out.parent.mkdir(exist_ok=True)
    s = dict(host=host, runs=0, failed=0, frozen=0, refused=set(), bad_checks=0, checks=0,
             reboots=0, worst_frame_ms=0.0, worst_at="", min_internal=None,
             first_uptime=None, last_uptime=None, errors=[])
    end = time.time() + minutes * 60
    next_note = time.time() + 300
    with out.open("w") as log:
        while time.time() < end:
            for name, query in CYCLE:
                if time.time() >= end:
                    break
                if name in s["refused"]:
                    continue
                status, body = fetch(host, query, timeout)
                s["runs"] += 1
                stamp_now = datetime.datetime.now().strftime("%H:%M:%S")
                if status == "busy":
                    time.sleep(5)
                    continue
                if status == "refused":
                    s["refused"].add(name)       # this board cannot do it; stop asking
                    say(f"  {host}: {name} not available here ({body[:80]}) - skipped from now on")
                    continue
                if status in ("frozen", "error"):
                    s["failed"] += 1
                    s["frozen"] += status == "frozen"
                    s["errors"].append(f"{stamp_now} {name}: {body[:120]}")
                    say(f"  {host}: {stamp_now} {name} {status.upper()}: {body[:120]}")
                    log.write(json.dumps({"t": stamp_now, "run": name, "status": status,
                                          "body": body[:500]}) + "\n")
                    time.sleep(10)
                    continue
                try:
                    r = json.loads(body)
                except ValueError:
                    s["failed"] += 1
                    s["errors"].append(f"{stamp_now} {name}: reply was not JSON")
                    continue
                log.write(json.dumps({"t": stamp_now, "run": name, "reply": r}) + "\n")
                log.flush()
                up = r.get("uptime_s")
                if up is not None:
                    if s["last_uptime"] is not None and up < s["last_uptime"]:
                        s["reboots"] += 1
                        say(f"  {host}: {stamp_now} REBOOT (uptime {s['last_uptime']} -> {up} s, "
                            f"reset reason {r.get('reset_reason')})")
                    if s["first_uptime"] is None:
                        s["first_uptime"] = up
                    s["last_uptime"] = up
                v = r.get("verify")
                if isinstance(v, dict):
                    s["checks"] += v.get("checks", 0)
                    s["bad_checks"] += v.get("bad_checks", 0)
                    if v.get("bad_checks"):
                        say(f"  {host}: {stamp_now} {name} {v.get('bad_checks')} BAD CHECKS")
                wf = find_key(r, "worst_frame_us")
                if wf and wf / 1000 > s["worst_frame_ms"]:
                    s["worst_frame_ms"] = wf / 1000
                    s["worst_at"] = f"{stamp_now} {name}"
                ia = find_key(r, "internal_after")
                if ia:   # verify replies carry 0 here: they do not measure the heap
                    s["min_internal"] = ia if s["min_internal"] is None else min(s["min_internal"], ia)
            if time.time() >= next_note:
                next_note += 300
                say(f"  {host}: {s['runs']} runs, {s['failed']} failed, {s['bad_checks']} bad checks, "
                    f"{s['reboots']} reboots, uptime {s['last_uptime']} s")
    results[host] = s


def find_key(obj, key):
    """First value of `key` anywhere in a nested reply."""
    if isinstance(obj, dict):
        if key in obj:
            return obj[key]
        for v in obj.values():
            f = find_key(v, key)
            if f is not None:
                return f
    elif isinstance(obj, list):
        for v in obj:
            f = find_key(v, key)
            if f is not None:
                return f
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("hosts", nargs="+", help="hostnames or IPs")
    ap.add_argument("--minutes", type=float, default=40)
    ap.add_argument("--timeout", type=float, default=120, help="seconds per request")
    args = ap.parse_args()

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    say(f"soak {args.minutes:g} min from {datetime.datetime.now():%H:%M}: {', '.join(args.hosts)}")
    results = {}
    threads = [threading.Thread(target=soak, args=(h, args.minutes, args.timeout, stamp, results))
               for h in args.hosts]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    bad = False
    say("")
    for h in args.hosts:
        s = results.get(h)
        if s is None:
            say(f"== {h}: the soak thread died - see above")
            bad = True
            continue
        ok = s["failed"] == 0 and s["bad_checks"] == 0 and s["reboots"] == 0
        bad |= not ok
        say(f"== {h}: {'PASS' if ok else 'FAIL'}")
        say(f"  {s['runs']} runs, {s['failed']} failed ({s['frozen']} UI-thread freezes), "
            f"{s['reboots']} reboots, uptime {s['first_uptime']} -> {s['last_uptime']} s")
        say(f"  verify: {s['checks']} checks, {s['bad_checks']} bad"
            + (f" (not run: {', '.join(sorted(s['refused']))})" if s["refused"] else ""))
        say(f"  worst frame {s['worst_frame_ms']:.1f} ms ({s['worst_at']}); "
            f"lowest internal heap {s['min_internal']}")
        for e in s["errors"][:10]:
            say(f"    {e}")
        say(f"  replies: bench/soak_{h}_{stamp}.jsonl")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
