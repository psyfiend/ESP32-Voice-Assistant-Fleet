param([string]$Out = "", [switch]$NoBackup)
# FLASH A RELEASE to the P4 boards on the desk, built from a clean tree so the
# firmware reports the bare tag (FW_VERSION "0.2.12.0", no "+dirty"). The
# owner's local debug lines are backed up, stashed and popped back exactly as
# gate.ps1 does. Run after tagging, on main.
#
#   powershell -ExecutionPolicy Bypass -File scripts/release/flash_release.ps1
#
# Boards and ports are the desk's (HANDOFF): check them by USB serial first if
# anything was replugged.
#
# EACH BOARD'S SETTINGS ARE SAVED FIRST (A17): save_settings.ps1 keeps its
# /settings as bench/settings/<board>/<yyyymmdd-hhmm>.json. A board whose copy
# cannot be made is NOT flashed and is reported; -NoBackup flashes it anyway
# (a board that does not boot cannot answer). restore_settings.ps1 puts a copy
# back.
#
# THE 1060 IS WRITTEN APP-ONLY AT 460800, ENDING WITH A WATCHDOG RESET (LESSONS,
# 2026-10-13: two 1.5 Mbaud uploads lost its USB, and only a watchdog reset
# brings this native-USB P4 out of a download mode). pio builds it; esptool
# writes it.
$ErrorActionPreference = "Stop"
$repo = (git rev-parse --show-toplevel).Trim()
Set-Location $repo
if (-not $Out) { $Out = Join-Path $repo ".pio\release" }
New-Item -ItemType Directory -Force $Out | Out-Null
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
$py = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
$esptool = "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py"
$log = Join-Path $Out "flash.log"
# environment, port, hostname, how it is written ("pio": pio's upload; "slow": app-only, esptool, 460800)
$boards = @(@("WS_P4_TOUCH_LCD_5", "COM15", "fleet-ws-p4-5", "pio"),
            @("WS_P4_TOUCH_LCD_4B", "COM7", "fleet-ws-p4-4b", "pio"),
            @("CYD_P4_1060P470", "COM9", "fleet-cyd-p4-1060", "slow"))
$saver = Join-Path $repo "scripts\release\save_settings.ps1"

$hasLocal = [bool](git status --porcelain platformio.ini)
if ($hasLocal) {
    Copy-Item platformio.ini (Join-Path $Out "platformio.local.ini") -Force
    git stash push -m "local debug flags (release flash)" -- platformio.ini | Out-File $log
} else { "platformio.ini has no local changes" | Out-File $log }
try {
    "describe: " + (git describe --tags --dirty) | Out-File $log -Append
    foreach ($b in $boards) {
        $saved = & powershell -NoProfile -ExecutionPolicy Bypass -File $saver $b[2] $b[0]
        $saved | Out-File $log -Append
        if ($LASTEXITCODE -ne 0 -and -not $NoBackup) {
            "{0,-20} NOT FLASHED: no settings backup (-NoBackup to flash anyway)" -f $b[0] | Out-File $log -Append
            continue
        }
        $boardLog = Join-Path $Out ("flash_" + $b[0] + ".log")
        # A native program's stderr is an error record in 5.1 and fatal under "Stop" (gate.ps1):
        # the result is the exit code, stderr is only logged.
        $ErrorActionPreference = "Continue"
        if ($b[3] -eq "slow") {
            & $pio run -e $b[0] *> $boardLog
            if ($LASTEXITCODE -eq 0) {
                $bin = Join-Path $repo (".pio\build\" + $b[0] + "\firmware.bin")
                & $py $esptool --chip esp32p4 -p $b[1] -b 460800 --before default-reset --after watchdog-reset `
                    write-flash 0x10000 $bin *>> $boardLog
            }
        } else {
            & $pio run -e $b[0] -t upload --upload-port $b[1] *> $boardLog
        }
        $ok = $LASTEXITCODE -eq 0
        $ErrorActionPreference = "Stop"
        "{0,-20} {1}" -f $b[0], $(if ($ok) { "FLASHED" } else { "FAILED (see $boardLog)" }) | Out-File $log -Append
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
