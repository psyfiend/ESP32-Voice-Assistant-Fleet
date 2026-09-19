# Handoff — 2026-09-19

**Start here.** `CLAUDE.md` is the stable how-it-works. This is where we are, what will bite you,
and what to do next. Kept lean on purpose: anything that is "why we did X and not Y" now lives in
`docs/LESSONS.md`, and anything that is a design lives in `docs/design/`.

**A warning about this file.** When it paraphrases a spec, the paraphrase becomes the spec for
whoever reads it first. Say *which* document a summary is compressing, and treat vocabulary that
does not appear in the source as suspect.

---

## Where the project is

Phases 0, 1 and 2.1–2.5 are merged and tagged `v0.2.5`. Work since then is on
**`fix/49-link-liveness`**, 19 commits, pushed, **not yet merged**. It carries #49, #50, #51, the
first cut of 2.6's swipe navigation, and a lot of small corrections found on glass.

Four boards are attached and flashed: `CYD_S3_3248` (COM10), `WS_P4_5` (COM15),
`WS_S3_4B` (COM8), `WS_P4_4B` (COM7).

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

---

## THE HEADLINE: #49 is half solved, and #59 is probably the other half

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

0. **#59 — flash the C6 on `WS_P4_4B`.** Approved. Highest value experiment available.
1. **Finish verifying 2.6's swipes on glass**, then merge this branch.
2. **#43 — Home Assistant over the websocket.** See the transport note below.
3. **#44 — outbound commands.**
4. **2.8 slots** — and the card-corner artifact below goes with it.

### #43's transport decision, now informed

Checked rather than assumed: **both ESP-IDF reference projects use REST for HA.** NINA does
`GET /api/states/{entity_id}` per tile; `ha-dashboard` polls on a 30 s task. Neither uses a
websocket for HA — though NINA pulls `esp_websocket_client` for other feeds.

`esp_websocket_client` is an **official Espressif component from the Component Registry**, not part
of core IDF — which is exactly why it is absent from the Arduino prebuilt libs. Nobody writes their
own.

**Recommendation: vendor `esp_websocket_client`** rather than write one. It is plain ESP-IDF C over
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
- **Don't claim a script worked because it printed something.** Check the file changed.

**Versioning:** `A.B.C.D`, where **C is the roadmap phase**. Tag on `main` at merge, never during
development. A dirty tree appends `+dirty`.
