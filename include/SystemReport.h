#pragma once
//
// SystemReport - the System Doctor.
//
// The report writes to registered sinks and knows nothing about LVGL; the UI
// registers itself as a sink rather than being reached into.
//
// That gives three things:
//   - the report works with no GUI present (Serial is the default sink),
//   - the Phase 4 Settings -> System Info page registers a sink and gets the
//     whole report without a second copy of the formatting,
//   - this file includes no LVGL header, same rule as SystemCore.
//
// See docs/design/startup.md section 3.4.
//
#include <Arduino.h>

class SystemCore;

namespace SystemReport {

// One formatted line of report output.
using Sink = void (*)(const char *line);

// A caller-supplied extra section. Called during run(), between the built-in
// sections and the I2C scan, and should emit through line(). Used by GUIManager
// for [UI STATE], the one genuinely LVGL-dependent part of the report.
using Section = void (*)(void);

// Returns false if the (small, fixed) sink or section table is full.
bool addSink(Sink s);
bool addSection(const char *name, Section fill);

// Emit one line to every registered sink, and to Serial if this run is
// echoing. Sections call this.
void line(const char *fmt, ...);

// Format a byte count as "<exact> bytes (<human> KB|MB)".
//
// One convention, used everywhere memory is reported. Exact bytes come first
// because that is the number you diff between two runs to spot a regression;
// the human-readable form is a convenience in parentheses, never a
// replacement. Values under 1 KB print as bytes alone.
//
// The unit is "KB"; a lower-case b would be bits.
//
// Returns `out` so it can be used inline in a printf argument list.
const char *fmtBytes(uint64_t bytes, char *out, size_t outLen);

// Walk everything and emit it.
//
// echoSerial is off for the automatic boot
// run (whose content largely duplicates the boot-time Serial prints the
// managers already produce), on for a manually-triggered run such as the
// System panel's "Dump Config" button. -D DUMP_CONFIG forces it on.
//
// With no sinks registered at all - a GUI-less build - Serial echo is forced
// on regardless, so the report is never silently swallowed.
void run(SystemCore &core, bool echoSerial);

} // namespace SystemReport
