param(
    [Parameter(Mandatory = $true, Position = 0)][string]$HostName,
    [Parameter(Mandatory = $true, Position = 1)][string]$File,
    [switch]$NoReboot
)
# PUT A SAVED SETTINGS FILE BACK (A17): POST /settings with the file as the body.
# The board checks it (JSON, an object, a schema it knows) on its HTTP task, its
# settings task writes it, and it answers with the result. It then restarts, so
# every page and setting is read from the file again; -NoReboot leaves it
# running (pages show the file when next built, everything at the next boot).
# A board whose boot is read-only (a failed mount, A17) refuses, and says why.
#
#   powershell -ExecutionPolicy Bypass -File scripts/release/restore_settings.ps1 fleet-ws-p4-5 bench\settings\WS_P4_TOUCH_LCD_5\20261015-0542.json
#
# The copies are made by save_settings.ps1, and by flash_release.ps1 before it
# writes a board.
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"   # Invoke-WebRequest's progress bar is slow in 5.1
# esp_http_server does not answer "Expect: 100-continue"; without this, 5.1
# waits for it before sending the body.
[Net.ServicePointManager]::Expect100Continue = $false

if (-not (Test-Path $File)) { Write-Output "no such file: $File"; exit 1 }
$bytes = [IO.File]::ReadAllBytes((Resolve-Path $File).Path)
try {
    $doc = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
} catch { $doc = $null }
if (-not $doc -or -not $doc.schema) { Write-Output "not a settings file: $File"; exit 1 }

$uri = "http://$HostName/settings" + $(if ($NoReboot) { "" } else { "?reboot=1" })
try {
    $r = Invoke-WebRequest -UseBasicParsing -TimeoutSec 20 -Method Post -Uri $uri -Body $bytes `
        -ContentType "application/json"
    Write-Output ("{0}: {1}" -f $HostName, $r.Content.Trim())
} catch {
    # A refusal (400, 409) carries the board's reason as its body; 5.1 has
    # already read the stream, and keeps the text in ErrorDetails.
    if ($_.ErrorDetails -and $_.ErrorDetails.Message) {
        Write-Output ("{0}: {1}" -f $HostName, $_.ErrorDetails.Message.Trim())
    } elseif ($_.Exception.Response) {
        Write-Output ("{0}: refused ({1})" -f $HostName, $_.Exception.Message)
    } else {
        Write-Output ("{0} did not answer ({1})" -f $HostName, $_.Exception.Message)
    }
    exit 1
}
exit 0
