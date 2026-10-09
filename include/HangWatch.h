#pragma once
//
// HangWatch - where is the UI thread stuck? (2.10d, -D DEBUG_HANG only)
//
// The owner had WS_P4_5 freeze twice in a card window (2026-10-09): picture
// stopped mid-animation, /screenshot timing out, the HTTP side alive. That is
// loopTask - where LVGL runs - either spinning or blocked, and nothing says
// which or where.
//
// A watcher task on the other core sees loop() stop beating for STALL_MS, then
// stops loopTask (vTaskSuspend saves its registers), reads where it was - the
// program counter, the return address and every code address left on its stack
// - lets it go again, and does that three times, so a loop shows as the same
// few addresses. The result goes to the serial log and to
//
//   http://<board>/hang
//
// to be turned into function names with addr2line against that build's
// firmware.elf. One report per freeze; the board is left frozen, as found.
//
class HttpServer;

namespace HangWatch {
void begin(HttpServer &http);   // from loopTask (SystemCore::begin())
void beat();                    // every loop()
}
