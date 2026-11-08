#pragma once
// "About this board": a Sheet client (K74) - a small read-only sheet saying
// what this board is, what it runs and how it is doing. Every build: the system
// panel's About button opens it; debug builds can also open it from a PC,
// GET /popup?about=1 (about=0 closes it).
// docs/design/components.md, "Sheet".
namespace AboutSheet {
bool open();    // false when a sheet is already showing
void close();
bool isOpen();
}
