<#
Creates the GitHub milestones, labels and issues for docs/ROADMAP.md.

Prerequisites:
    gh auth login
    gh auth refresh -s project      # only if you also want the Project board

Run once:
    powershell -ExecutionPolicy Bypass -File scripts\setup_tracker.ps1

Idempotent: skips milestones and issues whose titles already exist, so re-running
after adding roadmap entries only creates the new ones.
#>

$ErrorActionPreference = "Stop"
$gh = "C:\Program Files\GitHub CLI\gh.exe"
if (-not (Test-Path $gh)) { $gh = "gh" }

& $gh auth status *> $null
if ($LASTEXITCODE -ne 0) { Write-Error "Not logged in. Run: gh auth login"; exit 1 }

$repo = (& $gh repo view --json nameWithOwner -q .nameWithOwner)
Write-Output "Repo: $repo"

# --- labels -------------------------------------------------------------
$labels = @(
    @{ n = "phase-0"; c = "5319e7"; d = "Repo hygiene" },
    @{ n = "phase-1"; c = "0e8a16"; d = "Connectivity: WiFi, MQTT, HA discovery" },
    @{ n = "phase-2"; c = "1d76db"; d = "UI foundation: registry, cards, grid, nav" },
    @{ n = "phase-3"; c = "fbca04"; d = "Build sheet" },
    @{ n = "phase-4"; c = "d93f0b"; d = "Settings, navbar, web config" },
    @{ n = "phase-5"; c = "b60205"; d = "OTA" },
    @{ n = "phase-6"; c = "c5def5"; d = "Card + hardware library expansion" },
    @{ n = "spike";   c = "e99695"; d = "Time-boxed investigation, not shippable work" },
    @{ n = "blocked"; c = "000000"; d = "Waiting on an external answer or another issue" }
)
foreach ($l in $labels) {
    & $gh label create $l.n --color $l.c --description $l.d 2>$null | Out-Null
}
Write-Output "Labels ensured."

# --- milestones ---------------------------------------------------------
$milestones = @(
    "Phase 0 - Repo hygiene",
    "Phase 1 - Connectivity",
    "Phase 2 - UI foundation",
    "Phase 3 - Build sheet",
    "Phase 4 - Settings, navbar, web config",
    "Phase 5 - OTA",
    "Phase 6 - Expansion"
)
$existing = (& $gh api "repos/$repo/milestones?state=all" -q ".[].title")
foreach ($m in $milestones) {
    if ($existing -contains $m) { Write-Output "  = $m"; continue }
    & $gh api "repos/$repo/milestones" -f title="$m" | Out-Null
    Write-Output "  + $m"
}

