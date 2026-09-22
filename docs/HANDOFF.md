# Handoff — 2026-09-21

**If you are the owner returning after time away, read `docs/REVIEW_2026-09-20.md` FIRST.** It is
the same work written for someone who was not here, with a test plan and pass/fail criteria. This
file assumes you were.

**Start here.** `CLAUDE.md` is the stable how-it-works. This is where we are, what will bite you,
and what to do next. Kept lean on purpose: anything that is "why we did X and not Y" now lives in
`docs/LESSONS.md`, and anything that is a design lives in `docs/design/`.

**A warning about this file.** When it paraphrases a spec, the paraphrase becomes the spec for
whoever reads it first. Say *which* document a summary is compressing, and treat vocabulary that
does not appear in the source as suspect.

---

## Where the project is

Phases 0, 1 and 2.1–2.5 are merged and tagged `v0.2.5`. **`fix/49-link-liveness` merged to `main`
on 2026-09-19** (`7d723be`, 23 commits, `--no-ff` per ROADMAP §3.2's exception — the intermediate
commits are the diagnostic record, and for #49 that record *is* the finding). It carried #49's
detection half, #50, #51, the first cut of 2.6's swipe navigation, and a lot of small corrections
found on glass.

**No new tag.** `C` tracks the roadmap phase and 2.6 is not finished; the build counter `D` moves
on its own. Boards report `v0.2.5.x`.

**`feat/43-ha-websocket` is the live branch, 10 commits, pushed, NOT merged.** #43's inbound half
is code-complete and verified on hardware; #56 and #57 rode along because the HA path made both
concrete. The owner has not signed it off, and that is the gate — not the build.

Four boards attached: `CYD_S3_3248` (COM10), `WS_P4_5` (COM15), `WS_S3_4B` (COM8),
`WS_P4_4B` (COM7). **All four now carry the #43 firmware and the real HA dashboard**
(`-D USE_HA_DASHBOARD`, set on all five screen environments as of 2026-09-21). `WS_P4_7B` has the
flag but is not plugged into the PC.

**The two S3 boards work. The two P4 boards do not** - they render a card or two and lose WiFi
within seconds. That is #243, not a regression: see the headline below.

### Read in this order

1. `CLAUDE.md` — the HAL/BSP, the startup split, the token rules, **and the file-editing rule at
   the top, which is not optional.**
2. **`docs/LESSONS.md`** — read before debugging anything.
3. `docs/design/dashboard.md` — the page spec, the grid, the two knobs, the unit policy.
4. `docs/design/card-layout.md` — **before moving anything inside a card.**
5. `docs/design/ha-websocket.md` — what HA's API actually gives us, measured. Read before #43.
6. `docs/design/cards.md` — the card spec. Its "Implementation notes" first.
7. `docs/design/tokens.md`, `docs/design/startup.md` — the design system, and boot order.
8. `docs/ROADMAP.md` §7 — the milestone list.
9. `docs/REVIEW_2026-09-20.md` — the 2026-09-19/20 work in review form: what changed, what it
   took, what is unproven, and a test plan. Supersedes nothing; it is a narrative of the same
   commits.

---

## THE HEADLINE: #49 IS SOLVED, AND IT WAS NEVER OUR BUG

**[espressif/esp-hosted-mcu#243](https://github.com/espressif/esp-hosted-mcu/issues/243).** Found
2026-09-21. Our board, our symptom, our log lines, our sdkconfig. Full write-up in `LESSONS.md`.

One failed `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA` RX-buffer allocation permanently disables host
RX, because the retry added in 2.12.12 exits at an interrupt gate that was already cleared. Writes
keep working, so the driver still reports associated - which is exactly why a dead board showed
full bars.

**The fix is not reachable from our build.** `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y` plus
`CONFIG_CACHE_L2_CACHE_LINE_64B=y` are compiled into arduino-esp32's PREBUILT libraries; our
`esp32p4/sdkconfig` has neither. Reaching them means rebuilding the P4 framework libs with the IDF
component manager. Upstream's confirmed result: 4 stalls in 13 minutes became 2 h 52 m with zero.

**#41 is dead as a theory** - NVS writes are not the cause. **#59's C6 update was real and worth
doing** and was also not the cause.

**What we keep regardless:** detection (a board now knows it is offline) and the RSSI poll backoff,
which stops the 10 s UI freezes once a board has stalled. Neither fixes the stall.

### The superseded theory, kept for the record

#### #49 is half solved, and #59 was NOT the other half

**Detection works. Recovery does not.**

The fix on this branch means a board now KNOWS when its link is dead — the WiFi glyph drops to a
bare red stalk, the dump says `DEGRADED`, and `isOnline()` returns false. Before this week a dead
board sat showing full green bars for six hours. That part is done and confirmed on hardware.

**What it cannot do is get back on.** Both P4 boards were still off the network the next morning.
`DEGRADED (none)`, `Offline (last reason: 2)` (`AUTH_EXPIRE`).

### The finding that reframes it — see #59

**Neither S3 board has ever dropped.** The S3s have a built-in radio; the P4s reach WiFi through a
separate ESP32-C6 over an internal link.

Waveshare's own `docs/P4_C6_HOSTED_WIFI.md` (in two of the vendor trees under
`reference/Waveshare Official Repos/`) publishes a host/slave compatibility matrix:
esp_hosted **1.4.x** pairs with ESP-IDF before 6.0; **2.12–3.0** pairs with 6.0 and later. **Our
host runs 2.12.11.** The C6 runs whatever Waveshare factory-flashed, version unknown — because the
query for it fails.

That query failing is the boot warning we dismissed as cosmetic for weeks:

    E rpc_core: Response not received for [0x15e](Req_GetCoprocessorFwVersion)

An old slave does not implement that RPC at all. **It is the mismatch announcing itself.**

Waveshare's own doc says to validate *"association, IP traffic, reconnect, and restart behavior"*
when host dependencies change. **Reconnect** is our exact symptom.

It also explains the detail that puzzled everyone: a failed board could not raise its **own**
rescue access point either. If the host-to-C6 conversation is broken, every radio command fails —
not just joining someone else's network.

**DONE on `WS_P4_4B`, 2026-09-19. The C6 was on older firmware; it is now on 2.12.9 and the boot
warning is GONE** - on a board that printed it every boot for weeks. Procedure, evidence and
rollback in #59.

**Whether it fixes the dropouts is unproven.** They take 5-6 hours on that board, so only a soak
answers it. `WS_P4_5` and `WS_P4_7B` are deliberately untouched as controls: if the 4B survives the
night and they do not, that is as close to conclusive as this project gets. **That soak is the
single most important thing to check next.**

### SOAK RESULT, 2026-09-20: the C6 update did NOT fix it

Read out of Home Assistant rather than off a serial cable - HA knows when it last heard from each
board, which is the same question and needs no port:

| Board | C6 | HA last heard |
|---|---|---|
| `WS_P4_4B` | **updated to 2.12.9** | **9 h 27 m ago** |
| `WS_P4_5` | untouched | 1 h 09 m ago - **contaminated, see below** |
| `WS_P4_7B` | untouched | 42 h ago - unplugged from the PC, not evidence |
| `CYD_S3_3248` | n/a, own radio | seconds |
| `WS_S3_4B` | n/a, own radio | seconds |

**The 4B dropped anyway.** It lasted about 9.5 hours against the 5-6 it used to manage, which is
*weak* evidence of improvement and nothing more - one sample, and nobody recorded when its clock
started. The C6 firmware mismatch was real and worth fixing on its own merits; it was not the cause.

**`WS_P4_5` is a void data point and it is my fault.** It went quiet around the time a flash
attempt was killed on COM15. The evidence says `pio` crashed before `esptool` ever wrote (no
`Writing at` lines in its log), but an orphaned `esptool` was found later, so it cannot be ruled
out. Do not cite that 1 h 09 m for anything.

**What this leaves.** The P4/S3 split still holds perfectly - no S3 board has EVER dropped, both
P4s do - so the fault is still in the esp_hosted path, just not in the version pairing. #41 (do
NVS writes disrupt the SDIO transport?) is the next unexamined suspect and is still open.

