#pragma once
#ifndef LOG_PAGE_H
#define LOG_PAGE_H

#include <functional>

// ---------------------------------------------------------------------------
// LogPage - the System Doctor's output, on its own screen.
//
// WHY IT MOVED OUT OF THE SYSTEM PANEL. The panel is an accordion that
// animates its own height, and it had a scrollable log box nested inside it.
// The deck's Audio/Display panels animate the same way over the whole card
// grid and are smooth; the System panel has always been choppy. The difference
// is the nested scroller: LVGL re-lays-out and re-clips that subtree on every
// frame of the height animation, which is work the deck panels never do.
//
// Moving the log to its own screen makes the panel a plain box again, and
// gives the buttons room to breathe - on CYD_S3_3248 they had been squeezed to
// the point of being unreadable.
//
// Same shape as ReferencePage and CardDemo, deliberately: its own screen,
// restores the previous one on Back, and the dashboard stands down before it
// opens. That last part is not optional - two full widget trees at once is
// what exhausts LVGL's layer buffers and hangs the board, which this project
// has now been bitten by three times.
// ---------------------------------------------------------------------------

namespace LogPage {

// text is the accumulated report, owned by the caller and only read here.
void show(const char *text);

// What the "Dump" button runs - the full System Doctor, re-rendered. Registered
// by GUIManager, which is the only thing that can reach SystemCore.
void setDumpHandler(std::function<void()> cb);

// What the "Clear" button runs. The log is a tail that keeps growing, so
// without this a fresh dump lands underneath the previous three and the thing
// you asked for is the part scrolled off the bottom.
void setClearHandler(std::function<void()> cb);

// Run when the page closes, so the dashboard can be rebuilt.
void setCloseHandler(std::function<void()> cb);

// Replace what is on screen, if the page is open. Called after a fresh dump.
void refresh(const char *text);

void close();

} // namespace LogPage

#endif // LOG_PAGE_H
