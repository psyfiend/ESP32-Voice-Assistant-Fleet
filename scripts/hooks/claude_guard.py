#!/usr/bin/env python3
"""
claude_guard.py - a PreToolUse hook that refuses destructive shell commands.

Wired in .claude/settings.json for the Bash and PowerShell tools. Claude Code
runs it before every such command and passes the call as JSON on stdin; a
command matching a rule below is refused (permissionDecision "deny") with the
reason, and never runs. Everything else passes untouched (no output).

WHY (owner, 2026-10-13): rules in CLAUDE.md are read and interpreted; this is
run by the harness on every call and cannot be skipped. It is there to stop
ACCIDENTS - the realistic risk - not a determined workaround: a command hidden
inside a script file it cannot see (a .ps1 that runs `git clean`) passes. The
other layers: GitHub's protection of main (no force push, no deletion), the
git pre-commit hook (scripts/hooks/pre-commit), and a backup of the files git
does not hold (secrets headers, reference/, the local platformio.ini).

To change a rule: edit RULES, run the self-test (python claude_guard.py --test)
and commit. To get past one deliberately, the owner runs the command himself.
"""

import json
import re
import sys

# Paths a recursive delete may touch: build output and scratch space only.
SAFE_DELETE = re.compile(r"(\.pio|build_cache|Temp[\\/]+claude|[\\/]tmp[\\/])", re.I)

# `git` and its global options, up to the subcommand - so a rule matches the
# subcommand itself, never a word in a commit message.
G = r"\bgit(?:\s+(?:-C\s+\S+|-c\s+\S+|--no-pager|-q|--git-dir=\S+))*\s+"
REST = r"[^;&|\n]*"   # the rest of this statement
# `gh` where a command starts (the start, or after ; & | ( or a newline) - not
# the words "gh pr merge" inside a commit message.
GH = r"(?:^|[;&|(\n])\s*gh\s+"

# (name, pattern, reason). Searched anywhere in the command, so `cd x; git
# reset --hard` is caught as well as a bare one.
RULES = [
    ("force push",
     G + r"push\b" + REST + r"(\s--force\b|\s--force-with-lease\b|\s-f\b|\s\+[\w./-]+)",
     "Force-pushing rewrites published history."),
    ("delete main",
     G + r"(branch\s+-[dD]\s+main\b|push\b" + REST + r"(--delete\s+main\b|\s:main\b))",
     "Deleting main is never needed."),
    ("reset --hard",
     G + r"reset\b" + REST + r"--hard\b",
     "git reset --hard discards uncommitted work, including the owner's local debug lines."),
    ("git clean",
     G + r"clean\b(?!" + REST + r"(\s-n\b|--dry-run))",
     "git clean deletes untracked files - the secrets headers, reference/, bench/ are untracked."),
    ("discard changes",
     G + r"(checkout\s+(\S+\s+)?--\s|checkout\s+\.(\s|$)|restore\b(?!" + REST + r"--staged))",
     "This discards uncommitted changes in the working tree."),
    ("drop stash",
     G + r"stash\s+(drop|clear)\b",
     "Dropping a stash can lose the owner's local platformio.ini lines."),
    ("skip git hooks",
     G + r"(commit\b" + REST + r"(\s--no-verify\b|\s-n\b)|config\b" + REST +
     r"(--unset" + REST + r"core\.hookspath|core\.hookspath\s+\S))",
     "The pre-commit hook guards the debug lines and file bytes; it is not to be skipped or re-pointed."),
    ("secrets headers",
     r"(\brm\b|\bdel\b|Remove-Item|\bmv\b|Move-Item|Rename-Item|Set-Content|Out-File|Clear-Content|" + G +
     r"(rm|checkout|restore)\b|>)" + REST + r"LocalSecrets\.h",
     "The secrets headers are not in git and cannot be recovered."),
    ("history rewrite",
     G + r"(filter-branch|filter-repo|rebase\b" + REST + r"\s-i\b|update-ref\s+-d)",
     "Rewriting history is not done here."),
    ("tag delete",
     G + r"(tag\s+-d\b|push\b" + REST + r"--delete\s+v\d|push\b" + REST + r"\s:refs/tags/)",
     "Release tags are permanent (W3)."),
    # --- THE OWNER MERGES (2026-10-13, W12): main takes changes only through a
    # pull request, and only the owner clicks Merge, in the browser. GitHub's
    # ruleset refuses any push to main; these refuse the doors GitHub cannot
    # tell from his own (Claude acts with his login).
    ("push to main",
     G + r"push\b" + REST + r"(\smain\b|:main\b|:refs/heads/main\b|\s--all\b|\s--mirror\b)",
     "main takes changes only through a pull request the owner merges (W12). Push the branch and open a PR."),
    ("merge a PR",
     GH + r"pr\s+merge\b",
     "Only the owner merges a pull request, in the browser (W12)."),
    ("repo settings",
     GH + r"(repo\s+(edit|delete|rename|archive)|ruleset\s+(create|delete|edit|update))\b",
     "The repository's settings and rulesets are the owner's."),
]


