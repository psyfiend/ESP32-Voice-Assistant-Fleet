#pragma once
// "About this board" (2.12 P4): the Sheet's second client - a small read-only
// sheet saying what this board is, what it runs and how it is doing. Debug
// builds only (DEBUG_POPUP) until the owner says where it belongs: GET
// /popup?about=1 opens it, about=0 closes it. docs/design/components.md, "Sheet".
namespace AboutSheet {
bool open();    // false when a sheet is already showing
void close();
bool isOpen();
}
