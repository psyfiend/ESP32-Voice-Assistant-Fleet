# Context panels — idea capture, not a design

**Status: captured 2026-09-09, not designed, not scheduled.** Raised by the owner during the
Phase 2.1 discussion. Written down so it is not lost, and so the analysis below does not have to
be redone. Blocks on the `Card` base class (#15, milestone 2.4) and build-sheet schema v1 (#20,
milestone 3.1) — nothing here should be built before both exist.

Related: ROADMAP §4.3 (what happens to the existing panels), Phase 4.4 (context sheets).

---

## The idea, as raised

> Is there a way to leverage the panel concept similar to the core of the build-sheet idea, where
> we create the framework to instantiate a panel — animations, label, size/font/slots — and then
> the panel can be called into existence by filling in the blanks of what the content should be,
> plus some addressing system that says "page 2, left panel"?

With a worked example: a full-screen Weather Forecast view whose bottom-left panel declares
`Title: "Update Frequency" | Row: toggle "Auto-Update" | Row: (if auto-update) radio "1hr, 6hr,
12hr, 24hr"`. Users would be able to design their own context panels and apply them to their own
pages.

And the owner's own objection, which is the sharp part:

> On a page full of disparate cards I'm not sure how the context panels will know which card to
> pull from.

---

## The analysis: that is two different features

The objection dissolves once they are separated. They look alike because both render as an
accordion panel; they differ in **who supplies the binding**, which is the only hard question.

### A. The invoked context sheet — binding comes from the invocation

Long-press a light card, the sheet opens with brightness and colour temperature. Long-press a
media card, transport controls. This is ROADMAP §4.3's proposal and Phase 4.4's milestone.

**There is no addressing problem here.** The sheet is not ambient and does not survey the page
looking for something to bind to — it is *opened by* a specific card, and that card passes its own
entity in. One sheet object, reused, re-populated per invocation. "Which card does it pull from"
has exactly one answer: the one you long-pressed.

What varies is the *interior*, and that is a function of the entity's `kind` — a `light` gets
brightness and colour temp, a `switch` gets an on/off and a history strip. So the interior is
selected the same way a card type is: by kind, with an override available.

### B. The page-slot panel — binding comes from the build sheet

"Page 2, left panel" is a different animal. It is not invoked by anything; it is *part of the
page*, like a card is. Which means it does not need a new addressing system at all — it is
declared in the build sheet next to the cards, with its own entity bindings written there.

The Weather Forecast example is this one, not A. `Update Frequency` is not pulled from a card on
the page; it is a property of the *view*, declared alongside it.

**Consequence worth stating:** if B is "a thing you place on a page with a declared binding," then
B is a card. A tall, panel-shaped, control-bearing card, but structurally a card — same placement
grammar, same binding, same lifecycle. Building it as a separate parallel concept would duplicate
the grid engine, the binding logic and the staleness handling for no gain.

**So: A is a mechanism. B is a card type.** The reusable "panel template" the owner is describing
is real and worth building — it is the *interior* vocabulary that A and B both render into.

---

## The trap to avoid

The pseudocode sketch (`Row: toggle "Auto-Update"`, `Row: radio "1hr, 6hr…"`) is a declarative
grammar for panel interiors. That is genuinely the right instinct — but it must be **the same
grammar the build sheet already uses**, not a second one.

Inventing a second declarative language means two schemas, two parsers, two sets of validation
errors, and two things to document, for one concept. Whatever primitives the build sheet gains for
cards — rows, labels, toggles, sliders, option lists, conditional visibility — are the primitives
panel interiors should be assembled from.

This is why the note at the top says it blocks on schema v1. Designing the panel grammar first
guarantees inventing it twice.

The conditional in the example (`Row: (if auto-update) radio …`) is worth flagging separately: it
is the point at which a build sheet stops being data and starts being a program. Schema v1 should
decide deliberately whether it supports conditional visibility bound to another control's value,
because retrofitting it is a schema break and HANDOFF already records that persisted sheet
references must never silently re-point.

---

## What happens to `Panel_Audio` and `Panel_Display` meanwhile

**Nothing, deliberately.** They stay exactly as they are.

They are currently the only working UI in the project, which makes them the regression baseline
for Phase 2.1 — "does the board still behave identically" is only answerable while they exist
unchanged. Their fate is already decided (ROADMAP §4.3): they become settings pages in Phase 4.1,
and the accordion mechanism inside `UIToolkit` becomes the context sheet in Phase 4.4. Their
use-case-specific content — audio test controls, touch-point visualiser — is genuinely
developer-facing and there is no reason to delete it; a settings page is a fine home for it.

The part worth keeping is the accordion *mechanism*, not the panels' contents. That is already
in `UIToolkit`, not in `Panel_Audio`, which is why nothing needs to move yet.
