# Changelog

Versions are `v0.<phase>.<release>`; a build adds the commits since that tag, and `+dirty` for a
modified tree (`v0.2.8.14+dirty`). The phase follows the roadmap. The release number counts tagged,
signed-off merges to `main` in the order they happen - it does not follow milestone numbers, which
began finishing out of order (decided 2026-10-05; `docs/ROADMAP.md` 3.3). This file is where a
version is matched to its milestones.

## v0.2.8 - not yet tagged: the 2.10a merge

- **2.10a, the card popup (#65)**: long press (250 ms) opens a window on every card - usable in its
  first frame, closed by the X, a tap outside, a drag down on its title or 60 s untouched; the card
  pressed in with an accent border while held and while its window is open; a switch's toggle that
  slides and can be dragged; a settings deck with Pause; sparks along the window's edge.
- Already on `main` since v0.2.7, untagged until now: `GET /screenshot` (#58); 2.9's esp_lcd display
  stack on six of nine boards, `GET /bench` (#67); overlay layers that never scroll (#68); the
  battery prototype (#72); wall-clock time from SNTP (#74); the floating tag (#80); Linen round two;
  the README and MIT licence; design interview sections 1 and 2.

## v0.2.7 - 2026-09-24

- Through milestone 2.7 - the last tag whose number was also a milestone's.