**A cheap instrument nobody was using:** `scripts/scan_ha_icons.py`'s sibling technique. HA's
`last_updated` on any entity a board publishes answers "is that board alive" for every board at
once, from a PC, with no serial cable and no touching the fleet. That is how this result was
obtained and it is how the next soak should be read.

**Do not read the S3 boards as evidence either way.** `CYD_S3_3248` and `WS_S3_4B` have built-in
radios, have never once dropped, and are not part of this experiment. A long uptime on the 3248
says the firmware is not leaking or wedging; it says nothing about #49, because the 3248 does not
have the hardware that fails. Only the three P4 boards can answer this.

Note the update is a full-flash write and wiped NVS, so the 4B is running on the compile-time
credentials and its `_proven` flag has reset.

### What the #49 branch actually built

None of this becomes wrong if #59 turns out to be the cause — a wall panel still has to survive a
router reboot.

- **RSSI is re-polled with an age stamp.** It used to be read once in the `GOT_IP` handler and
  never again, so the header showed a value from the moment of association for as long as the
  board stayed up. Not a stale cache — a number nobody ever asked for a second time.
- **`LinkHealth`** — a second axis beside `ConnState`, because one enum cannot express "the driver
  says connected and it is wrong". Fed by an ICMP probe ladder (gateway → DNS → off-LAN, rotating
  on failure so a rate-limiting router cannot convict a healthy link) and by MQTT's verdict
  relayed down through `SystemCore`.
