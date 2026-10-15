#!/usr/bin/env python3
"""
pre_commit.py - what git checks before every commit in this repo, whoever makes
it (scripts/hooks/pre-commit calls this; `git config core.hooksPath
scripts/hooks` turns it on - once per clone).

Refuses the commit, saying why, when the staged changes:
  1. add a LOCAL-ONLY debug line to platformio.ini (the owner keeps
     -D DEBUG_POPUP / DEBUG_HANG / DEBUG_FRAMES uncommitted - HANDOFF;
     DEBUG_SETTINGS_NOMOUNT is A17's test, a boot that cannot save);
  2. put a control byte into a text file - the silent half of the heredoc
     corruption CLAUDE.md's STOP section is about (0x08, 0x00...);
  3. include a secrets header.

Added 2026-10-13 (owner): rules in markdown are interpreted; this runs.
"""

import subprocess
import sys

LOCAL_ONLY_FLAGS = ("DEBUG_POPUP", "DEBUG_HANG", "DEBUG_FRAMES", "DEBUG_SETTINGS_NOMOUNT")
SECRETS = ("ConnectivityLocalSecrets.h", "MqttLocalSecrets.h")
BINARY_EXT = (".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".bin", ".elf", ".ttf", ".otf",
              ".woff", ".woff2", ".zip", ".gz", ".pdf", ".webp", ".a", ".o")


def git(*args):
    return subprocess.run(["git", *args], capture_output=True).stdout


def main():
    problems = []
    staged = git("diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z").decode("utf-8", "replace")
    files = [f for f in staged.split("\0") if f]

    for f in files:
        name = f.replace("\\", "/").rsplit("/", 1)[-1]
        if name in SECRETS:
            problems.append("%s: a secrets header must never be committed" % f)

    if "platformio.ini" in files:
        diff = git("diff", "--cached", "-U0", "--", "platformio.ini").decode("utf-8", "replace")
        for line in diff.splitlines():
            if line.startswith("+") and not line.startswith("+++"):
                for flag in LOCAL_ONLY_FLAGS:
                    if ("-D " + flag) in line or ("-D" + flag) in line:
                        problems.append("platformio.ini: adds '%s' - the owner's local-only debug line "
                                        "(unstage platformio.ini, or that hunk)" % line[1:].strip())

    for f in files:
        if f.lower().endswith(BINARY_EXT):
            continue
        data = git("show", ":" + f)
        if b"\0" in data[:8000] and not f.endswith((".c", ".h", ".cpp", ".md", ".py", ".ini", ".txt", ".json")):
            continue   # a binary file git knows as one
        bad = sorted({b for b in data if b < 9 or (13 < b < 32 and b != 27)})
        if bad:
            problems.append("%s: control bytes %s - a corrupted write (CLAUDE.md, STOP); fix it with Edit"
                            % (f, ", ".join("0x%02X" % b for b in bad)))

    if problems:
        sys.stderr.write("\npre-commit (scripts/hooks/pre_commit.py) refused this commit:\n")
        for p in problems:
            sys.stderr.write("  - " + p + "\n")
        sys.stderr.write("\n")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