def gh_api_write(cmd):
    """A `gh api` call that would merge, or change rules, refs or settings."""
    for m in re.finditer(GH + r"api\b" + REST, cmd):
        call = m.group(0)
        sensitive = re.search(r"(/merges?\b|/rulesets\b|/protection\b|/git/refs\b|/branches/|repos/[^/\s]+/[^/\s]+\s*($|\s))",
                              call)
        writes = re.search(r"(-X\s*|--method[\s=]+)(POST|PUT|PATCH|DELETE)\b|\s-[fF]\s|\s--(raw-)?field\b|\s--input\b",
                           call, re.I)
        if sensitive and writes:
            return True
    return False


def bare_push_on_main(cmd, cwd):
    """`git push` with no refspec while main is checked out pushes main."""
    if not cwd:
        return False
    for m in re.finditer(G + r"push\b(" + REST + r")", cmd):
        args = [a for a in m.group(1).split() if not a.startswith("-")]
        if len(args) <= 1:   # nothing, or only a remote name
            try:
                import subprocess
                head = subprocess.run(["git", "-C", cwd, "rev-parse", "--abbrev-ref", "HEAD"],
                                      capture_output=True, text=True, timeout=5).stdout.strip()
            except Exception:
                return False
            if head == "main":
                return True
    return False

# A recursive delete outside the safe paths. Checked separately: the paths
# decide it, not just the verb.
RECURSIVE_DELETE = re.compile(
    r"(\brm\b[^;&|\n]*\s-[a-zA-Z]*[rR][a-zA-Z]*\b"
    r"|Remove-Item\b[^;&|\n]*-Recurse"
    r"|\brmdir\b[^;&|\n]*\s/s\b|\brd\b[^;&|\n]*\s/s\b"
    r"|\bdel\b[^;&|\n]*\s/s\b"
    r"|shutil\.rmtree)", re.I)


def verdict(cmd, cwd=None):
    """None if the command may run, else (rule name, reason)."""
    for name, pat, reason in RULES:
        if re.search(pat, cmd, re.I):
            return name, reason
    if gh_api_write(cmd):
        return ("GitHub API write",
                "Merging, and changing rulesets, branch protection or refs through the API, is the owner's (W12).")
    if bare_push_on_main(cmd, cwd):
        return ("push to main", "main is checked out, so this pushes main - which takes changes only "
                                "through a pull request the owner merges (W12).")
    for m in RECURSIVE_DELETE.finditer(cmd):
        # The rest of that statement: its paths decide.
        tail = re.split(r"[;&|\n]", cmd[m.start():], maxsplit=1)[0]
        if not SAFE_DELETE.search(tail):
            return ("recursive delete",
                    "Recursive deletes are allowed only under .pio/ or the scratchpad.")
    return None


