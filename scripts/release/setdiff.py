"""Compare two settings files key by key (a board's saved copy against what it serves now):
python scripts/release/setdiff.py bench/settings/<env>/<saved>.json now.json
A test that changed the owner's settings is done when this prints "0 difference(s)"."""
import json
import sys


def flat(d, p=""):
    o = {}
    for k, v in d.items():
        if isinstance(v, dict):
            o.update(flat(v, p + k + "."))
        else:
            o[p + k] = v
    return o


a = flat(json.load(open(sys.argv[1], encoding="utf-8")))
b = flat(json.load(open(sys.argv[2], encoding="utf-8")))
n = 0
for k in sorted(set(a) | set(b)):
    if a.get(k) != b.get(k):
        print(k, a.get(k), "->", b.get(k))
        n += 1
print(n, "difference(s)")
