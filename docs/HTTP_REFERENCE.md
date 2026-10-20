# HTTP reference - every address the boards answer

Written 2026-10-13 from the code on `refactor/ui-foundation` (2.12), every parameter checked
against its handler. When a route changes, change this file in the same commit.

## How the server works

- **One server per board**: ESP-IDF's `esp_http_server` on **port 80**, owned by `SystemCore`
  (`src/HttpServer.cpp`). It starts the first time Wi-Fi is up, and only if at least one route was
  registered. Room for 8 routes; the most any board has today is 7 (WS_P4_5: six GET and the
  settings restore's POST).
- **GET only, except the settings restore (`POST /settings`, A17); no authentication, plain
  HTTP.** Anyone on the LAN who can reach a board can use every route it has. That is why the
  riskier ones are build flags.
- **Address**: `http://<hostname>/<route>?<key>=<value>&...`, by the DHCP hostname (it resolves
  through the router's DNS) or the board's IP.
- **At most 3 connections at once**, and the least recently used is dropped, so a browser's idle
  keep-alive cannot lock a script out.
- **A handler never touches the screen itself.** It runs on the server's own task, asks the UI
  thread (`loop()`, where LVGL runs) to do the work, and waits. So a **frozen UI shows as a 503
  "did not respond"** while the network still answers - which is itself a useful test.
- **Busy means 503.** `/screenshot`, `/bench` and `/popup` serve one request at a time; a second
  one gets `503 Service Unavailable` (with `Retry-After` on the first two).

| Board | Hostname | Routes |
|---|---|---|
| WS_P4_5 | `fleet-ws-p4-5` | settings, screenshot, bench, panel, + popup and hang (owner's local lines) |
| WS_P4_4B | `fleet-ws-p4-4b` | settings, screenshot, bench, panel, + popup (local) |
| CYD_P4_1060 | `fleet-cyd-p4-1060` | settings, screenshot, bench, panel, + popup (local) |
| WS_P4_7B | `fleet-ws-p4-7b` | settings, screenshot, bench, panel |
| CYD_P4_4880 | `fleet-cyd-p4-4880` | settings, screenshot, bench, panel |
| WS_S3_4B | `fleet-ws-s3-4b` | settings, screenshot, bench |
| CYD_S3_3248 | `fleet-cyd-s3-3248` | settings, screenshot, bench |
| WS_S3_5B, CYD_S3_8048 | `fleet-ws-s3-5b`, `fleet-cyd-s3-8048` (by the same pattern; not plugged in) | settings, screenshot, bench |

The names have no MAC suffix (`ConnectivityDefaults.h`, `APPEND_MAC_SUFFIX`); two boards of one
model would collide.

## At a glance

| Route | Exists when | Boards | What it is for | Can it change anything? |
|---|---|---|---|---|
| `/settings` | always | all nine | read the owner's saved settings; set the ones without a screen yet | **yes - the settings file** |
| `/screenshot` | `-D ENABLE_SCREENSHOT` (committed, in `[S3-options]` and `[P4-options]`) | all nine | a PNG of what is on the glass | no |
| `/bench` | `-D ENABLE_BENCH` (committed, same sections) | all nine | time where a frame goes | no; the UI freezes while it measures |
| `/panel` | `DISPLAY_ESPLCD` and `HAS_MIPI_PANEL` (committed; no flag of its own) | the five P4s | DSI panel health; resend its init; flash-write tests | **yes - see the warning** |
| `/popup` | `-D DEBUG_POPUP` (the owner's local lines, never committed) | P4_5, 4B, 1060 | drive the card window from a PC | **yes - settings, and real lights** |
| `/hang` | `-D DEBUG_HANG` (local line) | P4_5 | where the UI thread stopped, after a freeze | no; `test=1` freezes the UI 4 s |

`platformio.ini` marks `ENABLE_SCREENSHOT` and `ENABLE_BENCH` "remove for a release build": no
authentication. `/settings` and `/panel` have no flag; a release build serves them as they are.

What each kind of effect means below:

- **Reads** - nothing on the board changes.
- **UI** - the screen changes (a window opens, a page switches) and nothing is kept; a reboot, or
  closing the window, undoes it.
- **RAM** - a setting that lasts until the next reboot.
- **Saved** - written to `/cfg/settings.json` on LittleFS by the settings task, and kept over a
  reboot. Undo it the same way, or with the matching Reset.
- **Device** - sends a command to a real light, switch or scene through Home Assistant (or MQTT
  for the virtual lamps). On the owner's lights, only Office ones are safe to command
  (DECISIONS W9).

---

## `/settings` - the saved settings

**Always present, every board.** Registered by `Settings::begin()` at boot, even when LittleFS will
not mount (the store then lives in RAM and says so in `?stats=1`). The design: `Settings.h` and
`docs/design/card-sheet.md` §17.

### Reading

| Address | Reply | Effect |
|---|---|---|
| `/settings` | the whole settings document as JSON (`application/json`) - schemes, entities, cards, groups, added cards, imports, system | Reads |
| `/settings?stats=1` | text: the file and whether it is mounted or read-only, saves / failures / a change pending, the last save's time, duration and size, the boot note (what the mount and the file were at boot - a failed mount that left the partition untouched says so here, A17), the files in `/cfg`, **the last ten changes with their time and task** (`loopTask` = the card window, `httpd` = a browser), and any saved entries nobody claims | Reads |

### Setting (each is **Saved**)

Each answers one line of text. A card's change shows **the next time its page is built** - swipe
away and back. A card's id is shown in its window: SETTINGS > Entity properties > Card ID
(`<area>_<label>_<yymmdd>_<hhmm>`, K33).

| Address | What it sets | Rules |
|---|---|---|
| `?card=<id>&name=<text>` | the card's own name (K37) | up to 39 characters of printable ASCII, else refused; empty removes it |
| `?card=<id>&label=<choice>` | Entity label, as SETTINGS' row | the choice's saved name; `default` or empty removes it (`inherit` is kept: a named card's default is Custom, K49) |
| `?card=<id>&tap=<choice>` | Tap action | saved name of the choice; `default`/empty removes |
| `?card=<id>&tap_scene=<scene id>` | the scene a tap loads | the scene's HA id |
| `?card=<id>&group=<g>` | the card label's group (K44): `ha_area`, `own`, `none` or a group id | |
| `?card=<id>&label_text=<text>` | the card label's own words, with `group=own` | |
| `?card=<group card>&present=card\|cluster` | how a group card made on the device is shown (2.11, K90): its members as a cluster, or one card for them | `default`/empty: the dashboard's |
| `?card=<group card>&slots=<id>,<id>` | the members in a cluster's header, highest priority first (K104); a member there is not also a tile unless made visible | member card ids, comma-separated, up to 4; empty: the dashboard's |
| `?card=<group card>&cl_style=swatch\|bar` | that cluster's header style (K102) | empty: the page's |
| `?card=<group card>&cl_icons=off` | the header's values without their icons (K104's "Show sensor icons") | empty: icons shown |
| `?card=<member>&hidden=yes\|no` | a member's tile hidden or shown in its cluster (K104's Visible) | empty: the default - a tile unless it is in the header |
| `?page=<slug>&cl_style=swatch\|bar` | the clusters' header style on that page (K102) | empty: the dashboard's (Swatch) |
| `?arranger=<weight>&v=<number>` | one of the arranger's weights or sizes (2.11 round 2, K105): `hole`, `comfy`, `loose`, `long`, `order`, `away` (the playground's weights), `maxcell`, `mincell`, `tilew`, `tileh`, `hdr` (its sizes, mm); `off` = 1 lets every page flow as before 2.11, to compare. Saved under `system.arranger`; each page is arranged again when next built, and its answer kept in `pages.<slug>.layout` | an unknown name is refused; empty `v`: the playground's default |
| `?group=new&name=<Name>` | a new custom group; answers its id | 1-39 characters of ASCII |
| `?group=<id>&color=RRGGBB` | a group's colour | six hex digits; empty: back to its colour by name |
| `?group=<id>&name=<Name>` | rename a group (every card in it; an HA area keeps its link) | empty: its own name again |
| `?group=<id>&delete=1` | delete a group | its cards show their dashboard area again |
| `?prune=cards` / `?prune=entities` | remove saved entries no card or entity claims (K35) | right after boot a group's members are not known yet, so a paused member can look unclaimed - prune later |
| `?window_close=60\|30\|10\|off` | how long a window (the card window, About) waits untouched before it closes itself (K79); `off`: never, and no countdown bar | anything else is refused; `60` is the default and is stored as nothing. **From the next window** |
| `?window_bar=on\|off` | the countdown bar on a window's bottom edge, and on the Remove dialog over it | `on` is the default, stored as nothing. May go with `window_close` in one address |

The values a choice takes (`label`, `tap`) are the names in `CardTypes.h`'s name tables; the
handler does not check them, and a name the firmware does not know reads as "not set". The whole
query is at most ~159 characters. Spaces go as `%20` or `+`.

```
curl "http://fleet-ws-p4-5/settings"
curl "http://fleet-ws-p4-5/settings?stats=1"
curl "http://fleet-ws-p4-5/settings?card=office_overhead_261008_0310&name=Ceiling"      # Saved
curl "http://fleet-ws-p4-5/settings?card=office_overhead_261008_0310&name="             # Saved: removes it
curl "http://fleet-ws-p4-5/settings?group=new&name=Climate"                             # Saved
curl "http://fleet-ws-p4-5/settings?window_close=30&window_bar=off"                     # Saved: next window
curl "http://fleet-ws-p4-5/settings?window_close=60&window_bar=on"                      # Saved: the defaults again
```

### Restoring a saved copy (`POST /settings`, A17)

| Address | Body | Reply | Effect |
|---|---|---|---|
| `POST /settings` | a whole settings file, as `GET /settings` gave it (at most 64 KB) | one line: `restored: <bytes>, <n> card(s), written in <ms> ms - ...`, or `refused: <why> - nothing changed` (400: not JSON, not an object, a schema this firmware does not know; 409: this boot is read-only) | **Saved** - replaces the whole file. Pages show it when next built; everything at the next boot |
| `POST /settings?reboot=1` | the same | the same, ending `- restarting` | **Saved**, then the board restarts half a second after answering |

The body is checked on the HTTP task; the settings task writes it (a flash write from the HTTP
task, whose stack is in PSRAM, reboots the board - LESSONS), and the handler waits for that write
before it answers. A board whose mount failed refuses: its boot is read-only, and the file on its
flash is never written over.

The scripts do it for you - copies live in `bench/settings/<board>/<yyyymmdd-hhmm>.json`
(gitignored), and `flash_release.ps1` makes one for each board before it writes it:

```
powershell -ExecutionPolicy Bypass -File scripts/release/save_settings.ps1 fleet-ws-p4-5 WS_P4_TOUCH_LCD_5
powershell -ExecutionPolicy Bypass -File scripts/release/restore_settings.ps1 fleet-ws-p4-5 bench\settings\WS_P4_TOUCH_LCD_5\20261015-0542.json
curl --data-binary @settings.json -H "Content-Type: application/json" "http://fleet-ws-p4-5/settings?reboot=1"
```

---

## `/screenshot` - what is on the glass

**`-D ENABLE_SCREENSHOT`, all nine boards** (both shared option sections). `src/UI/Screenshot.cpp`,
issue #58. **Reads**; the UI pauses while the picture is rendered (milliseconds to a second; the
`X-Render-Ms` header says how long), and runs slower while the PNG is encoded.

| Address | Reply |
|---|---|
| `/screenshot` | a PNG, rendered by LVGL from its object tree: the active screen, then the top layer (toasts) and system layer (the FPS/CPU overlay) over it - upright on every board, whatever the panel's rotation |
| `/screenshot?fb=1` | (esp_lcd boards: the five P4s and WS_S3_4B; ignored elsewhere) the frame buffer the panel is actually showing, copied as it is: physical orientation (the P4_5's comes out portrait), no overlays re-rendered. The one picture that does not trust LVGL |
| `?z=1..9` | zlib effort (default 5); larger is smaller and slower |
| `?f=-1..4` | PNG row filter (default 0; -1 tries all five per row) |

Headers: `Content-Disposition: inline; filename="<hostname>.png"`, `Cache-Control: no-store`.
`503` with `Retry-After: 2` while another screenshot is running; `503 "did not respond within 5
s"` when the UI thread is stuck.

```
curl -o p4_5.png "http://fleet-ws-p4-5/screenshot"
curl -o glass.png "http://fleet-ws-p4-5/screenshot?fb=1"
python scripts/screenshot.py                     # every board, into screenshots/
```

---

## `/bench` - where a frame's time goes

**`-D ENABLE_BENCH`, all nine boards.** `src/UI/Bench.cpp`, `Bench.h` (2.9, #67). **The UI freezes
for the measurement** (n x the frame time; `anim` and `page` show the screen moving instead).
Nothing is saved. If `page` or `deck` differ from what is showing, it switches, lets the board
settle, measures and switches back - unless `keep=1`.

| Key | Values | Meaning |
|---|---|---|
| `what` | `full` (default) | redraw the whole screen (a page change, the worst case) |
| | `card` | redraw one card's area (a value changing, the common case) |
| | `anim` | open one deck panel while the other closes, n times, recording every frame LVGL draws |
| | `page` | change page n times as a swipe does, timing each rebuild and recording every frame |
| | `copy` | (esp_lcd) how fast the PPA, the DMA2D copier and the CPU move frame-buffer memory |
| | `verify` | (esp_lcd + screenshot) n checks, each comparing LVGL's render with the panel's frame buffer in one instant; any `bad` pixel is a flush error |
| `n` | 1-100 (default 20); `anim`/`page` 1-8 (default 4); `copy` 1-10 (default 3) | frames, episodes or repetitions |
| `act` | `none`, `anim`, `page` (`verify` only) | what happens between checks |
| `gap` | 20-3000 ms (`verify` only, default 250) | ordinary frames after each action |
| `page` | swipe-order index | the page to measure on (default: the one showing) |
| `deck` | `0`/`1` | deck hidden/shown (default: as it is) |
| `keep` | `1` | stay on that page and deck afterwards |
| `tasks` | `1` | also CPU time per FreeRTOS task and each core's idle time (not with `anim`, `page`, `verify`) |

Reply: JSON (`Cache-Control: no-store`) - per-frame times in microseconds (render, copy, present,
wait: avg/min/max), chunks per frame, an `fps_ceiling`, the board, firmware, panel and bus, uptime
and reset reason; the field meanings are in `Bench.cpp` above `buildJson()`. A bad query is a
`400` naming the problem; a run in progress a `503` with `Retry-After: 5`.

```
curl "http://fleet-ws-p4-5/bench?n=20&what=full"
curl "http://fleet-ws-p4-5/bench?what=card&page=1&deck=1"
curl "http://fleet-ws-p4-5/bench?what=verify&n=10&act=page"
python scripts/bench.py fleet-ws-p4-5            # the standard matrix, with a screenshot of each
python scripts/soak.py --minutes 60 fleet-ws-p4-5
```

---

## `/panel` - the DSI panel's health

**The five P4s** (`DISPLAY_ESPLCD` and `HAS_MIPI_PANEL`; `SystemCore` registers it there, with no
flag of its own). `src/PanelDebug.cpp`, `PanelDebug.h` (2.10c round 9, 2.10d).

| Address | Does | Effect |
|---|---|---|
| `/panel` | the panel chip and driver; **DSI underruns since boot** (the bridge's feed from PSRAM running dry - a flash on the glass) and the last few times they happened; help lines | Reads. It never reads the panel's registers: a DSI read while the panel streams stops the picture (2.10c) |
| `/panel?resend=1` | sends the BSP's panel init sequence again, without a reset, and says how many commands went | **Changes the panel controller's registers** back to the BSP's values; the picture may blink. For a washed-out picture (the 4B, twice) |
| `/panel?flash=N&nvs=1` | N (up to 50) NVS writes of 512 B into a scratch namespace `fleet_test`, 150 ms apart, timing each and counting underruns around it; the namespace is erased afterwards | Writes and erases its own scratch NVS only |
| `/panel?flash=N` (no `nvs=1`) | **refused**, with the reason: only the NVS mode is kept (#98) | Nothing |

> **The erase mode is gone (#98, the owner's choice of fix, 2026-10-14).** `/panel?flash=N` without
> `nvs=1` used to erase and write sectors at the start of the `spiffs` partition - since 2.10d the
> LittleFS partition that holds `/cfg/settings.json`. Erasing its first blocks destroys the file
> system's superblock, and the board mounts with "format if the mount fails", so the next boot
> formatted the partition and lost every saved setting. The test had already answered its question
> (DECISIONS A14), so it was removed; a request for it now answers with a refusal and changes
> nothing. 2.10d's test sheet ran `flash=20` and `flash=8&kb=256` on the P4_5
> (`archive/TEST_2.10d.md`, F2-F3): if that board lost its settings afterwards, this was why. Builds
> before `feat/ui-followups` still have it: on them, never run it without `&nvs=1`.

```
curl "http://fleet-ws-p4-4b/panel"
curl "http://fleet-ws-p4-4b/panel?resend=1"          # changes the panel's registers
curl "http://fleet-ws-p4-5/panel?flash=20&nvs=1"     # scratch NVS only
```

---

## `/popup` - drive the card window from a PC

**`-D DEBUG_POPUP` only** - the owner's local lines on WS_P4_5, WS_P4_4B and CYD_P4_1060, never
committed. `src/Cards/Popup/PopupDebug.cpp`. What `scripts/ui_golden.py` drives. **One action per
request**: when a query has several keys, the first in this order acts and the rest are ignored -
`rows`, `close`, `about`, `drawer`, `tree`, `mem`/`sparks`, `addcard`/`removecard`, `pause`/`member`,
`scheme`, `show`, `look`, `scene`, `power`, `view`, `set`, `ctl`, `deck`, the History keys
(`span`, `more`, `row`, `dur`, `chips`, then `x`, `hist`), `clabel`, `groups`, `tap`, then `card`.
The reply is text. `503 "Busy"` while another `/popup` runs; `503 "did not pick it up within 3 s"`
when the UI thread is stuck.

`N` for a card is its number in the plain `/popup` list: the cards on the page showing, in
order.

### Looking

| Address | Reply | Effect |
|---|---|---|
| `/popup` | the cards on this page, numbered, with area and entity count; LVGL's pool; window open or not; the scheme; the selector look | Reads |
| `/popup?mem=1` | LVGL's pool to the byte: used, free, biggest free block, fragmentation | Reads |
| `/popup?tree=1` | the open window's LVGL objects, one line each: class, place, size, flags, events, text and style as LVGL resolves it (2.12 P0) | Reads |
| `/popup?tree=page` | the same for the whole screen | Reads |
| `/popup?groups=1` | every card-label group (id, name, colour, HA area or local) and each card's group | Reads |
| `/popup?hist=1` | with History showing: what HaHistory holds - state, points, events, the last answer's size and time | Reads |

### The window (each **UI**; nothing kept)

| Address | As if... |
|---|---|
| `/popup?card=N` | a long press on card N: opens its window; replies with the build time, window size and LVGL's pool |
| `/popup?close=1` | closing it; replies with the pool afterwards. **Anything changed in the window is saved now**, as on the glass |
| `/popup?view=N` | the corner icons: `0` the controls (a member's, inside one), `1` History, `2` Members, `4` Scenes, `5` Properties, `6` Card UI (2.8 round 3, as SETTINGS' last row), `7` a cluster's Header slots (2.11), `8` Visibility (2.11, K96: the card's options by where it is), `9` its Add to a cluster (K91, K100), `10` a cluster's Add cards (K104), `3` only lists. Also lists the entities' levels, an HA group's members and why recent commands failed |
| `/popup?deck=1` / `deck=0` / `deck=2` | SETTINGS opened / folded; `2` the left panel (CHART or SCENES) opened |
| `/popup?ctl=N` | a light's control: `0` brightness, `1` color temperature, `2` color; lists where the stage's children landed |
| `/popup?member=N` | a tap on row N of Members (opens Members first) |
| `/popup?span=N` | History's span buttons: chart `0` 24 h, `1` 12 h, `2` 6 h, `3` 1 h; activity `0` 24 h, `1` 7 days. Asks Home Assistant for that history |
| `/popup?more=1` | "Show more" (asks HA for more) |
| `/popup?row=N` | a tap on activity row N from the top (its details) |
| `/popup?x=N` | a finger on the chart, N px from its left edge (the scrub line) |
| `/popup?dur=0\|1` | activity's Show duration off/on (not kept) |
| `/popup?chips=0\|1` | the span buttons off/on (not kept) |
| `/popup?about=1` / `about=0` | opens / closes "About this board" (2.12 P4) - the board, screen, chip, firmware, uptime, memory, LVGL's pool, each second. Every build also opens it from the system panel's About button (#97) |
| `/popup?drawer=1` / `drawer=0` | a tap on the header's status slot: the system panel opens / closes (#97, so the harness can see it); "already open/closed" when it was |
| `/popup?slot=<kind>` / `slot=-1` | 2.8: a tap on the header's slot of that kind - `clock`, `sensor`, `page`, `alarm` (round 3: the 12-hour notice), `status` (round 2; round 1 took a number): its panel opens - the status slot's is the system panel; `-1` closes any. A slot not on the bar (the notice with no alarm within its hours) says so. Round 3: `slot=deck0` / `deck1` taps a deck panel's header (AUDIO / DISPLAY), as `/bench?what=anim` does - under UI Tweaks, where the deck rises. UI |
| `/popup?alarm=HHMM` / `alarm=-1` | 2.8 round 2: a **pretend** next alarm at that time (24 h; today's if still to come, else tomorrow's), so the bell, the notice and their panels can be seen before K84 builds alarms - **RAM only, never saved, nothing rings** (`Alarms.h`); `-1` clears it. UI |
| `/popup?alarmin=N` | 2.8 round 3: a pretend next alarm **N minutes from now** (`alarmin=120`: the bell by the clock and, within the 12 hours, the notice among the status icons). As `alarm=`, RAM only. UI |
| `/popup?tweaks=1` / `tweaks=0` | 2.8 round 3: UI Tweaks opened / closed, as the system panel's button; the deck rises under it. UI |
| `/popup?sensorerr=1\|0` | 2.8 round 3 (T10): the header's sensor shown failed - `--°` and the "!" - whatever its entity says; `0` as it is again. **RAM only**. UI |
| `/popup?cardstate=N` | 2.8 round 4: the cards' active state, as UI Tweaks' Card state - `0` Fill, `1` Icon glow, `2` Bracket, `3` Corner cut (round 5) - **not saved**; `-1` the owner's again. UI |
| `/popup?active=RRGGBB` | 2.8 round 6: the active colour ("on" - a fill, the frames, lit glass) in place of the scheme's, for judging colours on glass (the screenshots' colours are not the panel's) - **not saved**, until the scheme changes; `active=0` the scheme's own again. A leading `#` is fine if URL-encoded (`%23`). UI |
| `/popup?glyphs=0\|1\|2\|3` | 2.8: the header's glyphs bare, in a rounded square, in a circle - **not saved** (a capture leaves the owner's choice); `3` the saved style again. UI |
| `/popup?pagehdr=0\|1` | 2.8: this page's header hidden / shown - **Saved**, as the page slot's panel and Hide Bar save it (`pages.<slug>.header`; shown is the default and is stored as nothing) |
| `/popup?removecard=3` | the Hide dialog (K96; Remove card's until 2.11) over any card's window, to look at - never tap its Hide, which now hides any card; `close=1` takes it down |
| `/popup?kbd=1\|2` | 2.11 (K95): the keyboard over the open window - `1` as a cluster's Rename, or a card's Entity label Custom (its name); `2` as Card label's Custom (the card label's words). Then `kbtype=<words>` types them (`+` a space), `kbmode=0\|1\|2` shows the lower / upper / symbol keys, `kbpress=3` empties the field, `kbpress=2` Cancel. Replies with LVGL's pool. UI (`kbpress=1` saves: below) |

### Settings for the next windows (each **RAM**)

| Address | Sets |
|---|---|
| `/popup?sparks=0\|1\|2` | the edge sparks from the next window: now and then / once / off |
| `/popup?rows=N` | N greyed test rows in SETTINGS from the next window (to try dragging a tall panel); `rows=0` undoes it |
| `/popup?scheme=0\|1\|2` | the colour scheme: Fleet, Midnight, Linen (no window may be open). Since 2.8 round 3 through the dashboard - every page, the header and the ground follow, as the system panel's selector does. **Not saved**: a reboot comes back in the owner's scheme (`system.ui.scheme`, else Midnight) |

### Changes that are **Saved**

| Address | As if... | What is written |
|---|---|---|
| `/popup?pause=1` / `pause=0` | the deck's Pause chip: pause / resume whatever the window shows (an HA group with its members) | the entities' `paused` flags, a second later |
| `/popup?show=0\|1\|2` | SETTINGS' Scenes row: visible scenes / all / none, on the open card | the card's `scenes`, when the window closes |
| `/popup?look=0..4` | the system panel's Selector look, for the scheme showing: `0` Black - Square, `1` Black - Round, `2` Silver - Square, `3` Silver - Round, `4` the scheme's own | the scheme's `selector` (at once with no window open; else when it closes) |
| `/popup?clabel=N` | choosing line N of the open card's Card label row (K46); replies with the row | the card's `group` (and a solo group, for its own words) |
| `/popup?addcard=1` | "Add card to dashboard" in a member's own view (K17) | a new card entry after its group card |
| `/popup?removecard=2` | Visibility's Hide from this page: its dialog, on any card (K96; K66's remove, every card since 2.11) | hidden only if Hide is tapped |
| `/popup?removecard=1` | Hide from this page, confirmed - no dialog; on a mini tile, Hide from the cluster | the card's `removed` (the page's pool, its settings kept); a tile's `hidden` = `yes` |
| `/popup?restore=-2` | (round 6) the page slot's panel's "View hidden cards (N)" (`slot=page` first): the hidden cards' sheet opens | nothing |
| `/popup?restore=N` | Restore beside the Nth card in the hidden cards' sheet (`restore=-2` first) | the card's `removed`, removed - back on its page |
| `/popup?removecard=4` / `removecard=5` | on a copy made in a cluster (K91): `4` its "Remove this copy?" dialog; `5` its Remove, no dialog | `5`: the copy's whole entry, deleted (`Settings::dropCard()`) |
| `/popup?addcards=MASK&mv=0\|1` | a cluster's Add cards (K104, K91) on its open window: the cards whose bits are set ticked (bit 0 the list's first - `view=10` shows the list), Copy or Move, and Add - the window closes | an entry with `in` for each, as `addto=`; Move also each card's `removed` |
| `/popup?addto=N&mv=0\|1` | Add to a cluster (K91, K100) on the open card: the Nth cluster on the page (screen order), Copy (`mv=0`) or Move (`mv=1`), and Add - the window closes | a new card entry with `in` (the cluster's id), `entity`, `area`, `group`, `name`; Move also the card's `removed` |
| `/popup?join=1` | Add card to dashboard (K92 rule 9) in a member's own view or a tile's window: "which group?" opens - with This card's Copy / Move row for a cluster's tile (K112) | nothing yet |
| `/popup?join=2+N` (`&mv=1`) | its Add card to dashboard with group N chosen (0 the group card's, 1 the area's, then the others); `mv=1` Move, a tile's only - the window closes | a new card entry with `page`, `after`, `entity`, `from` (the tile); Move also the tile's `hidden` = `moved` |
| `/popup?removecard=6` / `7` / `8` | (K112) on a card made on the glass: `6` its Remove from the page, no dialog; `7` that dialog; on a card moved out of a cluster, `8` its Move back to the cluster - each closes the window | `6`: the card's entry deleted; `8`: the entry deleted and the tile's `hidden` cleared |
| `/popup?kbpress=1` | the keyboard's Save (2.11, K95) | what it was opened for: a cluster's `label_text` (none: its group's name), a card's `name`, or its card label's `label_text` - when the window closes |

### **Device** - these command real lights and scenes

| Address | As if... |
|---|---|
| `/popup?power=1` | a tap on Power (or a switch's toggle): every entity the window shows, on or off |
| `/popup?set=V` | a tap on the light's slider at V: percent, kelvin or hue, for the control showing |
| `/popup?scene=N` | a tap on offered scene N (lists the light's scenes, hidden ones marked) - loads it in HA |
| `/popup?tap=N` | a **tap** on card N (no window open): whatever its Tap action says - toggle, open the window on a view, load a scene, or nothing |

```
curl "http://fleet-ws-p4-5/popup"                    # the numbered cards
curl "http://fleet-ws-p4-5/popup?card=8"             # open card 8's window
curl "http://fleet-ws-p4-5/popup?view=1"             # its History
curl "http://fleet-ws-p4-5/popup?tree=1" > desk.txt  # its object tree
curl "http://fleet-ws-p4-5/popup?close=1"
python scripts/ui_golden.py capture fleet-ws-p4-5 --board WS_P4_5 --label mine
```

`include/Cards/CardPopup.h` has an older list in its comments: it still names `extra=`, which the
handler no longer reads, and lacks `look`, `show`, `scheme`, `tap`, `groups`, `clabel`, `dur`,
`addcard` and `about`. This file is the complete one.

---

## `/hang` - where the UI thread stopped

**`-D DEBUG_HANG` only** - the owner's local line on WS_P4_5. `src/HangWatch.cpp`, `HangWatch.h`
(2.10d). A watcher task on the other core notices `loop()` has not run for 3 s, samples where it
is three times (program counter, return address, the code addresses on its stack), and keeps the
first report until reboot. One report per freeze; the board is left frozen, as found.

| Address | Reply | Effect |
|---|---|---|
| `/hang` | "no hang seen since boot", or the report: when, how long, the task's state, three samples, and the `addr2line` line to decode them against that build's `firmware.elf` | Reads |
| `/hang?test=1` | "loop() will spin for 4 s" - a deliberate stall to try the watcher; read `/hang` after | **The UI freezes for 4 s** |

```
curl "http://fleet-ws-p4-5/hang"
```

---

## Not here

Nothing on the boards listens for anything but GET on port 80: no POST, no websocket server, no
OTA upload. The boards are clients of Home Assistant (websocket, REST) and MQTT; those are their
outgoing connections, not addresses to call. The Phase 4 web UI (4.5) and the captive portal will
add routes to this same server.
