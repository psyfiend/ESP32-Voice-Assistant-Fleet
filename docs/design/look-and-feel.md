# Look and feel - the whole product

The owner's answers to the look-and-feel interview (`interview-phase2-3.md` §1), as direction every
later design choice (windows, header, groups, the settings file) is tested against. What is built is
described in `tokens.md`, `components.md`, `card-sheet.md`, `header-slots.md` and `group-cards.md`;
this file keeps the reasons and the owner's standing rules. Anything still open is marked **OPEN**
with what would settle it.

---

## 1. The owner's two standing rules

1. **"Most if not all of the cards will be coming from HA and I'd rather not sacrifice any
   functionality HA can offer."** Every HA function of an entity shown is *reachable* on the board -
   on the card's face, in its window, or as a service call - in domain priority order (lights,
   switches, covers, climate, sensors, weather, media, scenes and scripts). Anything ruled out is
   measured first, never ruled out by assumption.
2. **"I'm a sucker for eye candy. I will be sad if the best we can do for a dashboard is a simple
   grid of uniformly sized squares."**

Ruled out, owner agrees: live camera video (snapshots on events are acceptable) and maps. **Album
art is in**: it comes from HA's media proxy over plain HTTP (not the websocket, so its receive
buffer is not the limit), decodes into PSRAM (the P4 has a hardware JPEG decoder; the S3s decode in
software), and a 300x300 picture is about 180 KB of PSRAM, not `lv_mem`. To be measured when the
media card is built. A media player is wanted (Music Assistant, Sendspin, or one-offs for Spotify,
which NINA has a page for in `reference/`, or SoundCloud).

## 2. Who looks, from where

**Mostly the owner, at arm's length: glance, tap, done.** Devices in common areas must read from
2-3 m.

| Where | Job | Distance |
|---|---|---|
| Owner's nightstand | clock / alarm first (#35), dashboard second | arm's length, in the dark |
| Kitchen | a prominent weather station, plus other cards | 2-3 m |
| Garage | garage cards | 2-3 m |
| Desk | light use | arm's length |
| Irrigation | a dedicated irrigation interface | arm's length |

A board has a **role**, and the role decides more than which cards it shows. A per-device
**viewing distance** setting (close / far) that scales the hero value and state on top of the
PPI-derived type scale was agreed (#73) and is not built; it picks a millimetre target, never
board-specific numbers.

## 3. The feel

The owner gave a rule rather than three words:

- **At its smallest, a card does one thing extremely well: it advertises its state or its intended
  function.**
- Above that, small touches that make it feel premium and thought through: small weather animations
  on a weather card; a **clock card** that flips as the time changes, with several faces;
  transitions; drag-and-drop layout editing. "Not excess animations just for the sake of it."

The test for every choice: (1) at minimum size, state or function is unmistakable; (2) an effect is
allowed when tied to something real - time passing, weather, an action the user took - never idle
decoration; (3) every effect can be turned off per device. The three words: **clear, alive,
crafted**.

**Motion**: design for the P4s; cut effects where they lag on the S3s. Wanted: motion tied to things
happening - panels opening, page swipes, a long press opening a window. Not wanted: gratuitous
animation. Snappy comes before eye candy (W6).

## 4. Night - not built

A must-have for any bedroom screen. Light comes from a local sensor or any HA lux entity. The minimum
is customizable **night hours**, when the screen goes black and shows **a clock, or nothing**, and
**a tap brings the dashboard back** without also acting on what is under the finger. HA alerts are
optionally shown on the night screen. Night hours need wall-clock time (`TimeService`, SNTP, which
the boot report waits for up to 10 s). The bedroom clock is milestone 2.15, alerts 2.14 (K131,
K138).

## 5. Beyond the grid

The grid is not the goal. The owner is open to presenting **clusters** of information (rooms,
functions, logical groups) as something other than cards. Direction: **mixed card sizes on the unit
grid, and groups with a header and members inside one border, are the base**; fluid shapes (a sky
panel, a dial, an L-shaped kitchen) have good ideas but each is discussed before it is built. The
owner cannot conceive of a required L-shaped group, so groups are **rectangles**; the real question
is how members are arranged inside one.

- A group does not cost the cells its members would cost alone: inside a shared border members lose
  their own borders and sit closer on the group's own internal grid, and the group's header holds
  secondary entities (temperature, occupancy, lux) that would each take a whole cell. Groups can be
  both more logical and denser.
- A slider on a card face takes the drag only when the finger starts on it; swipes anywhere else
  change the page. Horizontal is preferred over vertical for card faces. **Windows may use vertical
  sliders**, as HA's details dialog does for brightness and color temperature.
- **An arranger fills the page** (`layout-engines.md`). It searches the whole page at once, so
  shapes are chosen together, not per group; overflow is reported and never spilled to another
  page. The owner's priority is **no dead space**: "less 'oh no I put too many cards on the page'
  and more 'I need to find an entity to fill this space!'". A group grows into dead space rather
  than a card growing twice as wide or tall (K121); single cards keep a size near today's. Auto is
  the default; locking a layout by hand is not built (K120).
- **Odd member counts**: the short row's members stretch to share the row (5 members in a 3-wide
  group: the 2 in the short row take 1.5 slots each).