- **A three-rung recovery ladder** — re-associate, cycle the radio, hand back to the state machine.
- **`isOnline()` returns false on `LINK_DEAD`.** This is the line that makes the verdict mean
  something to the rest of the system: every caller asks `isOnline()` and none asks
  `getLinkHealth()`.

**Two guards that must not be removed casually:**

- A probe that has **never** been answered is not evidence. Many routers drop ICMP; without this
  every board would convict its own healthy link within minutes and cycle its radio forever.
- Each completed recovery ladder widens the cool-off, so a board whose problem is upstream settles
  into checking occasionally rather than thrashing.

**The probe is demand-driven** — a held MQTT session is continuous evidence, so a healthy board
sends none at all. That keeps a 2.5 KB task stack out of internal RAM on `CYD_S3_3248`.

---

## What is next

### IN FLIGHT RIGHT NOW - the P4 library rebuild (#61)

A lib-builder run is underway in WSL as of 2026-09-21. If you are picking this up cold, this is
where it got to.

| | |
|---|---|
| WSL | Ubuntu-24.04 LTS, WSL2, working |
| lib-builder | `~/esp32-arduino-lib-builder`, branch **master** (IDF `release/v5.5`) |
| IDF resolved | **5.5.5** - exact match to our framework, so no version drift |
| menuconfig | done: PREFER_SPIRAM on, L2 line 64B, USE_MEMPOOL restored to on |
| build | **running** - `./build.sh -t esp32p4_es qio 80m_200m` |

**Next step when it finishes:**

```bash
find ~/esp32-arduino-lib-builder -name sdkconfig -newermt '-3 hours'
python scripts/verify_p4_sdkconfig.py <that path>
```

**Do not install on a green tick alone.** The judgement is not "is the diff empty" - it is "is
every difference explainable, and would any of them change behaviour we care about". Build-id and
version strings are fine. Anything touching memory layout, task stacks or the WiFi/lwIP path needs
a hard look first.

**Expect more than two differences.** `USE_MEMPOOL` and the L2 cache line BOTH differed from
Espressif's shipped build before anything was typed, which means lib-builder master has drifted
from 55.03.311. If the unexplained list is long, the fallback is to pin lib-builder to the
`idf-release_v5.5` tag rather than master and rebuild - closer to the shipped point, fewer
incidental changes.

Everything else is in `docs/REBUILD_P4_LIBS.md`, including the two traps that nearly went into it
as instructions: lib-builder's branches are named after ESP-IDF rather than Arduino, and `-t` takes
the CHIP VARIANT rather than the target.

---

