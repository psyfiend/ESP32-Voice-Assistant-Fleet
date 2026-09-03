"""
PlatformIO pre-build hook: derive FW_VERSION from git and inject it as a build flag.

Versioning scheme is A.B.C.D (see docs/ROADMAP.md section 3.3):

    A  major   breaking changes            \
    B  minor   new features, compatible     >  you set these by tagging: `git tag v0.1.0`
    C  patch   fixes, performance          /
    D  build   every commit                  <- AUTOMATIC, never typed by hand

D is the number of commits since the most recent tag, so it increments on its own and
cannot drift. You only ever tag A.B.C.

    tag v0.1.0, then 14 commits  ->  0.1.0.14
    uncommitted local changes    ->  0.1.0.14+dirty
    no tags in the repo yet      ->  0.0.0.<total commits>

The "+dirty" suffix matters when debugging a board on a wall: it makes a hand-modified
local build impossible to mistake for a real tagged release.

Wired up from platformio.ini via:  extra_scripts = pre:scripts/fw_version.py
"""

import subprocess

Import("env")  # noqa: F821  (injected by PlatformIO/SCons)


def _git(*args):
    """Run a git command in the project dir, return stripped stdout or None."""
    try:
        out = subprocess.check_output(
            ["git"] + list(args),
            cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
            stderr=subprocess.DEVNULL,
        )
        return out.decode("utf-8", "replace").strip()
    except Exception:
        # No git, not a repo, git not on PATH - all non-fatal. Build must not break
        # just because version metadata is unavailable.
        return None


def _describe():
    """Return (version, commit) derived from git state."""
    commit = _git("rev-parse", "--short=8", "HEAD") or "unknown"

    # --long always includes the count+sha, even when HEAD is exactly on a tag,
    # which keeps parsing uniform instead of special-casing the on-tag form.
    desc = _git("describe", "--tags", "--long", "--match", "v[0-9]*")

    if desc:
        # e.g. "v0.1.0-14-gabc12345"
        base, count, _sha = desc.rsplit("-", 2)
        version = "{}.{}".format(base.lstrip("v"), count)
    else:
        # No matching tag yet. Fall back to total commit count so the number is
        # still monotonic and still tells you which build you're looking at.
        count = _git("rev-list", "--count", "HEAD") or "0"
        version = "0.0.0.{}".format(count)

    # Any tracked modification, staged or not, makes this not a reproducible build.
    if _git("status", "--porcelain", "--untracked-files=no"):
        version += "+dirty"

    return version, commit


version, commit = _describe()

env.Append(  # noqa: F821
    CPPDEFINES=[
        ("FW_VERSION", env.StringifyMacro(version)),  # noqa: F821
        ("FW_COMMIT", env.StringifyMacro(commit)),  # noqa: F821
    ]
)

print("fw_version: FW_VERSION={} FW_COMMIT={}".format(version, commit))
