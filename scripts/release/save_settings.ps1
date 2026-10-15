param(
    [Parameter(Mandatory = $true, Position = 0)][string]$HostName,
    [Parameter(Mandatory = $true, Position = 1)][string]$Board,
    [string]$Dir = ""
)
# SAVE A BOARD'S SETTINGS before it is flashed (A17): GET /settings, checked to
# be a settings file, kept as bench/settings/<board>/<yyyymmdd-hhmm>.json
# (bench/ is gitignored). An earlier copy is never overwritten: a second save in
# the same minute gets "-2", so a wiped board cannot replace its last good copy.
# Exits 1 when the board does not answer or what comes back is not a settings
# file, so a caller can refuse to flash.
#
#   powershell -ExecutionPolicy Bypass -File scripts/release/save_settings.ps1 fleet-ws-p4-5 WS_P4_TOUCH_LCD_5
#
# Put a copy back with restore_settings.ps1.
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"   # Invoke-WebRequest's progress bar is slow in 5.1
$repo = (git rev-parse --show-toplevel).Trim()
if (-not $Dir) { $Dir = Join-Path $repo "bench\settings" }
$folder = Join-Path $Dir $Board

try {
    $r = Invoke-WebRequest -UseBasicParsing -TimeoutSec 15 -Uri "http://$HostName/settings"
} catch {
    Write-Output ("NO BACKUP of {0}: {1} did not answer ({2})" -f $Board, $HostName, $_.Exception.Message)
    exit 1
}
$bytes = $r.RawContentStream.ToArray()
try {
    $doc = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
} catch { $doc = $null }
if (-not $doc -or -not $doc.schema) {
    Write-Output ("NO BACKUP of {0}: {1}/settings did not answer with a settings file ({2} bytes)" -f $Board, $HostName, $bytes.Length)
    exit 1
}

New-Item -ItemType Directory -Force $folder | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$file = Join-Path $folder ($stamp + ".json")
for ($i = 2; Test-Path $file; $i++) { $file = Join-Path $folder ("{0}-{1}.json" -f $stamp, $i) }
[IO.File]::WriteAllBytes($file, $bytes)

$cards = if ($doc.cards) { @($doc.cards.PSObject.Properties).Count } else { 0 }
Write-Output ("saved {0}: {1} bytes, {2} card(s) -> {3}" -f $Board, $bytes.Length, $cards, $file)
exit 0
