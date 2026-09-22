#!/usr/bin/env python3
"""
verify_p4_sdkconfig.py - is the rebuilt P4 config the one we meant to build?

Step 5 of docs/REBUILD_P4_LIBS.md, and the step that makes the whole exercise
safe rather than hopeful. We are replacing 136 prebuilt libraries in order to
change TWO settings. This answers "did we change two things, or two hundred?"
without anyone reading 1,968 config lines by eye.

    python scripts/verify_p4_sdkconfig.py <path-to-new-sdkconfig>

Compares against reference/framework-baseline/sdkconfig.esp32p4_es.55.03.311,
the configuration the fleet runs today, captured before any of this started.

Exit code 0 means safe to install. Anything else means stop and read.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join(ROOT, "reference", "framework-baseline",
                        "sdkconfig.esp32p4_es.55.03.311")

# The two we are deliberately changing, and what we expect them to become.
INTENDED = {
    "CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM": "y",
    "CONFIG_CACHE_L2_CACHE_LINE_64B": "y",
}

# Settings the two above are expected to drag with them. Changing a cache line
# size is not a local edit - IDF recomputes sizes and alignments from it - so
# these are consequences rather than surprises.
EXPECTED_FALLOUT = (
    "CONFIG_CACHE_L2_CACHE_LINE_SIZE",
    "CONFIG_CACHE_L2_CACHE_LINE_128B",
    "CONFIG_ESP_HOSTED_MEMPOOL",
)

# THE VARIANT GUARD. docs/REBUILD_P4_LIBS.md names building the wrong chip
# variant as the number one risk: esp32p4 is rev3+, esp32p4_es is the pre-rev3
# silicon we actually have. A wrong build here would link, boot, and then
# misbehave in ways indistinguishable from new bugs.
MUST_HOLD = {
    "CONFIG_ESP32P4_SELECTS_REV_LESS_V3": "y",
    "CONFIG_ESP32P4_REV_MIN_1": "y",
}


def load(path):
    """Parse an sdkconfig into {symbol: value}. `# X is not set` becomes 'n'."""
    out = {}
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            m = re.match(r"^(CONFIG_[A-Za-z0-9_]+)=(.*)$", line)
            if m:
                out[m.group(1)] = m.group(2)
                continue
            m = re.match(r"^# (CONFIG_[A-Za-z0-9_]+) is not set$", line)
            if m:
                out[m.group(1)] = "n"
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__.strip())
    new_path = sys.argv[1]

    for p in (BASELINE, new_path):
        if not os.path.exists(p):
            sys.exit("missing: %s" % p)

    base = load(BASELINE)
    new = load(new_path)

    print("baseline : %s  (%d symbols)" % (os.path.basename(BASELINE), len(base)))
    print("new      : %s  (%d symbols)" % (new_path, len(new)))
    print()

    # --- The variant guard runs first. Nothing else matters if this is wrong.
    bad_variant = []
    for k, want in MUST_HOLD.items():
        got = new.get(k, "<absent>")
        if got != want:
            bad_variant.append((k, want, got))

    if bad_variant:
        print("!! WRONG CHIP VARIANT - DO NOT INSTALL THIS BUILD !!")
        for k, want, got in bad_variant:
            print("   %s : expected %s, got %s" % (k, want, got))
        print()
        print("   You almost certainly ran `-t esp32p4` instead of `-t esp32p4_es`.")
        print("   build.sh matches -t against the CHIP VARIANT, not the target.")
        print("   See docs/REBUILD_P4_LIBS.md step 3.")
        return 2
    print("chip variant : esp32p4_es confirmed (pre-rev3, matches our silicon)")
    print()

    # --- Classify every difference.
    intended, fallout, unexpected, missing, added = [], [], [], [], []

    for k in sorted(set(base) | set(new)):
        b, n = base.get(k), new.get(k)
        if b == n:
            continue
        if k in INTENDED:
            intended.append((k, b, n))
        elif any(k.startswith(p) for p in EXPECTED_FALLOUT):
            fallout.append((k, b, n))
        elif b is None:
            added.append((k, n))
        elif n is None:
            missing.append((k, b))
        else:
            unexpected.append((k, b, n))

    def show(title, rows, fmt):
        print("%s (%d)" % (title, len(rows)))
        for r in rows:
            print("   " + fmt(r))
        if not rows:
            print("   none")
        print()

    show("INTENDED - the two we came here to change", intended,
         lambda r: "%s : %s -> %s" % r)
    show("EXPECTED FALLOUT - implied by the cache line change", fallout,
         lambda r: "%s : %s -> %s" % r)
    show("UNEXPECTED - differ and should not", unexpected,
         lambda r: "%s : %s -> %s" % r)
    show("ONLY IN THE NEW BUILD", added, lambda r: "%s = %s" % r)
    show("ONLY IN THE BASELINE", missing, lambda r: "%s = %s" % r)

    # --- Verdict.
    for k, want in INTENDED.items():
        got = new.get(k, "<absent>")
        if got != want:
            print("!! %s is %s, expected %s - menuconfig did not take." % (k, got, want))
            return 3

    noise = len(unexpected) + len(added) + len(missing)
    print("-" * 60)
    if noise == 0:
        print("SAFE TO INSTALL. Both options set; nothing else moved.")
        return 0

    print("STOP AND READ. %d unexplained difference(s)." % noise)
    print()
    print("A handful of build-id or version strings is usually benign - check them")
    print("by eye. Dozens of functional differences means the branch, the IDF")
    print("version or the config arguments do not match what we run today, and")
    print("installing it would change many things at once with nothing")
    print("attributable. See docs/REBUILD_P4_LIBS.md step 5.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
