#!/usr/bin/env python3
"""Fetch a PNG screenshot from one or more fleet boards (issue #58).

Each board built with -D ENABLE_SCREENSHOT serves GET /screenshot on port 80.
This saves every board's picture into screenshots/ (gitignored) as
<host>_<YYYYmmdd-HHMMSS>.png.

    python scripts/screenshot.py                  # every board in FLEET below
    python scripts/screenshot.py fleet-ws-p4-5    # one board, by hostname
    python scripts/screenshot.py 192.168.0.186    # or by IP
    python scripts/screenshot.py --query "z=9&f=-1" fleet-ws-p4-5

Standard library only - nothing to install.
"""

import argparse
import datetime
import pathlib
import sys
import time
import urllib.error
import urllib.request

# DHCP hostnames, as the boards register them (DeviceIdentity::hostname(),
# MAC suffix off). They resolve through the router's DNS on the owner's network.
FLEET = [
    "fleet-cyd-s3-3248",
    "fleet-ws-p4-5",
    "fleet-ws-p4-7b",
    "fleet-ws-p4-4b",
    "fleet-ws-s3-4b",
]

OUT_DIR = pathlib.Path(__file__).resolve().parent.parent / "screenshots"


def fetch(host, query, timeout):
    url = f"http://{host}/screenshot" + (f"?{query}" if query else "")
    for attempt in range(3):
        start = time.monotonic()
        try:
            with urllib.request.urlopen(url, timeout=timeout) as resp:
                data = resp.read()
                render_ms = resp.headers.get("X-Render-Ms", "?")
            return data, time.monotonic() - start, render_ms
        except urllib.error.HTTPError as e:
            # 503 = another capture in progress on that board; worth a retry.
            if e.code == 503 and attempt < 2:
                time.sleep(2)
                continue
            raise RuntimeError(f"HTTP {e.code}: {e.read().decode(errors='replace').strip()}")
    raise RuntimeError("busy after 3 attempts")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("hosts", nargs="*", help="hostnames or IPs (default: the whole fleet)")
    ap.add_argument("--query", default="", help="encoder knobs passed through, e.g. 'z=9&f=-1'")
    ap.add_argument("--timeout", type=float, default=30.0, help="seconds per board (default 30)")
    args = ap.parse_args()

    OUT_DIR.mkdir(exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    failed = 0

    for host in args.hosts or FLEET:
        try:
            data, secs, render_ms = fetch(host, args.query, args.timeout)
        except Exception as e:  # report and carry on to the next board
            print(f"FAIL  {host:<20} {e}")
            failed += 1
            continue
        path = OUT_DIR / f"{host}_{stamp}.png"
        path.write_bytes(data)
        print(f"ok    {host:<20} {len(data) // 1024:>5} KB  {secs:5.2f} s  (render {render_ms} ms)  -> {path.name}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
