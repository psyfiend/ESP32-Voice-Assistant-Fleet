# GUI Framework — superseded

**This document has been superseded by `ROADMAP.md`.** Its vision sketch — a layered UI with
a data-source abstraction, reusable cards and a page/grid engine — was folded into the
roadmap's target architecture (§4) and Phases 2–4, where it is now specified in far more
detail and against real decisions rather than as a sketch.

Where its content went:

| Was here | Now |
|---|---|
| The layered UI vision | `ROADMAP.md` §4 (target architecture) and Phase 2 |
| The data-source abstraction | Merged with the HA-discovery entity registry into **one** object — `ROADMAP.md` §4.1, built as `Fleet_Entities` |
| "Roughly how the layers fit together" | `ROADMAP.md` §4, plus the Fleet Data Architecture artifact |
| Where the UI is today | `HARDWARE_STATUS.md` (per-board) and `FUTURE_IMPROVEMENTS.md` (startup reorganisation) |

Two things are kept here because they have no better home yet.

---

## Why build this rather than adopt ESPHome or an existing dashboard

The reason is **personal, not technical**, and that is a sufficient reason on its own. This is
a hands-on learning project; skipping to a mature framework forecloses the part that is
actually the point — understanding the thing by building it.

It is worth being explicit about that, because the technical case for adopting something
off-the-shelf is perfectly good and will keep presenting itself. The project is already
compatible in spirit with the HA-side automation it talks to.

A from-scratch rewrite of the *voice-assistant* side would be a separate and much larger
"adopt vs build" question — see the AEC entry in `FUTURE_IMPROVEMENTS.md`. Not this one.

## Strategic UI event logging

Worth placing serial output at deliberate points in UI code — taps, panel open/close, page
transitions — for when GUI work gets deep enough that visual-only feedback cannot show what
is happening. Same spirit as the existing `DEBUG_<AREA>` convention in `CLAUDE.md`, and it
should use that mechanism (`DEBUG_UI`) rather than inventing another.

Trigger points deliberately not decided. Revisit when it would actually be useful rather than
designing it speculatively — most likely during Phase 2.6, where §5.2 flags gesture conflicts
as a real risk and "why did that swipe do nothing" is exactly the question this would answer.