# --- issues -------------------------------------------------------------
# phase, title, body
$issues = @(
    @("0", "0.3 Flash-test LVGL 9.5.0 for visual regressions", "LVGL was bumped from an untagged v9.4.0+134 commit to the v9.5.0 release tag. Both dev targets build clean (RAM +0, flash +0.7% on CYD_S3_3248), but no board has been flashed yet.`n`nAcceptance: WS_P4_TOUCH_LCD_7B and CYD_S3_3248W535 flashed; panels render, touch works, no new visual artefacts."),
    @("0", "0.7 Clone test: prove a fresh clone builds", "Clone the repo into a scratch dir, run ``git submodule update --init --recursive``, and build.`n`nThis is the milestone that proves the whole Phase 0 submodule fix actually worked. Before it passes, everything in Phase 0 is unverified.`n`nAcceptance: fresh clone builds WS_P4_TOUCH_LCD_7B with no manual intervention."),
    @("0", "0.9 Review lv_conf.h against LVGL 9.5 template", "``include/lvgl/lv_conf.h`` is still labelled 'Configuration file for v9.4.0'. There is no hard version gate and the build is correct, but options added in 9.5 are silently taking defaults instead of being reviewed.`n`nRelevant to the Phase 2 memory work - draw-buffer and cache options live here.`n`nAcceptance: diffed against 9.5's lv_conf_template.h; new options reviewed and set deliberately; header re-labelled."),
    @("1", "1.1 Connectivity state machine", "Replace the current 4-state enum with a formal state machine: BOOT -> STA_CONNECTING -> STA_CONNECTED -> AP_FALLBACK -> APSTA -> DEGRADED, plus the 4 user-selectable modes (OFF / STA_WITH_AP_FALLBACK / STA_PLUS_AP / STA_ONLY).`n`nThe link type must be part of the model from the start (WIFI_STA | WIFI_AP | WIFI_APSTA | ETHERNET | NONE) even though only WiFi is implemented - see ROADMAP Q9.`n`nAcceptance: state observable via callback; mode selectable; ``isOnline()``/``getIP()``/``getLinkType()`` are the only accessors callers use."),
    @("1", "1.2 SPIKE: does APSTA work over esp_hosted on P4?", "The four P4 boards have no WiFi radio - they go through an ESP32-C6 co-processor over SDIO. Whether softAP() works concurrently with STA on that transport is unknown.`n`nGates 1.3. If APSTA does not work on P4, connectivity mode 2 (STA_PLUS_AP) cannot be honestly offered fleet-wide and the settings UI must say so per-board.`n`nAlso test: does APSTA worsen the known CYD_S3_3248 brownout resets?`n`nAcceptance: answer written into docs/PROJECT_STATUS.md."),
    @("1", "1.3 AP + captive portal", "Depends on 1.2.`n`nAcceptance: device raises an AP; DNS redirect works; credential entry page works; STA-only mode selectable; automatic AP fallback on STA failure.`n`nMust ship simultaneously: a physical recovery path (hold BOOT N seconds to force AP fallback for one boot), or STA_ONLY can permanently strand a device when the AP password changes."),
    @("1", "1.4 Connectivity settings UI + header status icon", "On-device: scan networks, pick one, enter password, view IP/RSSI/mode. Header bar shows a live WiFi status icon driven by the 1.1 state machine.`n`nAcceptance: a board with no stored credentials can be fully configured by touch alone."),
    @("1", "1.5 Flash-test WiFi on the remaining 4 boards", "4 of 8 flash-tested so far (WS_P4_4B, WS_S3_4B, WS_P4_7B confirmed; CYD_S3_3248 connects with the reset issue). Remaining: WS_P4_TOUCH_LCD_5, CYD_P4_1060P470, CYD_S3_8048W550, WS_S3_TOUCH_LCD_5B.`n`nAcceptance: every board in the PROJECT_STATUS WiFi column is confirmed or has a documented reason."),
    @("1", "1.6 MQTT transport (Fleet_MQTT)", "New library, separate from Fleet_Connectivity - it must not know whether the link is WiFi or Ethernet.`n`nAcceptance: connect with credentials from the settings layer; publish; subscribe; Last Will and Testament; exponential backoff with jitter on reconnect; status visible in the UI."),
    @("1", "1.7 Entity Registry (Fleet_Entities)", "The keystone - see ROADMAP section 4.1. One registry serves both the UI and HA discovery; they are the same object.`n`nMUST be plain C++ with zero Arduino dependencies (ROADMAP Q8), so it stays portable to ESP-IDF and unit-testable on a PC.`n`nCritical: providers never call lv_* directly. They write to the registry under a mutex and set a dirty flag; an lv_timer in the LVGL task drains it. See ROADMAP section 4.2 - this is the highest-severity trap in the project.`n`nAcceptance: registry + dirty-flag bridge; SystemProvider populates rssi/ip/uptime/heap; MqttProvider maps topics to entities both directions."),
    @("1", "1.8 Home Assistant MQTT discovery", "Depends on 1.7. Discovery payloads are GENERATED by walking the registry, never hand-written per peripheral.`n`nNaming scheme (ROADMAP Q5): device_id = fleet_<board>_<mac6>; unique_id = <device_id>_<object_id>; discovery on homeassistant/<component>/<device_id>/<object_id>/config.`n`nAcceptance: device and all entities appear correctly in HA; two boards with identical hardware do not collide."),
    @("2", "2.1 Startup reorganization", "The three-way split already proposed in FUTURE_IMPROVEMENTS: main (hardware bring-up only) / LVGL_Startup (engine plumbing) / GuiManager (actual screen content).`n`nDo this BEFORE adding UI, not after - every new panel added first makes it harder."),
    @("2", "2.2 Design system", "Colour tokens, spacing scale, type scale, Material Design Icons subset as an LVGL font, card elevation/border/radius.`n`nThe gap between 'functional' and 'gorgeous' is mostly icons and typography, not layout code. Retrofitting icons after cards exist means touching every card.`n`nAlso settle a cast helper for the LV_PART_MAIN | LV_STATE_DEFAULT deprecation warning before 8 card types each reproduce it.`n`nAcceptance: one reference page rendering every token."),
    @("2", "2.3 SPIKE: LVGL memory budget per card", "Tileview keeps every tile alive. Rough estimate: 5 pages x 15 cards x ~8 objects x ~200 bytes is ~120KB against LV_MEM_SIZE of 128KB - it will not fit.`n`nMeasure real heap per card on CYD_S3_3248 (the smallest board), not the biggest. Decide: lazy tile building vs. moving LV_MEM into PSRAM.`n`nGates 2.5."),
    @("2", "2.4 Card base class", "MUST carry preferred_span / min_span / priority from the very first version (ROADMAP Q4). Retrofitting responsive sizing after 8 card types exist means rewriting all 8.`n`nAcceptance: grid placement with spans in sub-grid units; entity binding; staleness handling; tap + long-press; compact and full variants."),
    @("2", "2.5 Page + grid engine", "LVGL native grid with LV_GRID_FR(1) fractional units. Sub-grid units per ROADMAP Q3b: a page authored as 3x3 cells allocates 6x6 units, so a quarter-page card is exactly 3x3 units.`n`nIncludes the placement validator: overlaps and out-of-bounds detected at compile time for compiled sheets, parse time for JSON.`n`nAcceptance: same page definition renders correctly on 3 different resolutions."),
    @("2", "2.6 Tileview navigation", "Swipe L/R between pages, U/D to menus.`n`nThe gesture-conflict problem is the single most common failure in LVGL dashboards: a swipe starting on a slider or scrollable card gets swallowed. Cards must be non-scrollable and interactive sub-widgets belong in context sheets, not dashboard cards.`n`nAcceptance: swiping works from anywhere on a card; page indicator dots."),
    @("2", "2.7 First four card types", "sensor, binary_sensor, switch, button - each with compact and full variants, bound to real HA entities over MQTT.`n`nAlso the local-only demo cards (system info, device temp, memory, uptime, RSSI) that make a fresh flash useful with connectivity mode 0 / no network at all."),
    @("2", "2.8 Header bar v2", "Configurable slot list replacing the fixed title + one icon. Clock, WiFi/MQTT status, optional sensor slots, repositionable.`n`nPerformance: update the clock label only when the displayed minute changes. Updating one card must invalidate one card, not the screen."),
    @("3", "3.1 Build sheet schema v1", "Nested struct model: Dashboard > Pages > Grid > Cards > Entities. Plain C++, Arduino-free.`n`nMust include schema_version from day one - once a sheet lives on LittleFS, a firmware update can arrive that no longer understands it.`n`nOpen question to settle first: the per-sheet 'authority' field (LOCKED vs DEFAULT) - see ROADMAP Q2.`n`nAcceptance: documented in docs/design/build-sheet.md."),
    @("3", "3.2 Compile-time build sheet loader", "Struct literals in a header, BSP style. Two real dashboards for the actual HA setup.`n`nAcceptance: a device boots into a real dashboard with no runtime config present."),
    @("3", "3.3 Runtime JSON build sheet loader", "ArduinoJson streaming from LittleFS, with the compiled sheet as fallback. Graceful parse-failure handling - refuse and fall back, never crash.`n`nAcceptance: editing the JSON changes the layout on next boot; a corrupt file falls back cleanly."),
    @("4", "4.1 Standard settings pages", "Brightness, volume, screen dimming/timeout, device info, network info.`n`nDebounce NVS writes by a few seconds - a brightness slider can otherwise generate hundreds of writes a minute against finite erase cycles."),
    @("4", "4.2 Live layout settings", "Grid rows/cols, spacing, card sizes, page reordering - applied live.`n`nBlocked on the ROADMAP Q2 'authority' decision: if compile-time sheets always win, this is pointless on exactly the devices that have one."),
    @("4", "4.3 Navbar", "Configurable edge strip navigating to pages and fullscreen cards. Position and contents user-configurable."),
    @("4", "4.4 Context sheets (repurposed panels)", "The existing accordion/expanding-panel mechanism in UIToolkit becomes a bottom sheet that opens on long-press of a card, populated from that card's entity.`n`nThis is what keeps the animation you like while removing it from the primary navigation path - and it is what makes 2.6's gesture handling tractable."),
    @("4", "4.5 Web config page", "Synchronous WebServer pumped from loop(), NOT ESPAsyncWebServer - the async server runs handlers on its own task and walks straight into the LVGL threading trap.`n`nSame server instance serves the captive portal from 1.3.`n`nAcceptance: same settings surface as on-device."),
    @("5", "5.1 Decide the partition layout", "MUST be finalised before the first OTA-capable release ships. Changing the partition table later requires a full serial reflash of every board - it cannot be done over OTA.`n`ndefault_16MB.csv has two app slots, but LittleFS space for build sheets and web assets may need a custom CSV."),
    @("5", "5.2 OTA with automatic rollback", "An OTA that bricks a wall-mounted panel means getting a screwdriver.`n`nUse esp_ota_mark_app_valid_cancel_rollback() gated behind 'did the link come up AND did LVGL render a frame', so a bad image auto-reverts on next boot.`n`nAcceptance: push-to-device and on-device 'check for update'; a deliberately broken image rolls back."),
    @("5", "5.3 Fleet build script", "One command builds all 8 environments and stages images with version metadata."),
    @("6", "6.1 Card library expansion", "light, climate, weather, media, camera - each with compact and full variants."),
    @("6", "6.2 WS_S3_4B onboard hardware libraries", "AXP2101 power management, PCF85063 RTC, QMI8658 IMU, controllable PWRKEY. Reference Waveshare's own repo.`n`nEach becomes an Entity Registry provider, so it lands on dashboards and in HA automatically."),
    @("6", "6.3 RS485 / Modbus support", "WS_S3_5B with the Modbus-RTU-Relay-B board. Note WS_P4_4B exposes UART1 on its second header bank but as raw TTL, not RS485 - needs an external transceiver."),
    @("6", "6.4 Weather station fullscreen template", "Multi-cell or full-page card pulling from an HA weather entity, local sensors, or MQTT."),
    @("6", "6.5 Alarm clock page", "The nightstand-clock look referenced in the original brief.")
)

$openTitles = (& $gh issue list --state all --limit 300 --json title -q ".[].title")
foreach ($i in $issues) {
    $phase = $i[0]; $title = $i[1]; $body = $i[2]
    if ($openTitles -contains $title) { Write-Output "  = $title"; continue }
    $ms = $milestones | Where-Object { $_ -like "Phase $phase*" }
    $args = @("issue","create","--title",$title,"--body",$body,"--label","phase-$phase","--milestone",$ms)
    if ($title -match "SPIKE") { $args += @("--label","spike") }
    & $gh @args | Out-Null
    Write-Output "  + $title"
}

Write-Output ""
Write-Output "Done. Issues: gh issue list"
Write-Output "Project board (needs 'project' scope): gh auth refresh -s project"