- **Area display is uniform**: the card label type (Float, Band, Tag, None) applies to groups as to
  cards.
- Drag-and-drop of entities and groups is wanted; it would change the order or set a pin and run the
  arranger again. Not built.

## 6. Card sizes

Cards default to compact because it is neater, but a 1x1 card that becomes 2x1 or 2x2 (HA's cards
are mostly horizontal) spends a much smaller share of itself on the secondary line, and a light card
could carry a dimmer slider on its face. `card-layout.md` §4 already makes a card draw more when its
cell has room: a bigger span unlocking more content is that rule extended. Who picks a card's size -
the settings file, the glass, or both - is OPEN.

## 7. Page transitions from pre-rendered pictures - not built

Idea: while the board is idle, build each page out of sight and take its picture, so a fresh picture
of every page is ready for a transition: slide, dissolve, shrink / grow, wipe. All four work on
pictures without the full-screen layer that froze the P4_5 (slide and wipe move or clip the picture,
fade sets its opacity, shrink / grow scales it - the P4's PPA accelerates that; on the S3s it is the
effect to cut first). Costs: a card builds in about 7.5 ms plus about 8 ms of layout, so a 15-card
page is ~250 ms of busy `loop()`, done only after an idle period and abandoned when a touch
arrives; a full-screen picture is 1.8 MB of PSRAM on `WS_P4_5`; the picture is a little old when
shown. The snapshot capability (#58) is the enabler for transitions, the overview and screenshots
(`pages.md` §4).

## 8. Navigation, authority, setup

- **Fewer cards per page makes navigation matter more:** a navbar with hot-links to pages or rooms
  (4.3); a card of any size that links to a room page or a function page (a weather page, all the
  house lights).
- **Authority**: the settings file is what every ordinary restart returns to; a **hard reset** wipes
  customizations to factory settings. Backup and Restore (K181) is a milestone of its own.
- **A build sheet is "just a template with entity data and settings"** (owner): applying one at
  compile time has the same effect as importing an exported one. It holds page layouts, settings,
  anything customizable, applied on first boot or by import; the device and the web UI both export
  and import, perhaps at levels ("save all" or "settings only"). **"Restore previous settings when
  flashed"** keeps a user's configuration across a firmware update. The name "build sheet" stays.
  The current configuration is a LittleFS file (`/cfg/settings.json`), so an ordinary upload or OTA,
  which writes only the app partition, leaves it alone; only a full erase or a partition-table
  change loses it.
- **Web UI parity**: the web UI should configure as much as a build sheet holds (#87). Import and
  export go through it.
- **A first-boot setup wizard** like NINA's joins Wi-Fi through a QR code (`reference/`
  `wifi_qr_code.h`, `REFERENCE_PROJECTS.md`), then the web UI manages the device. Not built.

## 9. The card label

The coloured name on a solo card (K42) is a **label** with a type: **Float** (the default; the pill
hangs over the top-left corner and lies on the card, casting a shadow with Linen's shadows),
**Band** (a filled band inside the card), **Tag** (the pill sits on the card's top edge, flush with
its left), **None**. A file-folder tab was designed (the card's left edge runs straight up to the
tab, a rounded inside corner joins it to the top edge) and is not built: LVGL 9 has one radius for
all four corners, so it needs three or four extra objects a card. Details owner-set on glass:

- Gaps are equal on all sides: the card is narrower by the label's overhang, and every width-based
  fit measures the narrower surface; the side overhang is half the top one.
- Status pills (STALE, FAILED, PAUSED) follow the label type: in Float they float on the top edge
  like the label, inset half a radius from the right corner; they stay within the card's visual
  borders. A long label shortens with an ellipsis while a status shows.
- The header band is rounded like the card with a radius-0 "skirt" over its lower half to square the
  bottom, one small object a card. `clip_corner` is never used (`card-layout.md` §1.2).
- Groups never wear a label.

## 10. Linen and the header glyphs

- **Linen**: ground `CFC4B2` (the darker sand), card `F3EEE4`. The owner's complaint was the "on"
  color, the orange that fills an active light or door; "on" is a per-scheme value
  (`ST_ACTIVE`), Linen's `D47F45`, between burnt amber and terracotta, picked on glass from live
  candidates. A very light "on" does not work on a light scheme, and colors come out notably
  different on the device than on a PC monitor (#82), so a color is chosen on glass. The user may
  later pick among earth tones once settings exist (4.1).
- **Cooler colors are harsh on these screens, and the 7B is "wildly more cool" than any other
  screen**: tune its color (init commands or a per-board palette correction) - #82, OPEN.
- **Header glyphs**: a stronger chip and a filled circle are both good; the glyph style is shared
  by the page header and group headers (`header-slots.md`). The clock is a toggleable slot.
- **Status entities in a group header are icon only, like a card's hero, clearly distinct when
  active.** Group headers are clear unless the page's group style is Band (K102, K114).

## 11. Text sizes and uniformity

- **Chrome text** (the page header's words, panel titles, captions) should be derived like the card
  roles, from millimetre targets in `gen_type_scale.py`: titles 2.5 mm, captions 2.0 mm, which would
  fix the 4880 without a per-board multiplier; a device name that does not fit is shortened, never
  wrapped. **Not built for the panels and captions**: `UIToolkit.cpp` still picks one of two fixed
  sets of faces by `bspUiScale() >= 1.3`. The page header's words are derived (`HEADER`,
  `tokens.md` §8). The card area tag target is 1.90 mm; the owner wants it about 2.2 mm (OPEN, #73).
- **Cards should be uniform in font and icon size as much as possible.** One value face per page,
  the largest every value card on it can take with the corner icon counted in the fit, stepping up
  when every card can take it (the 7" roomy pages); icons follow the same rule. Direction, not
  built: a card picks `VALUE` or `VALUE_SM` by its own cell (`card-layout.md` §4), and a solo card
  beside tiles takes the tiles' face (K126).
- Hero value size, as seen on glass: 4B - 4x4 a bit too big, 3x3 could be larger, 3x4 and 4x3 good;
  7" boards - 6x3 "naked", 7x3 fine, 8x3 to 8x4 fits; 4880 landscape - 4x3 excellent, 5x3 too big
  in places.

## 12. Icons

MDI everywhere, no complaints. The owner runs a **custom Philips Hue icon pack in HA** and wants
those icons where an entity uses one (#83, OPEN).

## 13. Reference

The owner's irrigation dashboard in HA is the model for dense, glanceable sections: grouped
sections (each a rounded panel with a title, subtitle and a status pill), tables inside a card (rows
of zones with AM / PM / status columns and a day-of-week row with today highlighted), tall sectioned
dialogs (centred section headings with an italic one-line explanation, stepper time pickers, sliders
with the value on the right), and an event log as a timeline (a dot per event on a vertical line,
the entity's name bold, an `area > device` breadcrumb, state and time on the right). The card
window's breadcrumb and Activity view follow it.

## 14. Settled and open

**Settled**: who looks and from where; the feel rule; motion for the P4s; night mode's shape;
card sizes and the arranger with groups; the label types; Linen; sound on tap is **off by default**,
a per-device setting (#23); close / far viewing distance (#73).

**Open**: the arranger's scoring weights (tuned on glass); the file-folder label on the device
(#80); panel color per board (#82); custom HA icon packs (#83); chrome text's millimetre targets
(#73); who sets a card's size.
