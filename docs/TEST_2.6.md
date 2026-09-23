# Test sheet — milestone 2.6, horizontal pages (plus 2.7 round four)

Branch `feat/17-page-swipes`, cut from the tip of `feat/18-card-types`, so it carries 2.7 too.
`feat/18-card-types` stays at 2.7 round three and can merge on its own. Flashed to `CYD_S3_3248`
(COM10) and `WS_P4_5` (COM15). Design: `docs/design/pages.md`.

Every board now has two pages, **House** then **Fleet**. `-D USE_HA_DASHBOARD` is gone.

---

## Pages

**P1 — Swipe.** Swipe left anywhere on the cards.
- PASS: Fleet appears; the header's centre reads "Fleet" with the second dot lit; a toast
  "Fleet  -  2 of 2" shows for about 1.5 s. Swipe right: back to House.
- FAIL: nothing happens, the wrong page, or a tap fires on the card under your finger.

**P2 — Wrap.** On Fleet, swipe left again.
- PASS: House (wrapped). On House, swipe right: Fleet.

**P3 — Where a swipe must NOT turn the page.**
- Drag a deck slider sideways, fast: the slider moves, the page does not.
- With the drawer open, swipe sideways: nothing (the drawer stays, the page stays).
- During a header peek: same.

**P4 — Per-page knobs.** On House set, say, Tag, Icon, No Area, Col 4, Linen. Swipe to Fleet.
- PASS: Fleet still looks the way it did, and the drawer's labels describe Fleet. Swipe back:
  House is exactly as you left it, labels included.
- **Deck and Hide Bar are NOT per page**: hide the bar on House, swipe - it stays hidden.

**P5 — The header.** Device name still on the left, page title and dots centred, status glyphs on
the right. On the 3248 at 320 px, check nothing overlaps.

**P6 — Toast position.** On the boards where it was wrong (7B right-hand side, S3_4B half-way down)
the page-change toast should now be top-centre. **Unverified** - neither is on these two benches.

**P7 — Reboot.** Always boots to House, whatever page you left.

## The test cards (on Fleet, top of the page)

**T15 — Unavailable.** Tap **Avail** (Test area) off.
- PASS: **Reading** shows N/A with the diagonal within a second or two. Tap Avail on: it recovers.
  First time `ST_UNAVAILABLE` has been seen on hardware.

**R9/T10 — Refused.** Tap **Refuse**.
- PASS: it lights immediately (optimistic), then about 3 s later springs back with FAILED.

## Round two of 2.6 — flashed 2026-09-23

First-round results: P1-P5, P7, R9 PASS; P5 overlap on the 3248; P6 FAIL (toast against the right
edge); T15 partial (diagonal misplaced after reboot); C1 FAIL on the P4_5; C2 PASS; C3 partial.

**Q1 — Toast.** Centred at the top on both benches. (Cause: x was an offset from the centre.)
**Q2 — 3248 header.** No overlap: on a bar too narrow for both, the device name gives way and the
page title and dots sit at the left.
**Q3 — Dots.** Current page a larger filled dot; the others smaller hollow rings. Tell them apart
without relying on colour.
**Q4 — T15 again.** Leave Reading N/A, reboot, swipe to Fleet: the diagonal runs corner to corner.
(Cause: a fallback computed the card twice its real size.)
**Q5 — Corner size.** Now by the card's physical HEIGHT (17.5 mm and up = larger). P4_5: back to
the small ones. 7B, when next flashed: large at 3 rows with or without header and deck, small at 4.
**Q6 — Linen shadow, properly this time.** A shadow along the bottom edge and down the sides of every
card, not just in the corners. (Cause: the wrapper's clip margin was zero, see Card.cpp.)

## 2.7 round four

**C1 — Corner size by rows.** 3 rows or fewer: the larger corner icons, whatever the columns
(Col 7, Col 8 included). Row 4 and up: the smaller. Now applies on every board, so the P4_5 at
5x3 and the 4B pair at 4x3 get the larger ones too - judge those.

**C2 — Drawer buttons.** Crisp corners top and bottom, no light glint at the bottom corners on the
dark schemes, and a press no longer clips the button's top and bottom edges (it darkens instead).

**C3 — Linen shadow.** Tighter and darker than round three: a short, firm drop below each card
rather than a soft fuzz. Border darker again.
