# Future Improvements

Ideas, enhancements and fixes that affect the whole build environment or HAL, or that the owner has
asked to keep: not tied to one device (that is `HARDWARE_STATUS.md`), not bugs, not blocking
anything. Each item says what it is, why it is deferred and what it touches. An item is deleted when
it is built. Open questions for a milestone live in its GitHub issue; this file holds what has no
issue.

## Display and LVGL

### The last three boards on Arduino_GFX

`CYD_S3_8048` and `WS_S3_5B` (RGB) and `CYD_S3_3248` (QSPI) still run Arduino_GFX; the other six
boards run raw `esp_lcd` (`docs/display/README.md`, `docs/display/architecture.md`). Moving them is
the rest of ROADMAP 2.9 (#67), done one board at a time behind a per-board `-D DISPLAY_ESPLCD`.

Why the move is worth it:

- `Arduino_GFX::getFrameBuffer()` returns a single `uint16_t *`, so the API cannot express double
  buffering. The RGB class requests two hardware framebuffers and hands out one, which is a framebuffer
  of PSRAM paid for and never used, and is the reason `WS_S3_5B` (1024x600) tears.
- The QSPI path (`Arduino_Canvas`) transforms rotation per pixel on the CPU. `CYD_S3_3248` runs in its
  native portrait orientation, where no transform happens; a board's native orientation is the
  cheapest fix and is worth checking before assuming rotation is required.
- `ESP32_Display_Panel` (Espressif, Apache-2.0) covers most of these controllers.

Not done yet, so deferred: judging a render-stack change needs the demanding workload (cards, swipes,
animation) running on it, which is why it came after the card system.

### Diff the Arduino_GFX fork against every vendor tree (#40)

`reference/Waveshare Official Repos/` holds vendor repos that each ship a working copy of
`Arduino_GFX` for their board: `WS_P4_4B`, `WS_P4_5`, `WS_P4_7B`, `WS_S3_4B`, `WS_S3_5B`, and an
AMOLED 2.06 board that is not in the fleet. Only the P4-5's has been diffed, and only two files; that
diff found the reset-polarity fix. The rest have never been compared. The files known to differ and not
examined are `Arduino_GFX.h`, `Arduino_ESP32RGBPanel.cpp`, `Arduino_ESP32SPIDMA.cpp`,
`Arduino_DSI_Display.h` and `Arduino_RGB_Display.h`; the RGB ones bear on `WS_S3_5B`'s tearing. Do this
before tuning buffering on any board that stays on Arduino_GFX. It matters less for a board that moves
to `esp_lcd`.

### Smaller display items

- **Minimum brightness as a BSP field.** `Panel_Display.cpp` hardcodes the slider's floor (45 for the
  two 4B boards, 3 for the rest, from one measurement on the 7B). It belongs in `DisplayConfig`, measured
  per board.
- **LVGL buffering for the Arduino_GFX boards, from measurement.** `DOUBLE_BUFFERING` and
  `BUFFER_SIZE_PX` are honoured by both flush paths, but that path's flush is synchronous, so a second
  buffer buys nothing there. Measure before building per-device logic; the esp_lcd boards derive their
  buffering from `bspPresentMode()`.
