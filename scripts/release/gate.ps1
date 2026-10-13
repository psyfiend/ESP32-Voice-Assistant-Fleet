param([string]$Out = "")
# THE ALL-NINE RELEASE GATE (W2): every environment built from the COMMITTED
# platformio.ini. The owner's local debug lines (uncommitted, never committed)
# are copied to a backup file, stashed for the build, and popped back in a
# `finally` - whatever happens, including a failed build or Ctrl+C.
#
#   powershell -ExecutionPolicy Bypass -File scripts/release/gate.ps1
#
# Results: .pio/release/gate.log (one line per board with its flash use) and a
# build log per board beside it. Run it from a clean tree apart from
# platformio.ini; it refuses otherwise.
$ErrorActionPreference = "Stop"
$repo = (git rev-parse --show-toplevel).Trim()
Set-Location $repo
if (-not $Out) { $Out = Join-Path $repo ".pio\release" }
New-Item -ItemType Directory -Force $Out | Out-Null
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
$log = Join-Path $Out "gate.log"

$dirty = git status --porcelain --untracked-files=no | Where-Object { $_ -notmatch "platformio\.ini$" }
if ($dirty) { "Refusing: commit or set aside these first:"; $dirty; exit 1 }

$hasLocal = [bool](git status --porcelain platformio.ini)
if ($hasLocal) {
    Copy-Item platformio.ini (Join-Path $Out "platformio.local.ini") -Force   # the backup, before anything
    git stash push -m "local debug flags (release gate)" -- platformio.ini | Out-File $log
} else { "platformio.ini has no local changes" | Out-File $log }
try {
    $envs = "WS_P4_TOUCH_LCD_7B","WS_P4_TOUCH_LCD_5","WS_P4_TOUCH_LCD_4B","CYD_P4_1060P470","CYD_P4_4880P443",
            "WS_S3_TOUCH_LCD_4B","CYD_S3_3248W535","WS_S3_TOUCH_LCD_5B","CYD_S3_8048W550"
    $start = Get-Date
    foreach ($e in $envs) {
        $t0 = Get-Date
        & $pio run -e $e *> (Join-Path $Out "build_$e.log")
        $ok = $LASTEXITCODE -eq 0
        $flash = (Select-String -Path (Join-Path $Out "build_$e.log") -Pattern "Flash: .*" | Select-Object -Last 1).Matches.Value
        "{0,-20} {1,-8} {2:mm\:ss}  {3}" -f $e, $(if ($ok) { "SUCCESS" } else { "FAILED" }), ((Get-Date) - $t0), $flash |
            Out-File $log -Append
    }
    "all nine in {0:hh\:mm\:ss}" -f ((Get-Date) - $start) | Out-File $log -Append
}
finally {
    if ($hasLocal) {
        git stash pop | Out-File $log -Append
        if (-not (git status --porcelain platformio.ini)) {
            Copy-Item (Join-Path $Out "platformio.local.ini") platformio.ini -Force   # pop failed: the backup
            "stash pop did not restore it - restored from the backup copy" | Out-File $log -Append
        }
        "restored: " + ((git diff --stat platformio.ini) -join " ") | Out-File $log -Append
    }
}
Get-Content $log
