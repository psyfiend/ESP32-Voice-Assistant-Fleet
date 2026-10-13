# The guard, locked (managed settings) - installed by the owner, with admin rights

The guard in `scripts/hooks/claude_guard.py` also runs from the project's `.claude/settings.json`,
but those are ordinary files Claude can edit. Installed here, **Claude cannot change or remove it**:
`C:\Program Files\ClaudeCode\` needs Windows administrator rights to write, Claude runs as the
owner's normal user, and managed settings outrank every other settings file (`disableAllHooks` is
pinned to `false` so no lower file can switch hooks off; the deny rules here cannot be overridden).

It applies to every Claude Code session on this PC, in every project. The rules are about git and
deletes, so they are harmless elsewhere.

## Install (or update) - in a PowerShell opened with "Run as administrator"

```powershell
$repo = "C:\Users\Marge\Documents\PlatformIO\Projects\ESP32 Voice Assistant Fleet"
New-Item -ItemType Directory -Force "C:\Program Files\ClaudeCode\hooks" | Out-Null
Copy-Item "$repo\scripts\hooks\managed\managed-settings.json" "C:\Program Files\ClaudeCode\managed-settings.json" -Force
Copy-Item "$repo\scripts\hooks\claude_guard.py" "C:\Program Files\ClaudeCode\hooks\claude_guard.py" -Force
```

Then restart Claude (the desktop app), so new sessions load it.

**After any change to `claude_guard.py` or this file** (they arrive through a pull request the
owner merged), run the same commands again - the copies in Program Files do not follow the repo.

## Remove

Delete `C:\Program Files\ClaudeCode\managed-settings.json` (admin), restart Claude.