0. **DO THE REVIEW, THEN SIGN OFF #43, THEN MERGE.** `docs/REVIEW_2026-09-20.md` has the test
   plan, T1-T9. The owner's words, 2026-09-21: *"we're opening more than we're closing sometimes"* -
   so the review closes issues before anything new starts. Do this FIRST.

1. **#61 - the esp_hosted workaround.** This is now the top build task and it is why four of eight
   boards are unusable. `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y` +
   `CONFIG_CACHE_L2_CACHE_LINE_64B=y`, which means rebuilding the P4 framework libs.

   **URGENT since 2026-09-21, and the urgency is new.** The HA dashboard made the boards *worse*,
   exactly as #243 predicts: before #43 the P4s carried steady MQTT traffic and lasted 5-9 hours;
   now they do 18 REST fetches plus a websocket at boot - "bursty inbound TCP", the literal trigger
   in the issue title - and die in **seconds**. `WS_P4_5` showed two correct cards and dropped.
   `WS_P4_4B` never rendered a full page. We did not break them; we started feeding them the
   workload that reproduces the defect immediately instead of overnight.

   **Do NOT expect pioarduino 55.03.312 to fix it.** Checked 2026-09-21: Arduino 3.3.12's only
   hosted change is PR 12879, pinning `esp_hosted` to 2.12.3 "for lower hosted RAM usage". That is
   OLDER than what we run, predates the retry added in 2.12.12, and sets nothing about SPIRAM. It
   may change which failure mode appears; it will not remove it.
2. **2.7 card types.** The 18 real entities are now on glass, so this is judgeable for the first
   time: a `door` type, `LightCard` learning brightness, the corner-icon/hero split.
4. **2.8 slots** — and the card-corner artifact below goes with it.

**#49 IS NOT FIXED.** The C6 update did not stop the dropouts - see the headline below.

Done 2026-09-19: #50, #51, 2.6's vertical swipes (#17 stays open for the horizontal half).
Done 2026-09-20 on `feat/43-ha-websocket`: #43 inbound, #56, #57, **#60**, **#44 outbound**.

**#44's outbound leg is written but HAS NEVER RUN.** Nothing can tap the screen remotely, so the
path from a card tap to `call_service` has only ever been compiled. The transport under it is
proven (~91 ms echo, measured). T9 in `REVIEW_2026-09-20.md` is that test.

**#44 is not finished, deliberately.** Nothing subscribes to our own `/set` topics -
`MqttManager::subscribeCommand()` exists with no caller - because no entity in the fleet is both
writable AND advertised. The virtual test switches are `advertise = false` on purpose. The hook is
where it goes the day a real one exists.

## #43 — WHAT SHIPPED, AND HOW TO CHECK IT

Inbound is code-complete and running on `CYD_S3_3248`. Awaiting owner sign-off, then merge.

**The chain, as the boot log prints it:**

    [HA] auth_ok - session ready
    [HaProv] subscribe_trigger id 1 for 18 entities (675 B)
    [HaProv] HA ACCEPTED the subscription (id 1)
    [HaRest] initial values: 18 fetched, 0 failed
    [Cards] 8 of 18 cards placed; 10 dropped for space

If all five lines appear, the whole feature works. If `HA ACCEPTED` is missing, nothing else below
is trustworthy.

**How to read a board without a serial cable.** Only `WS_P4_5` has `ARDUINO_USB_CDC_ON_BOOT=0`;
the rest route `Serial` to native USB, which on `WS_S3_4B` is not cabled at all. Use HA instead -
see the soak table above. `pio device monitor -p COM10` works for the 3248.

**Pieces, and where each lives:**

| | |
|---|---|
| `components/esp_websocket_client/` | vendored Espressif v1.6.1, unmodified. See its README |
| `components/Fleet_HA/HaClient` | socket + auth + reassembly + retry ladder |
| `components/Fleet_HA/HaRest` | initial values, one entity per `loop()` pass |
| `components/Fleet_HA/HaValue.h` | `haCoerceState()` - ONE copy, shared by both paths |
| `components/Fleet_Providers/HaProvider` | the subscription and the live feed |
| `components/Fleet_Providers/ExternalEntities_HA.h` | the 18 entities |
| `include/Dashboards/Dashboard_HA.h` | the page, behind `-D USE_HA_DASHBOARD` |
| `scripts/scan_ha_icons.py` | which icons HA actually serves |

