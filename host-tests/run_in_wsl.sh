#!/usr/bin/env bash
# The host-tests suites without a Windows compiler: WSL's g++ (Ubuntu ships it), the flags of
# platformio.ini, against the Unity that `pio test -d host-tests` fetched on Windows - run that
# once first; it fetches Unity, then stops for want of gcc. From the repo root, in PowerShell:
#
#     wsl bash host-tests/run_in_wsl.sh
#
# Prints Unity's PASS / FAIL for every test; exits non-zero if any fails or will not build.
set -u
cd "$(dirname "$0")/.."
U=host-tests/.pio/libdeps/native/Unity/src
if [ ! -f "$U/unity.c" ]; then
    echo "Unity is not fetched yet: run 'pio test -d host-tests' once on Windows, then this again."
    exit 2
fi
SRC=(src/Layout/Layout.cpp src/TimeFormat.cpp)   # the same files as build_src_filter
O=/tmp/fleet_host_tests
mkdir -p "$O"
gcc -c "$U/unity.c" -I "$U" -o "$O/unity.o" || exit 1
rc=0
for t in host-tests/test/test_*/; do
    name=$(basename "$t")
    echo "== $name"
    if g++ -std=gnu++17 -Wall -Wextra -I include -I "$U" "${t}test_main.cpp" "${SRC[@]}" "$O/unity.o" -o "$O/$name"; then
        "$O/$name" || rc=1
    else
        rc=1
    fi
done
exit $rc
