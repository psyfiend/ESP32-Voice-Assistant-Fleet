param([string]$Out = "")
# FLASH A RELEASE to the P4 boards on the desk, built from a clean tree so the
# firmware reports the bare tag (FW_VERSION "0.2.12.0", no "+dirty"). The
# owner's local debug lines are backed up, stashed and popped back exactly as
# gate.ps1 does. Run after tagging, on main.
#
#   powershell -ExecutionPolicy Bypass -File scripts/release/flash_release.ps1
#
# Boards and ports are the desk's (HANDOFF): check them by USB serial first if
# anything was replugged.
$ErrorActionPreference = "Stop"
$repo = (git rev-parse --show-toplevel).Trim()
Set-Location $repo
if (-not $Out) { $Out = Join-Path $repo ".pio\release" }
New-Item -ItemType Directory -Force $Out | Out-Null
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
$log = Join-Path $Out "flash.log"
$boards = @(@("WS_P4_TOUCH_LCD_5", "COM15"), @("WS_P4_TOUCH_LCD_4B", "COM7"), @("CYD_P4_1060P470", "COM9"))

$hasLocal = [bool](git status --porcelain platformio.ini)
if ($hasLocal) {
    Copy-Item platformio.ini (Join-Path $Out "platformio.local.ini") -Force
    git stash push -m "local debug flags (release flash)" -- platformio.ini | Out-File $log
} else { "platformio.ini has no local changes" | Out-File $log }
try {
    "describe: " + (git describe --tags --dirty) | Out-File $log -Append
    foreach ($b in $boards) {
        & $pio run -e $b[0] -t upload --upload-port $b[1] *> (Join-Path $Out ("flash_" + $b[0] + ".log"))
        "{0,-20} {1}" -f $b[0], $(if ($LASTEXITCODE -eq 0) { "FLASHED" } else { "FAILED" }) | Out-File $log -Append
    }
    Select-String -Path (Join-Path $repo "include\fleet_fw_version.h") -Pattern "FW_VERSION" |
        ForEach-Object { $_.Line } | Out-File $log -Append
}
finally {
    if ($hasLocal) {
        git stash pop | Out-File $log -Append
        if (-not (git status --porcelain platformio.ini)) {
            Copy-Item (Join-Path $Out "platformio.local.ini") platformio.ini -Force
            "stash pop did not restore it - restored from the backup copy" | Out-File $log -Append
        }
        "restored: " + ((git diff --stat platformio.ini) -join " ") | Out-File $log -Append
    }
}
Get-Content $log