### The things that will bite whoever touches this next

- **HaProvider's receive path runs on the WEBSOCKET TASK, not `loop()`.** First asynchronous
  provider in the project. It must never touch LVGL; it writes through `EntityRegistry`'s mutex.
  Sends happen on the loop task only - `HaClient::sendText()` refuses otherwise.
- **Request ids must strictly increase within a connection.** HA answers `id_reuse` per request,
  not by dropping the socket, so a pooled or recycled id scheme fails looking like an HA bug.
- **A reconnect is a COLD START.** HA discards subscriptions with the connection. Nothing to clean
  up; everything to rebuild. `HaProvider` watches `HaClient::sessions()` - a counter, not a
  boolean, because a drop and recovery between two `loop()` calls is invisible to a boolean.
- **`staleAfterMs` is 0 on all 18 HA entities and that is deliberate.** `subscribe_trigger` fires
  on CHANGE, so silence carries no information and a stale window would grey out every quiet
  sensor overnight. Death is reported by HA saying `unavailable` - that is #56.
- **HA serves Fahrenheit.** It converts to the user's display unit before sending. The same deck
  probe reads 53.276 F here and ~11.8 C over Zigbee2MQTT. The page converts only when the source
  says `C`, so both land on F - but never infer a unit from a `device_class`.
- **The 8 KB reassembly buffer is a design constraint, not a tuning knob.** It is why the device
  registry (115 KB) is not fetched and why areas are resolved server-side instead.

### Deliberately NOT built

- **Area resolution at runtime.** Measured and documented (`ha-websocket.md` §7a): one
  `render_template` resolves all 18 in **818 bytes** against the device registry's 115,742. Not
  implemented because `Dashboard_HA.h`'s hardcoded areas are already correct, so it changes
  nothing visible. Build it when pages are grouped by area.
- **Reading `attributes.icon` live.** The glyphs are now in the font, but nothing consumes the
  field yet. 2.7.
- **Outbound.** That is #44.

---

### #43's transport — DECIDED 2026-09-19: both, and they are not redundant

**REST does not get replaced by the websocket. They do different jobs, and `ha-websocket.md` §7
already assumes both.**