- **PPA rotation outside the esp_lcd path** is not planned: the Arduino_GFX boards are not P4s and the
  S3 has no PPA. For a P4 using it, the buffers need 64-byte alignment, `MALLOC_CAP_DMA |
  MALLOC_CAP_SPIRAM` and explicit cache writeback/invalidate around every operation (`REFERENCE_PROJECTS.md`
  covers what is minable from Allsky's `ppa_accelerator.h`: approach only, that repo is unlicensed).
- **A written memory budget.** A table of what lives in internal SRAM and in PSRAM, in what allocation
  order, and the headroom on the worst board (`CYD_S3_3248`, whose QSPI draw buffers must stay internal).
  The largest free internal block matters more than the free-heap total: `softAP()` succeeded from a
  13 KB block, and the Wi-Fi driver allocates more per associated station. Nobody has written the table;
  `lv_mem_monitor()` and `ESP.getFreeHeap()` are the instruments.
- **A viewing-distance term for scaling.** Density scaling makes everything the same physical size,
  which is right for touch targets (a fingertip is 9 mm on every board) and arguably wrong for text on a
  7" panel across a room. Add a small per-board nudge only if something still looks wrong on glass.
- **`UI::sc()` duplicates LVGL's `lv_dpx()`.** `UI::sc(n)` is `n * PPI / 170` (`UITokens.cpp`);
  `lv_dpx(n)` is `n * PPI / 160` (`LV_DPX_CALC`, `lv_display.h`). The same function against a different
  reference constant. Collapsing them removes a concept and changes nothing visible (1.0625x), at the
  cost of a pass over every token and a flash to confirm nothing shifted: do it when something else is
  already touching those values. The same kind of duplicate is the touch rotation in
  `TouchManager::mapCoordinates()`, which `bb_captouch` implements itself. DPI in LVGL scales only
  shapes, never fonts, so `lv_display_set_dpi()` cannot replace the generated type scale.

## Audio

- **AEC is off indefinitely** (`ENABLE_AEC`). Voice returns as ROADMAP Phase 7: the boards speak
  ESPHome's native API from this firmware (DECISIONS A18, `design/voice-esphome.md`).
- **ES8311/ES7210 mic gain and output volume tuning**, from Waveshare's current sample repos rather than
  chip defaults. `CYD_S3_8048W550` audio is notably quieter than the other boards, unexplained; it may be
  a gain issue.
- **A generic pattern for other peripherals** (IMU, RTC, power management, temperature/humidity): extend
  the `HAS_X` BSP flag pattern, each paired with a small manager class that does nothing unless flagged
  (the shape of `AudioManager` and `TouchManager`) and a GUI panel gated the same way `Panel_Audio.cpp`
  gates its widgets. Relevant once the boards with an AXP2101, PCF85063, QMI8658 or SHTC3 get firmware.
- **A third `AudioManager` path** for bare I2S mics and amps with no register-based codec (ICS43434,
  INMP441, MAX98357A); the driver only knows ES7210/ES8311-style codecs. Open question: whether the
  current board lineup needs it at all, or only a future "basic boards" project.

## Connectivity and Home Assistant

- **The panel's own settings as HA entities.** Screen brightness (a `number`), dimming and screen timeout
  (`select` / `number`), perhaps the color scheme, published over MQTT discovery as the device's own
  sensors are, so an HA automation can use the house's sensors (a room's lux setting the panel's
  brightness, the screen sleeping when the room is empty). Belongs with the settings page (4.1, #23,
  D-7): each setting gets one definition that the page, the build sheet and HA all read and write. The
  `/popup`-style debug endpoints stay debug.
- **MQTT stays for what the board publishes.** HA's entities arrive over the websocket (`Fleet_HA`); the
  board's own entities and plain broker topics go out over MQTT discovery. MQTT discovery is the only way
  to publish entities that HA backs with a device and keeps across a restart, so the owner's plan to add
  sensors, hardware and controls to boards depends on it.
- **More entities the panel could expose to HA**: battery information; **custom toast popups an HA
  automation can invoke** (an inbound command entity, a different shape from the state we publish
  today); and further out something like Android's Live Updates or iOS's Live Activities, a persistent,
  updating notification surface driven from HA, probably a card type with a lifecycle rather than a
  toast. With a full web UI doing detailed configuration, not everything needs to be manageable from HA.
- **A per-peripheral discovery descriptor**: each `HAS_X`-gated peripheral registers a small descriptor
  (component type, object id, name, device class, state topic suffix) into a registry that the MQTT layer
  walks at connect time, injecting the device id (`<device_id>_<object_id>` as `unique_id`), so a
  peripheral advertises its entity whichever board it is wired to.
- **NVS encryption.** Connectivity settings are stored unencrypted in NVS (`Preferences`). The standard
  scheme ties NVS encryption to full flash encryption (an irreversible eFuse burn in release mode, not
  something to flip while boards are still being reflashed); chips with an HMAC peripheral (S3 confirmed,
  P4 unconfirmed) support a narrower key-derivation scheme. Either is a one-time per-device provisioning
  step (eFuse plus an `nvs_keys` partition), and the `Preferences` call sites are the same with or
  without it. Whether PlatformIO's Arduino build exposes the needed sdkconfig options is unchecked.
