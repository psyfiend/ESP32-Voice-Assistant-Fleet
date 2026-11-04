# What we built in 2.13 so far - a recap for Eric

Written 2026-11-04, at the end of the long session that ran from 2026-11-02 to 11-04. This is the
readable version: what changed on the boards, what changed in Home Assistant, and what changed in
how we work. The exact record lives in `DECISIONS.md` (K138-K164), `TEST_2.13.md` (the four rounds)
and `docs/design/ha-cards.md` §4. What is left is `PLAN_2.13.md`. When 2.13 merges, this file is
where the `CHANGELOG.md` entry comes from.

**In numbers:** 29 commits on `feat/2.13-add-from-ha`, four test rounds, 27 decisions. The
project's own code went from 64,629 lines in 260 files (`v0.2.20`) to **69,478 lines in 277
files**. 78 host tests pass.

---

## Just before: the 2.11 follow-ups (`v0.2.20`, merged 2026-11-01)

Tested on the laptop with the P4_5, merged and tagged when you were home. Moving a card now uses
your words (Hide from this page, Change group, Add to group, and so on), each move picks a target,
then Copy or Move, then a confirmation. **Restore** became the one way back from any move. An
emptied cluster hides itself, and Members shows HA's full names.

## On the boards

### Round 1 (2026-11-02, built overnight while you slept)

- **The boards can follow as many HA entities as you like.** HA refused a subscription bigger
  than about 45 entities, so the board now asks in parts. The registry holds 192 entities, and
  cards you add from HA are kept in the settings file, so they come back after a reboot.
- **The board knows what HA offers.** It reads every entity you labelled `fleet`, by area and by
  floor, and notices new ones: a notice "New in Office: 3D Printer" and a "1 new" mark. Nothing is
  ever added by itself (K133).
- **Notices queue** instead of replacing each other, each with a bar that runs down - the NINA
  toast idea, info notices only for now. Alerts join the same queue in 2.14.
- **Adding cards from HA**: to a cluster, to the page, or a whole new cluster from an HA area. A
  cluster can be linked to its HA area.
- **The first group view** - a tap on a cluster opens a window of all its members.
- **The thermostat** as a read-only card and window (mode, target, what it is doing, History).
- **Show helper text** in UI Tweaks, and a cluster's own card state for its tiles.

### Round 2 (2026-11-03, overnight)

- **CLUSTER became GROUP**, and **Rename group renames the group** - every card in it follows, and
  its color stays.
- **Edit cards** replaced all the separate "add" screens: one window over the page, with a column
  of lists - In this group, On the page, Hidden, then HA's areas. **The lists show the result while
  you tick**: dashed rows are coming, red rows are going.
- **Every choice comes before the confirmation**, which is only go or no-go, in the same sentences
  as the move dialogs (your rule: one tap never answers two questions).
- **Show as** (Cluster or Solo card) and **Face** for a group; **Long press opens** for every card
  (never Toggle, so SETTINGS is always one long press away).
- **Names fit their room**: a long name steps down a font size before it is cut, after NINA's
  font ladder.
- **Label and group got separate words** (K146, K158): a page card has a *label*, a *group* is the
  bordered block.

### Round 3 (2026-11-03 into 11-04)

- **The group view became the cluster itself** - its own header and real tiles that tap and
  long-press like on the page. No title bar and no X; a tap outside closes it.
- **A group keeps its GROUP panel however it is shown**, and a row that does not apply is greyed,
  never moved (your rule, now in memory as "options never move").
- **Tap and long press are kept separately** for a group shown as a cluster and as a solo card.
- **Desk inside Office opens as Desk does on the page** - "Office > Desk", with its own Members.
- Fixes after you looked: tiles take taps, **the drop shadow is no longer clipped** (a new LVGL
  lesson in `LESSONS.md`), and **tiles have a maximum size**: 25 x 20 mm, never more than twice as
  wide as tall, all one size.

### Round 4 (2026-11-04)

- **Pinned members.** A group is now a loose set of members, and the page shows the ones you
  **pin**. New members join unpinned, so adding never rearranges the page. Members and Edit cards
  show a pin (or the header glyph) in a square beside each pinned member. Visibility says **Pin
  this card** / **Unpin this card**.
- **Edit cards' tap cycle** for a group: tap to flip a pin, tap again to mark a card for removal,
  tap a third time to put it back. One **Save N** button, then "Saved N changes to Kitchen".
  Double-tapping a card that is not in the group yet adds it **and** pins it.
- **The Table bug is fixed**: Table shows under On the page again, and adding it sends it home.
- **Tiles or Cards** for the group view, in UI Tweaks and per group in Card UI.
- **The spare room is spread evenly** around and between the tiles.
- **The header sensors in the group view** drop a panel listing each one; a row opens its History.
- The system panel's Grid line says **fixed** for a held column or row count. The old idea of
  pinning a card to a grid cell is shelved in `FUTURE_IMPROVEMENTS.md`.

### Designed, for later

- **Alerts (2.14)**, from the "2.13 Alert Flow Mocks": one parent alert per rule with its causes,
  "About this alert", Acknowledge and "Quiet for 30 min", phone notifications with actions, and a
  card framed in the alert's color when its entity is a cause (K138, K139).
- **Voice and the ESPHome API** (A18, ROADMAP Phase 7): the boards speaking ESPHome's native API
  from our own firmware, so HA adopts them and voice goes to Assist. Optional, beside MQTT.

## In Home Assistant

- **Changed:** only wording, on 2026-11-02 - `FLEET_REFERENCE.md` rule 3 and `HOUSE_RULES.md` §7
  now say HA *offers* and you choose (K133). HA commit `d8293c5`; logged in
  `projects/2026-10_fleet-2.13/README.md`.
- **Looked at, not changed:**
  - the Security dashboard: its picker offers only security-class entities, so problem and
    battery alerts will need their own view;
  - there are no `alert.*` entities yet;
  - a few doors appear twice, probably leftovers from the renames;
  - "door left open" timers reset when HA restarts.
- **Decided, not built yet:**
  - the YAML conversion (K136): your logical and status layers as hand-written files, like the
    sprinklers package;
  - the alert anatomy (K138).

## How we work - the dev environment

- **Unattended overnight builds**, with your permission: a watchdog job restarted the work after a
  usage limit, and was removed afterwards.
- **Before every flash, each board's settings are saved, and checked afterwards with "0
  differences".** The tools are `save_settings.ps1`, `restore_settings.ps1` and `setdiff.py`.
- **New PC handles** (debug builds only) let Claude drive Edit cards, New group and the group
  view's drop-down from the PC and take screenshots. They are listed in `HTTP_REFERENCE.md`.
- **Mocks as artifacts first**: "2.13 Alert Flow Mocks", "2.13 Add Cards Mocks" and "2.13 Round 2
  Mocks" (17 versions).
- **Home Assistant through ha-mcp**, with its best-practices guide read before any change. Your
  sign-in to HA happens in the browser pane, by you.
- **Read-only helper agents** checked the docs against the code for this handoff. Many design
  documents were brought up to date.
- **New documents:**
  - `design/ha-cards.md` - 2.13's design and what it does now;
  - `design/voice-esphome.md`;
  - `TEST_2.13.md`;
  - `PLAN_2.13.md` - the rest of 2.13, in order;
  - this recap.
- The icon font gained a pin, and the host tests gained the group view's tile grid.