| Job | Transport | Why |
|---|---|---|
| auth, handshake | **WS** | three messages, milliseconds |
| area / device / entity registries | **WS** | `get_states` is 787 KB and unusable; the per-entity forms are 842 B |
| **initial value, per entity** | **REST** | `GET /api/states/<id>`, 399–843 B, 4–25 ms. Explicitly *not* `get_states` |
| **live changes** | **WS** | `subscribe_trigger`, filtered server-side |
| outbound commands (#44) | **WS** | `call_service` |
| sensor history for sparklines | **REST** | `cards.md` §4's "fetch it, do not store it". Unmeasured — see §8 |
| publishing OUR entities into HA | **MQTT** | discovery; the websocket cannot replace it |

**The 30 s latency goes away, and that is the websocket's doing specifically.** REST is *pull* — the
reference projects poll on a 30 s task, so a door could take half a minute to appear. WS is *push*,
so a change lands as fast as the LAN carries it. The 30 s was never a property of REST as such; it
is what polling costs. REST keeps its place above because one-shot fetches are exactly what it is
good at.

**Use `subscribe_trigger`, never `subscribe_events`.** Measured on the owner's instance: all
`state_changed` is 731 events and 913 KB per minute, ~15 KB/s of JSON parsed continuously to find
the handful that matter. The same window filtered to our 18 entities was **zero bytes**. That is
~100x and it is the single most important finding in `ha-websocket.md`.

### The library

Checked rather than assumed: **both ESP-IDF reference projects use REST for HA.** NINA does
`GET /api/states/{entity_id}` per tile; `ha-dashboard` polls on a 30 s task. Neither uses a
websocket for HA — though NINA pulls `esp_websocket_client` for other feeds.

`esp_websocket_client` is an **official Espressif component from the Component Registry**, not part
of core IDF — which is exactly why it is absent from the Arduino prebuilt libs. Nobody writes their
own.

**DECIDED: vendor `esp_websocket_client`** rather than write one. It is plain ESP-IDF C over
`esp-tls`/lwIP, both already linked here; the only obstacle is that PlatformIO's Arduino build
cannot run the IDF component manager, so it would be vendored like `bb_captouch_fork`. That is the
same code an ESP-IDF migration would use later, and it keeps the owner's "avoid Arduino-specific
libraries" constraint intact.

REST is *pull*: a 30 s poll means up to 30 s before motion or a door shows on screen. Outbound
commands are fine either way. It is inbound latency that suffers.

---

## Deliberately postponed — do not rediscover these

| What | Where it goes |
|---|---|
| **The card header band overhangs the rounded corners** — CONFIRMED on glass with a photo, 2026-09-19. `HANDOFF` was right and the code comment in `Card.cpp` was wrong | 2.8, with the slot rework |
| **RSSI card shows STALE when WiFi is down**, which is the wrong word. `ST_PAUSED` is defined as "the user's own choice", so it must not be overloaded — this wants its own state | with #56 |
| **Hide the battery glyph when a card is stale** | with #56 |
| **The system header bar needs its OWN colour**, not the scheme's. Paper makes it unreadable | 2.8 |
| **Corner icon = the DOMAIN; the hero = the specific fixture** | 2.7 |
| **State-dependent hero glyphs**. HA ships these in `attributes.icon` | 2.7 |
| **A `door` card type**; a `LightCard` that handles dimming/RGB/colour-temp | 2.7 |
| **Icons look undersized on large cards** — faces are picked by density alone, never by cell size | 2.7 |
| **Per-card full-screen detail page**, long-press, background dimmed | 2.7 / 3.2 / 4.4 |
| **Horizontal swipes** — left unclaimed on purpose, they belong to page navigation | 2.6 |
| **Swipe down on the LEFT half below the top band** — currently opens the log from the top band only; nothing else is bound | 2.6 |
| **Arduino_GFX only uses one draw buffer** | 2.9 / #40 |
| **Priority's vocabulary** — four bands was never ratified | 3.1 |
| **OTA** | Phase 5 |

---

## Things that will bite you

**Read the file-editing rule at the top of `CLAUDE.md` before your first edit.** It cost this
session three separate incidents in one day, including one where a script reported success and
changed nothing because its anchor string had been mangled the same way.

**Build from PowerShell, not Git Bash.** pioarduino rejects MSYS shells.

**`pio run` with no `-e` builds ONE environment.** Same for `pio device monitor` — it inherits
`default_envs` and will try to load a different board's ELF.

**Clear `.pio/build_cache` after editing any BSP header or `lv_conf.h`.**

**A serial monitor resets the board when it opens**, which destroys the evidence of a fault you
were trying to capture. Use `--rts 0 --dtr 0` to attach without resetting:

    pio device monitor -e WS_P4_TOUCH_LCD_5 -p COM15 -b 115200 --rts 0 --dtr 0

**Six of eight boards are `ARDUINO_USB_CDC_ON_BOOT=1`**, where serial is a buffered USB endpoint:
the boot log is gone before a monitor can attach, and on a hang everything still in the buffer is
lost. `SystemReport::line()` and the LVGL log callback both flush for this reason.

**Do not run two `pio` invocations at once.**

**Anything drawn on a panel stays ASCII, except `°`** — and a generated `num` face carries only the
glyphs `gen_type_scale.py` lists. A colon was missing from it until 2026-09-19, which showed as
tofu boxes on exactly the three boards using a generated face.

**Verify from outside the device.**

---

## What is measured vs. what is assumed

| Measured | |
|---|---|
| Card cost | ~2.8 KB in `lv_mem` |
| `lv_mem` pool | 128 KB static array in internal DRAM. 192 KB on P4 links and breaks the network |
| One font face | ~96 KB of flash |
| #49 branch cost on `CYD_S3_3248` | +168 bytes internal RAM, +8,456 flash |
| System panel, `WS_P4_5` | 640 px wide at x=623 (screen 1280, 740 logical), content 334 px |
| System panel, `WS_P4_4B` | 540 px wide at x=165 (screen 720, 480 logical), content 298 px |
| HA `/api/states` | 742 KB across 1,662 entities |
| HA event rate | 12.2/s on `subscribe_events`; **zero** for the same entities via `subscribe_trigger` |
| Screenshot PSRAM peak | 0.88 MB on `CYD_S3_3248`, 5.27 MB on `WS_P4_5` |
| MQTT connect block | up to ~3 s TCP + up to `SOCKET_TIMEOUT_S` for CONNACK, **on the LVGL task** |

**Still assumed:** that the C6 firmware is the cause of #49; that NINA's updater matches our board
wiring; that the recovery ladder works at all — **it has never once been seen to run to completion
and succeed.** No swipe gesture has been verified by a finger since the last two fixes.

---

## How to work with this owner

He is a hobbyist and an ESP32 enthusiast, not a professional developer, and explicit about that —
but he reads code, spots real bugs, and has caught several that were not obvious. **Treat his
instincts as data.** The broker log, the missing colon being a font problem rather than a
connectivity one, and the "swipe from anywhere" regression were all his.

**What works:**

- **Show, don't spec.** Build something he can react to.
- **Plain language, not metaphor.** His words: "sometimes I get a little lost in the slang."
- **Give a recommendation, not a menu.**
- **Own mistakes plainly and move on.**
- **He flashes fast** and will often hand you a COM port mid-turn.
- **Push back on scope when it is real.**
- **Structure long answers.** He said directly that the what-I-did / caveats / uncertain / next
  breakdown is his preferred format.

**What to avoid:**

- Don't say "we should wait until phase X" as a reflex.
- Don't trust a browser mock. The bench narrows the options; the glass decides.
- Don't commit straight to `main`. Feature branch, then merge.
- **Don't guess a fourth time.** Instrument it or ask the far end.
- **Don't claim a script worked because it printed something.** Check the file changed. On
  2026-09-20 this rule was broken three separate times in one day by the same mechanism: a Python
  replace whose anchor did not match, followed by a `print("ok")`. It cost a pause guard that
  silently did nothing, the whole of #56's wire-side detection, and a build flag that never reached
  `WS_S3_TOUCH_LCD_4B`. **Assert the anchor, then grep the file.** The owner found two of the three
  within minutes of looking at hardware.

### Added 2026-09-21, from the session that solved #49

- **He is often right about the shape of a thing before he can name it.** "Priority should be the
  entities I actually reach for" arrived as a musing and was a better model than the
  domain-weighting in `Dashboard_HA.h`. "Can a card group several doors?" was group cards. When he
  says "I'm wondering whether...", that is usually a design instinct, not a question - engage with
  it rather than answering narrowly.
- **A half-remembered detail from him is worth re-checking at the source.** He recalled NINA
  deferring NVS writes for wear reasons rather than connectivity. He was half right, and going back
  to find out surfaced a second rationale in a different file that reframed #41 entirely.
- **He will solve your problem if you tell him what you are stuck on.** Four weeks of #49 theories
  ended because he went looking for other people with the same symptom and found
  esp-hosted-mcu#243. Say plainly what is unexplained.
- **Tell him what has NOT been tested.** He acts on it immediately and without complaint. The tap
  path, the pause `unavailable` publish and the icon rendering were all flagged as unverified and
  all were on glass within the hour.
- **Report negative results as clearly as wins.** "The C6 update did not fix it" and "that data
  point is void because I killed a flash on that port" were both received as useful. Hedging them
  would have been worse than useless.
- **Do not let a background build run while you edit the tree.** Two flashes failed on 2026-09-20
  because a background `pio` compiled a half-finished edit. Edit first, then flash.

**Versioning:** `A.B.C.D`, where **C is the roadmap phase**. Tag on `main` at merge, never during
development. A dirty tree appends `+dirty`.