def main():
    try:
        call = json.load(sys.stdin)
    except Exception:
        return 0   # not ours to judge
    cmd = (call.get("tool_input") or {}).get("command") or ""
    v = verdict(cmd, call.get("cwd"))
    if not v:
        return 0
    name, reason = v
    print(json.dumps({
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "deny",
            "permissionDecisionReason":
                "Blocked by scripts/hooks/claude_guard.py (" + name + "): " + reason +
                " If it is really needed, ask the owner to run it.",
        }
    }))
    return 0


# --- self-test ---------------------------------------------------------------
BLOCK = [
    "git push --force origin main", "git push -f", "git push origin +main",
    "git push --force-with-lease", "git branch -D main", "git push origin --delete main",
    "git push origin :main", "git reset --hard HEAD~1", "cd x; git reset --hard",
    "git clean -fdx", "git clean -fd", "git checkout -- platformio.ini", "git checkout .",
    "git restore platformio.ini", "git stash drop", "git stash clear",
    "git commit --no-verify -m x", "git config core.hooksPath /dev/null",
    "git -C repo reset --hard", "git -c a=b push --force", "git config --unset core.hooksPath",
    "git push -q origin main", "git push origin HEAD:main", "git push --all", "git push --mirror x",
    "gh pr merge 12 --merge", "cd x && gh pr merge --admin",
    "gh api -X PUT repos/psyfiend/x/pulls/3/merge", "gh api --method DELETE repos/o/r/rulesets/1",
    "gh api repos/o/r/rulesets/1 -X PATCH --input r.json", "gh api -X DELETE repos/o/r/git/refs/heads/main",
    "gh repo edit --default-branch x", "gh repo delete o/r",
    "rm components/Fleet_Connectivity/ConnectivityLocalSecrets.h",
    "Remove-Item components/Fleet_MQTT/MqttLocalSecrets.h",
    "echo x > components/Fleet_MQTT/MqttLocalSecrets.h",
    "rm -rf reference", "rm -rf .", "Remove-Item -Recurse -Force docs",
    "rmdir /s /q src", "git filter-branch --tree-filter x", "git tag -d v0.2.12",
    "git push origin --delete v0.2.12",
]
ALLOW = [
    "git push", "git push origin v0.2.12", "git push -u origin feat/x", "git push -q -u origin chore/guards",
    "git push origin feat/maintenance", "gh pr create --fill", "gh pr view 3", "gh pr list",
    "gh api repos/o/r/rulesets", "gh api repos/o/r/rulesets/24984804 -q .name",
    "gh api repos/o/r/pulls/3", "gh issue create --title x --body y",
    "git commit -q -m \"the guard refuses gh pr merge and gh api -X PUT repos/o/r/rulesets/1\"",
    "git status --short", "git commit -q -m \"fix: x\"", "git merge --no-ff feat/x -m m",
    "git stash push -m x -- platformio.ini", "git stash pop", "git stash list",
    "git restore --staged x", "git checkout -q main", "git checkout -b feat/x",
    "git clean -n", "git tag -a v0.2.13 -m x", "git branch -d feat/old",
    "Get-Content components/Fleet_Connectivity/ConnectivityLocalSecrets.h -Raw",
    "rm -rf .pio/build_cache", "Remove-Item -Recurse -Force .pio\\build\\X",
    "rm -rf C:/Users/Marge/AppData/Local/Temp/claude/x/scratchpad/y",
    "pio run -e WS_P4_TOUCH_LCD_5", "grep -rn reset src",
    "git commit -q -m \"restore the old row; reset --hard is not used\"",
    "git log --oneline -5", "git config core.hooksPath",
]


def selftest():
    bad = 0
    for c in BLOCK:
        if not verdict(c):
            print("SHOULD BLOCK:", c); bad += 1
    for c in ALLOW:
        v = verdict(c)
        if v:
            print("SHOULD ALLOW:", c, "->", v[0]); bad += 1
    print("%d blocked, %d allowed, %d wrong" % (len(BLOCK), len(ALLOW), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(selftest() if "--test" in sys.argv else main())
