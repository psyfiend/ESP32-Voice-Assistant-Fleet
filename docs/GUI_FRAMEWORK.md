# GUI Framework — Vision & Architecture

This is the design/intent doc for the fleet's UI layer specifically — not the hardware HAL
(see `CLAUDE.md`), not per-board bug/test status (`PROJECT_STATUS.md`), and not general
architectural deferrals (`FUTURE_IMPROVEMENTS.md`, which still owns the *hardware*-side
generic-capability-pattern item and the MQTT/HA component item this doc leans on heavily).
Nothing in this file is a committed plan — it's intent and direction, written down so it
isn't lost, to be refined once real design work starts.

## Where the UI is today

`src/_Common/` (`LVGL_Test_UI.cpp` + `Panel_Header/Display/Audio/System.cpp`) is a hand-built
LVGL dashboard: a header bar, a bottom "deck" of panels, and a slide-out system/diagnostics
drawer. It's shared across every board via `build_src_filter`, and per-board differences are
handled the way the HAL handles them — `#ifdef HAS_X` gating scattered through the UI code
(`Panel_Audio.cpp` hiding widgets a board's codec can't support, `HAS_AUDIO_HW` now excluding
the whole audio panel for boards with no audio hardware at all, etc.).

This works and isn't wrong for what it's been so far — a hardware bring-up/test harness, one
audio panel and one display panel, a small number of boards. It doesn't scale past that: every
new sensor, every new board-specific widget, every combination of "this board has A and C but
not B" is another `#ifdef` fork in UI code that has to be hand-written and hand-tested. The
same friction that prompted `HAS_AUDIO_HW` this session (a capability that needed a real
code-path split, not just a hidden widget) will keep recurring as more boards and more
peripherals (IMU, RTC, power management, GPIO relays, RS485 sensors, MQTT-sourced data with no
local sensor at all) get real UI surfaces.

## The vision

A declarative, data-source-agnostic dashboard framework, roughly three layers:

**1. Layout templates.** A semi-readable description of screen structure — the kind of thing
sketched in conversation as *"3 column display, 2 groups per column, 2 sub-rows per group"* —
rather than hand-laid-out LVGL flex/grid code per board. Home Assistant's card-grid dashboards
are the reference point: titled boxes/cards, each with sub-items (title, subtitle, value),
sized and arranged declaratively instead of imperatively.

**2. A generic expanding-panel mechanism.** The existing system drawer (`Panel_System.cpp`,
slides up from the bottom, has a title, gets toggled from the header) is already halfway to
this — the goal is to generalize it so *any* panel is "give it a title, then populate it with
however many features/cards apply to this build," instead of the drawer being one hardcoded
special case and every other panel being its own bespoke widget tree.

**3. Data-source abstraction.** A card doesn't know or care whether its value comes from a
sensor physically wired to *this* board, or from an MQTT topic describing a sensor on some
other device entirely. Once the MQTT/Home-Assistant component from `FUTURE_IMPROVEMENTS.md`
exists, "local peripheral" and "remote HA entity" become two implementations of the same
data-source interface behind the card renderer — the UI layer stops needing to know which one
it's looking at. This is the part of the vision that most changes what "a board" even means:
a device with zero physical sensors attached could still have a full dashboard, entirely
MQTT-driven.

A natural consequence of layer 3: card **visibility** becomes runtime/user state, not just
build-time capability. A settings-style toggle menu — auto-populated from whatever cards a
given build's manifest makes *available* — lets a card be compiled in and configured, but
hidden by the user, independent of the `HAS_X`-style question of whether the hardware exists
at all. Those are two different questions (can this board show X at all vs. is the user
currently choosing to see it) that the current all-or-nothing `#ifdef` approach can't
distinguish.

## Roughly how the layers might fit together

Not designed yet, but the shape that falls out of the above:

- Some **manifest/registry** (compile-time list, or table, of "which cards exist and what
  feeds them") replaces scattered `#ifdef HAS_X` checks inside UI `.cpp` files. The HAL side's
  own generic-capability-pattern item (`FUTURE_IMPROVEMENTS.md`'s "Generic capability pattern
  for extraneous sensors/peripherals") is a natural upstream input to this — a manager class +
  `HAS_X` flag pair per peripheral becomes one manifest entry with a matching data-source
  binding.
- A **binding layer** maps each manifest entry's declared data source to either a local
  read function (a manager class getter, same shape as `AudioManager::getVolume()` today) or
  an MQTT subscription/topic, and the card rendering code only ever talks to that binding, not
  to `AudioManager` or `Wire` or an MQTT client directly.
- The **layout template** consumes the manifest (only showing what's both available and not
  user-hidden) and lays cards out per whatever grid/column spec applies to that panel.

## Why not just use ESPHome / an existing dashboard framework

Already fully compatible in spirit with the HA-side automation this fleet talks to (the
sprinkler-controller work in `FUTURE_IMPROVEMENTS.md` is explicitly built to match ESPHome's
Sprinkler component). The reason to build this rather than adopt something off-the-shelf is
personal, not technical: this is a hands-on learning project, and skipping straight to a
mature framework forecloses the part that's actually the point — understanding the thing by
building it. That's a valid reason on its own and doesn't need a technical justification to
stand. If a from-scratch rewrite of the whole voice-assistant side ever happens (see
`FUTURE_IMPROVEMENTS.md`'s AEC entry), that's a separate, larger "adopt vs. build" question —
not this one.

## Debugging aid worth considering: strategic UI event logging

Not part of the layered vision above — a smaller, more immediate idea flagged while wrapping
up the `WS_S3_TOUCH_LCD_5B` bring-up. Worth placing serial output at strategic points in UI
code (taps, panel open/close, etc.) as a debugging aid for whenever GUI design work gets deep
enough that visual-only feedback isn't enough to trace what's happening. Exact trigger points
not decided yet — revisit when it'd actually be useful rather than designing it speculatively
now. Same spirit as the existing `DEBUG_<AREA>` convention (see `CLAUDE.md`), just not
scoped out yet.

## Status

Vision only — no design decisions made, no code started. Not urgent (current `#ifdef`-based
UI still works fine at the current scale), but worth designing deliberately before more
one-off panels/widgets accumulate and make a later migration harder. Revisit once the MQTT/HA
component (`FUTURE_IMPROVEMENTS.md`) has real shape, since layer 3 above depends on it
directly and will likely drive some of layers 1 and 2's actual design.