- **A first-boot access point and a web UI that configures everything** (#6 captive portal, #27 web
  config, #36 distribution `platformio.ini`; scoped with the companion web app #87). A first-boot flag
  defaulting to STA+AP lets a user connect to the device and set up connectivity, MQTT, HA access, cards,
  pages and preferences in a browser; virtually everything should be reachable there, not only in the
  on-device GUI. It makes shipping a flashable binary viable, which is what lowers the barrier to anyone
  else adopting the project. A lean custom portal (native `WiFi.h` plus `DNSServer`) rather than
  `tzapu/WiFiManager`, because that library owns the whole scan, connect and portal flow with its own web
  UI and would fight the touchscreen as the primary settings surface. Stretch, not a priority: a web
  interface that builds a build sheet and compiles a custom binary.
- **Platform pin.** Reaching the P4/C6 boards needed pioarduino `55.03.311` (arduino-esp32 `3.3.11`) and
  PlatformIO Core `6.1.19`: the P4 has no radio and depends on its onboard C6 over SDIO (`esp_hosted`),
  and older host drivers could not complete the RPC handshake. Do not float that pin without rebuilding
  all nine environments.
- **Still open from Phase 1**: the captive portal (#6), the on-device connectivity screen (#7) and
  outbound entity commands beyond on/off and light levels (#44).

## Cards and the page

- **Icon face from the cell as well as the density.** Icon faces are picked by pixel density alone, so
  they are the same physical size whatever the card: right across boards, wrong across card sizes on one
  board (a puny corner icon and a lost hero on a big card; the sensor window's hero on a 7"). The faces
  already exist per board. The cost is flash: every face a translation unit references is linked, so a
  third referenced size is real money (CLAUDE.md, "Two costs"). Measure before doing it; it lands with
  the card library (#31).
- **The old card pinning, shelved** (K164). "Pin" on the glass means "shown on the page". The first grid
  engine's pinning, a card at a fixed cell (`PageSpec`'s place, `CardPage`'s "pin rejected ... flowing",
  `Layout.cpp`'s invalid pin), predates the arranger, which seldom lets a page overflow, and the layout web
  app makes a hand-placed cell less useful still. Its code is still there, along with the arranger's own
  "pinned" vocabulary (`_pinCols` / `_pinRows`, Arranger.cpp's pinned counts; the Grid line on the glass
  says "fixed"). Remove it, or refit it to the arranger under a new name once hand arranging is designed,
  and never under the word "pin".
- **A full-screen detail page for every card** (2.7, 3.2, 4.4), modelled on HA's more-info dialog: tap a
  card, get the extra information and controls for that entity, with the context panels sliding up and
  the cards visible behind. Interest is recorded in `REFERENCE_PROJECTS.md` (`esphome-modular-lvgl-buttons`'
  `detail.yaml`). The card window covers part of this; the rest lands across 2.7 (what each domain shows),
  3.2 (how a sheet declares it) and 4.4 (the context-sheet mechanism).
- **A manila-folder tag.** `HDR_TAG` draws a rounded rectangle above the card's top-left corner. The owner
  wants the tag's bottom corners to curve outward so it reads as a tab pushed up from inside the card,
  probably inset slightly from the left edge. Low priority; try it on glass.
- **Two cards for when the engine allows**: an irrigation card (the owner's HA irrigation system:
  scheduling and queuing, valve and zone entities decoupled from hardware, climate-driven run times, a
  guardian for stuck valves; probably a group card with its own detail page), and thermostat controls
  (`EntityKind::CLIMATE` shows read-only today, DECISIONS K144).
- **Switching a light's control live while the selector is dragged**, rather than on release. Measure
  frames with `DEBUG_FRAMES` first (W6).
- **Linen's panel slides** are a little choppier than the dark schemes' on the P4_5 (frames 8-40 ms,
  "totally passable"). Linen may get chrome of its own.
- **The 4880 shows the card window in portrait** and nothing was tuned for it (font sizes and fit); far
  down the list.
- **A rotation switcher** (with #71). Rotate-and-restart from UI Tweaks is small: the board already works
  out its frame path, layout and touch mapping from the rotation at boot. Switching live, without a
  restart, is a larger job.
- **The Card label row is greyed** until its redesign (K48): Current group / None / Custom, with a name
  window. With it: moving a card to another group gets its own row and window (a list of groups, each
  openable to its members, "change the label" or "move into a group panel"); NINA-style toasts; scene
  secondary info (#89); names and groups from the web UI (#87); the Selector look as a device-wide choice
  on the settings page (4.1).

### Arranging by hand, and an order by domain

Pages arrange themselves (K109); the owner wants these beside that:

- **A priority order for tiles by domain** (lights and switches first, then binary sensors, then
  sensors, or similar), so a cluster's tiles and the page's blocks fall in an order that means something
  before anyone moves them.
- **Arranging on the device**: a manual arrange mode, perhaps from the page slot's panel, where tiles,
  cards and clusters are dragged and dropped ("at a minimum some form of rudimentary manual arrangement
  mechanism"). He likes iPhone's jiggle and a temporary badge on each movable one. What a hand
  arrangement does to the arranger's answer (kept as pins, or the arranger re-run around them) is the
  design question.
- **The web app** shaping pages, clusters, cards and their sizes in detail makes the automatic answer
  matter less (3.1's build sheet). Its dashboard arranging could be the first part built at 3.1, so
  layouts and configurations can be tried quickly.
- **Each card's size, and tiles in a group by hand**: see ideas 3 and 4 in the brainstorm below.

### Brainstorm

Five ideas the owner wanted written down. None is scheduled or agreed design; each says what it extends
and where it would land.

1. **A roll-up entity per HA area, kept by HA** - one persistent entity for each area's doors, one for
   its lights, one for its sensors, that always reports its members and their states, including after
   the members change (a light moved to another area, a new door sensor added). The board would hold
   one entity per area and kind instead of a list it has to keep in step.
   - *Extends:* per-room status entities for cluster headers (`design/carried-from-2.13.md`, which
     notes the boards count from a group's members and may not need them) and K170 (an HA group entity is a group on the board; not built). The cluster header would read the
     roll-up's members (the header slots, K129's Header options).
   - *HA side:* a template entity built on `area_entities()` follows area changes on its own; a plain
     group helper does not, because its member list is fixed. It must be something the owner can read and
     manage without AI (the HA folder's process), named by function, one per area and kind. Open: one
     entity per kind, or one per area with members by kind as attributes; what its state says ("2 of 5
     on", "1 open").
   - *Lands:* HA's side of the 2.13 follow-on or 2.14 (alerts read the same roll-ups: a door left open
     is the door roll-up's state).
2. **The same for a group made on the board** - a group whose lights are in several HA areas reports its
   members and their states live, perhaps in its header.
   - *Extends:* the cluster header (K114-K119), the summary card (K98's words: "2 open", "Both"; the
     summary card is its own design, K189, not built beyond the compiled Garage - Doors), K170's
     "mirroring the boards' groups into HA" (a later idea there), and the panel advertising its own
     entities above: a board group published to HA over MQTT discovery would be the roll-up of idea 1,
     made on the board.
   - Open: on the board only (a header slot such as "3 of 5 on" that opens the members), or in HA too
     (the mirror); which board owns a group that several boards show (idea 5).
3. **More card presentations**:
   - a **card with a dimmer bar** - a light's level on the card face, set without opening the window
     (#31 card library, #86 control styles);
   - **revisit the card's layout** - where "last updated" goes, and a group shown as its solo card with
     **several members on its face** instead of one (K191, K193);
   - **card sizes from the grid** - half-size cards, cards of other widths, a solo card wide enough to
     show more. The arranger works in units already (K109, K120; `design/layout-engines.md`), so a size
     is a claim of units; what the arranger does with mixed sizes is the design question. Relates to the
     icon face from the cell above, and 4.2.
4. **Arranging by hand** - choosing what goes where on a page and in a group, and each card's size.
   Adds **size per card** (idea 3) and tiles inside a group arranged by hand as well as the page, to the
   arranging items above.
5. **Synchronizing boards** - with Fleet peer work (#55), push a group's configuration, or a whole
   board's, to other boards, or keep boards in step.
   - *Extends:* #55 peer discovery; K181 (Backup and Restore, not built; a backup from one board is
     meant to restore on another, the first manual form of this); Phase 3's export and import with the
     companion web app (#87).
   - Open: one-time push or kept in step; what is per board and never copied (its screen's arrangement,
     its own sensors, the Panel group); which board, or the web app, is the source.

## Touch

- **The library's own orientation system.** `bb_captouch` has per-controller orientation support
  (`setOrientation()`, `_iOrientation`, `fixSamples()`) that this project never uses; all rotation is the
  custom `TouchManager::mapCoordinates()`. Worth a real comparison against the vendor path.

## Hardware

- **Sync BSPs with Waveshare's latest repos.** Waveshare has updated the repos for all its devices, the
  P4 boards in particular, which were ESP-IDF-only before. Do an exhaustive pass per board: compare audio,
  display and LVGL bring-up against this project's, and import or examine vendor demos for hardware unique
  to that board (IMU, RTC, power management) that is not in this HAL yet.
- **P4 silicon revision.** Chips ship as `rev1_3` or `rev3_x`, a per-chip property (two units of one board
  model can differ) confirmed only by a chip-ID probe. `pioarduino` picks the matching prebuilt library
  variant from the board definition (`esp32-p4-evboard` selects `esp32p4_es`, pre-rev3), so the build
  setting is handled. `DisplayConfig.PHY_CLK_SRC` selects the DSI PHY clock per board; `PLL_F20M` and the
  IDF default behaved identically on a rev1.3 chip, so the rev1_3 / rev3_x clock distinction is real but
  unobserved and does not explain a DSI failure. Only the `WS_P4_5` has been probed (rev v1.3; one
  `esptool flash-id` per other P4 would settle what the fleet holds). `bsp_hw.SI_REV` stays
  `"unconfirmed"`; if it ever becomes load-bearing, read the revision at runtime and compare, as
  `checkAudioBspSanity()` does. Detail: `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.
- **`CYD_P4_1060`'s camera.** The board has a camera, with a through-hole in the front at the top in
  landscape. Unchecked: the sensor model, how it connects (the P4 has a MIPI-CSI input; whether this board
  uses it is unconfirmed), and whether Guition's pack in `reference/Guition Examples/` has an example. The
  P4 4B also has a CSI port, unpopulated on the owner's unit (`HARDWARE_STATUS.md`). Uses: presence
  detection, a doorbell-style view, a snapshot to Home Assistant.
- **WaveShare ESP32-S3 RGB Matrix board with a 128x64 HUB75 panel.** It has an ES8311, an ES7210 and dual
  mics, the codec pairing several boards already use, so `AudioManager` should mostly work once it has a
  BSP; also a QMI8658 (IMU) and an SHTC3. The HUB75 panel would be driven by
  `mrcodetastic/ESP32-HUB75-MatrixPanel-DMA`, an external dependency outside the display stack.
- **WaveShare Modbus-RTU-Relay-B** (8-channel RS485 relay module, no microcontroller of its own),
  intended to be driven by `WS_S3_TOUCH_LCD_5B` as a sprinkler controller, a special project outside this
  environment. The immediate to-do is in `HARDWARE_STATUS.md`'s `WS_S3_5B` entry (basic Modbus
  communication and relay switching).
- **Lithium battery power.** 3D-printed enclosures exist for most devices with room for a battery. Goal:
  one USB-C port on the enclosure that powers the device, charges the battery and carries data to a PC at
  once. Some boards have native battery headers (`HARDWARE_STATUS.md`); for the rest, TP4056 modules are on
  hand (USB-C input, with separate pads for input, battery and device output). The one fleet-wide
  battery item is the battery ADC "gauge" investigation, in `HARDWARE_STATUS.md`.
