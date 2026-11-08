# Context panels - an idea, not a design

**Status: not designed, not scheduled.** ROADMAP Phase 4.4 (#26) holds it. The analysis below is
kept so it is not redone. The Sheet's folder panels (`Kit::Sheet`, `components.md`) already do half
of it: the card window's SETTINGS and view panels are opened by a card and filled from that card's
entity.

## The idea

A framework to instantiate a panel - animations, label, size, font, slots - that is called into
existence by filling in what the content should be, plus an addressing system that says "page 2,
left panel". The worked example is a full-screen Weather Forecast view whose bottom-left panel
declares `Title: "Update Frequency" | Row: toggle "Auto-Update" | Row: (if auto-update) radio "1hr,
6hr, 12hr, 24hr"`. Users would design their own context panels and apply them to their own pages.

The owner's objection: on a page full of disparate cards, how do the context panels know which card
to pull from?

## Two different features

Both render as an accordion panel; they differ in **who supplies the binding**, which is the only
hard question.

### A. The invoked context sheet - binding comes from the invocation

Long-press a light card and the sheet opens with brightness and color temperature; long-press a
media card and it shows transport controls. There is no addressing problem: the sheet is opened by
a specific card, and that card passes its own entity in. "Which card does it pull from" has one
answer: the one you long-pressed. What varies is the interior, a function of the entity's `kind`
(a `light` gets brightness and color temperature, a `switch` an on/off and a history strip), chosen
the way a card type is: by kind, with an override available.

### B. The page-slot panel - binding comes from the page's data

"Page 2, left panel" is not invoked by anything; it is part of the page, like a card. It needs no
new addressing system: it is declared next to the cards with its own entity bindings. In the Weather
Forecast example, `Update Frequency` is a property of the *view*, not pulled from a card. A thing
placed on a page with a declared binding is a card (tall, panel-shaped, control-bearing), with the
same placement, binding and lifecycle; building it as a parallel concept would duplicate the layout
engine, the binding logic and the staleness handling.

**A is a mechanism. B is a card type.** The reusable "panel template" is the *interior*
vocabulary both render into.

## The constraint

A declarative grammar for panel interiors (`Row: toggle "Auto-Update"`) must be **the same grammar
the settings file and card specs already use**, not a second one: two schemas, two parsers and two
sets of validation errors for one concept. Whatever primitives cards gain (rows, labels, toggles,
sliders, option lists, conditional visibility) are the primitives panel interiors are assembled
from.

A conditional row (`Row: (if auto-update) radio ...`) is the point at which a spec stops being data
and starts being a program. Decide deliberately whether conditional visibility bound to another
control's value is supported, because retrofitting it is a schema break, and saved references must
never silently re-point.

## `Panel_Audio` and `Panel_Display`

They stay as they are: developer-facing content (audio test controls, the touch-point visualiser)
that is the regression baseline for the UI. The accordion mechanism in `UIToolkit` is the part
worth keeping for a context sheet.
