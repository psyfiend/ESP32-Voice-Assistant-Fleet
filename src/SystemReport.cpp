#include "SystemReport.h"
#include "SystemCore.h"
#include <FleetI2C.h>
#include "bsp_loader.h"

// FW_VERSION/FW_COMMIT are injected from `git describe` by
// scripts/fw_version.py; the fallbacks keep the build working if that hook is
// ever skipped.
#ifndef FW_VERSION
    #define FW_VERSION "unknown"
#endif
#ifndef FW_COMMIT
    #define FW_COMMIT "unknown"
#endif

namespace {

constexpr uint8_t MAX_SINKS    = 4;
constexpr uint8_t MAX_SECTIONS = 4;

SystemReport::Sink s_sinks[MAX_SINKS] = {};
uint8_t            s_sinkCount = 0;

struct SectionEntry { const char *name; SystemReport::Section fill; };
SectionEntry s_sections[MAX_SECTIONS] = {};
uint8_t      s_sectionCount = 0;

bool s_echoSerial = false;

// Helper to check ESP32 GPIO registers directly.
bool isGpioOutput(int pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) return false;
    if (pin < 32) return (REG_READ(GPIO_ENABLE_REG) & (1ULL << pin));
    else          return (REG_READ(GPIO_ENABLE1_REG) & (1ULL << (pin - 32)));
}

void reportConnectivity(SystemCore &core) {
    char cbuf[64];
    ConnectivityManager &conn = core.conn();
    MqttManager         &mqtt = core.mqtt();

    SystemReport::line("[CONNECTIVITY]");
    SystemReport::line("  Mode: %s", connModeName(conn.getMode()));
    SystemReport::line("  State: %s (%s)", connStateName(conn.getState()),
                                           linkTypeName(conn.getLinkType()));
    conn.getHostname(cbuf, sizeof(cbuf));
    SystemReport::line("  Hostname: %s", cbuf);
    SystemReport::line("  Device ID: %s", DeviceIdentity::deviceId());
    if (conn.isOnline()) {
        conn.getSsid(cbuf, sizeof(cbuf));
        SystemReport::line("  SSID: %s", cbuf);
        SystemReport::line("  IP: %s", conn.getIP().toString().c_str());
        SystemReport::line("  RSSI: %d dBm", (int)conn.getRssi());
    } else {
        SystemReport::line("  Offline (last reason: %u)", conn.getLastDisconnectReason());
    }
    if (conn.isApActive()) {
        SystemReport::line("  AP: %s / %s", conn.getApSsid(), conn.getApPassword());
        SystemReport::line("  AP IP: %s (%u client(s))",
                           conn.getApIP().toString().c_str(), conn.getApClientCount());
    }

    SystemReport::line("[MQTT]");
    SystemReport::line("  State: %s", mqttStateName(mqtt.getState()));
    if (mqtt.getState() != MqttState::SESSION_OFF) {
        SystemReport::line("  Base topic: %s", mqtt.getBaseTopic());
        if (!mqtt.isConnected()) {
            SystemReport::line("  Last failure: %s", mqttFailureName(mqtt.getLastFailure()));
            uint32_t s = mqtt.secondsUntilRetry();
            if (s) SystemReport::line("  Retry in: %lu s", (unsigned long)s);
        }
    }
}

void reportEntities(SystemCore &core) {
    EntityRegistry &entities = core.entities();

    // Whatever the providers have registered so far. Empty until they run,
    // which is itself the useful signal if a provider fails to start.
    SystemReport::line("[ENTITIES] %u registered", (unsigned)entities.count());

    const uint32_t now = millis();
    for (uint8_t i = 0; i < entities.count(); i++) {
        const Entity *e = entities.at(i);
        if (!e) continue;

        char val[56];
        switch (e->value.type) {
            case ValueType::BOOL:     snprintf(val, sizeof(val), "%s", e->value.b ? "on" : "off"); break;
            case ValueType::INT:      snprintf(val, sizeof(val), "%ld", (long)e->value.i); break;
            case ValueType::FLOAT:    snprintf(val, sizeof(val), "%.2f", e->value.f); break;
            case ValueType::TEXT_VAL: snprintf(val, sizeof(val), "%s", e->value.text); break;
            default:                  snprintf(val, sizeof(val), "-"); break;
        }

        SystemReport::line("  %s = %s%s%s%s", e->desc.id, val,
                           e->desc.unit[0] ? " " : "", e->desc.unit,
                           entities.isStale(*e, now) ? "  (stale)" : "");
    }
}

void reportHardware(SystemCore &core) {
    char b[48];

    SystemReport::line("[HARDWARE STATUS]");
    SystemReport::line("  I2C Backend: %s", FleetI2C::backendName());
    SystemReport::line("  Uptime: %lu ms", millis());
    // Same labels and same units as the boot banner in SystemCore, on purpose:
    // these are the same quantities and used to be reported three different
    // ways.
    SystemReport::line("  Free Heap: %s", SystemReport::fmtBytes(ESP.getFreeHeap(), b, sizeof(b)));
    SystemReport::line("  PSRAM: %s",     SystemReport::fmtBytes(ESP.getPsramSize(), b, sizeof(b)));
    SystemReport::line("  Flash: %s",     SystemReport::fmtBytes(ESP.getFlashChipSize(), b, sizeof(b)));
    #ifdef HAS_IO_EXPANDER
        SystemReport::line("  Pin Expander: Active (TCA9554/Similar)");
    #else
        SystemReport::line("  Pin Expander: None (Native GPIOs)");
    #endif
    #ifdef HAS_BUTTON
        SystemReport::line("  Button: ENABLED");
    #else
        SystemReport::line("  Button: NOT PRESENT");
    #endif

    SystemReport::line("[AUDIO]");
    #ifdef HAS_AUDIO_HW
        #ifdef HAS_ES8311
            SystemReport::line("  Codec ES8311: ENABLED");
        #else
            SystemReport::line("  Codec ES8311: NOT PRESENT");
        #endif
        #ifdef HAS_ES7210
            SystemReport::line("  ADC ES7210: ENABLED");
        #else
            SystemReport::line("  ADC ES7210: NOT PRESENT");
        #endif
        SystemReport::line("  Volume: %d%%", core.audio().getVolume());
        SystemReport::line("  Muted: %s", core.audio().getMute() ? "YES" : "NO");
    #else
        (void)core;
        SystemReport::line("  No audio hardware on this board");
    #endif

    // AMP pin diagnostics
    if (bsp_hw.I2S_AMP_EN != -1) {
        #ifdef HAS_IO_EXPANDER
            // If checking an Expander pin, we can't check ESP32 registers.
            // We rely on the fact that we wrote to it.
            SystemReport::line("AMP Driver: VIA IO EXPANDER (Assumed OUTPUT)");
        #else
            SystemReport::line("  Amp Pin: GPIO %d", bsp_hw.I2S_AMP_EN);
            bool isOut = isGpioOutput(bsp_hw.I2S_AMP_EN);
            SystemReport::line("  AMP Driver: %s", isOut ? "OUTPUT (OK)" : "INPUT/HI-Z (ERROR!)");
        #endif
        SystemReport::line("  AMP Pin Level: %s",
                           digitalRead(bsp_hw.I2S_AMP_EN) ? "HIGH (ON)" : "LOW (OFF)");
    } else {
        SystemReport::line("  Amp Pin: UNDEFINED");
    }
}

void reportDisplay(SystemCore &core) {
    SystemReport::line("[DISPLAY]");
    #ifdef HAS_RGB_PANEL
        SystemReport::line("  Display Type: RGB - %s", bsp_display.PANEL_MODEL);
    #elif defined(HAS_QSPI_PANEL)
        SystemReport::line("  Display Type: QSPI - %s", bsp_display.PANEL_MODEL);
    #elif defined(HAS_MIPI_PANEL)
        SystemReport::line("  Display Type: MIPI/DSI - %s", bsp_display.PANEL_MODEL);
    #endif
    SystemReport::line("  Touch Type: %s", bsp_touch.NAME);
    {
        const uint16_t ppi = bspPixelDensity();
        if (ppi) {
            SystemReport::line("  Density: %u PPI (%.1f\" diagonal)", (unsigned)ppi,
                               bsp_display.DIAGONAL_IN / 10.0);
            SystemReport::line("  UI Scale: %.2fx (derived)", (double)bspUiScale());
        } else {
            SystemReport::line("  Density: unknown - no DIAGONAL_IN in this board's BSP");
            SystemReport::line("  UI Scale: 1.00x (fallback)");
        }
    }
    SystemReport::line("  Resolution: %dx%d", bsp_display.WIDTH, bsp_display.HEIGHT);
    SystemReport::line("  Rotation: %d", bsp_display.ROTATION);
    SystemReport::line("  Brightness: %d%%", core.display().getBrightness());
}

void reportI2cScan() {
    SystemReport::line("[I2C BUS SCAN]");
    int nDevices = 0;
    for (uint8_t address = 1; address < 127; address++) {
        FleetI2C::beginTransmission(address);
        if (FleetI2C::endTransmission() == 0) {
            const char *name = "";
            if      (address == 0x18)                    name = "(ES8311)";
            else if (address == 0x40 || address == 0x41) name = "(ES7210)";
            else if (address == 0x5D || address == 0x14) name = "(GT911 Touch)";
            else if (address == 0x38 || address == 0x20) name = "(EXPANDER)";
            else if (address == 0x3B || address == 0x3C) name = "(AXS15231 Touch)";

            SystemReport::line("  Found I2C Device: 0x%02X %s", address, name);
            nDevices++;
        }
    }
    if (nDevices == 0) {
        SystemReport::line("  ERROR: NO I2C DEVICES FOUND! Check wiring/power.");
    } else {
        SystemReport::line("  I2C Scan Complete");
    }
}

} // namespace

namespace SystemReport {

const char *fmtBytes(uint64_t bytes, char *out, size_t outLen) {
    if (bytes < 1024ULL) {
        snprintf(out, outLen, "%llu bytes", (unsigned long long)bytes);
    } else if (bytes < 1024ULL * 1024ULL) {
        snprintf(out, outLen, "%llu bytes (%.1f KB)",
                 (unsigned long long)bytes, bytes / 1024.0);
    } else {
        snprintf(out, outLen, "%llu bytes (%.2f MB)",
                 (unsigned long long)bytes, bytes / (1024.0 * 1024.0));
    }
    return out;
}

bool addSink(Sink s) {
    if (!s || s_sinkCount >= MAX_SINKS) return false;
    s_sinks[s_sinkCount++] = s;
    return true;
}

bool addSection(const char *name, Section fill) {
    if (!name || !fill || s_sectionCount >= MAX_SECTIONS) return false;
    s_sections[s_sectionCount++] = { name, fill };
    return true;
}

void line(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (s_echoSerial) Serial.println(buf);
    for (uint8_t i = 0; i < s_sinkCount; i++) s_sinks[i](buf);
}

void run(SystemCore &core, bool echoSerial) {
    #ifdef DUMP_CONFIG
        s_echoSerial = true;
    #else
        // With nothing else listening, Serial is the only way this report can
        // be seen at all - so a sink-less build always echoes.
        s_echoSerial = echoSerial || (s_sinkCount == 0);
    #endif

    line("=== SYSTEM DIAGNOSTICS ===");

    // Firmware identity first, so a photo of the screen or a pasted serial dump
    // always says which build it came from.
    line("[FIRMWARE]");
    line("  Version: v%s", FW_VERSION);
    line("  Commit: %s", FW_COMMIT);
    line("  Device: %s", bsp_hw.device_name);

    reportConnectivity(core);
    reportEntities(core);
    reportHardware(core);
    reportDisplay(core);

    // Caller-registered sections - today just [UI STATE] from GUIManager.
    // Placed here so the I2C scan stays last, as it was.
    for (uint8_t i = 0; i < s_sectionCount; i++) {
        line("[%s]", s_sections[i].name);
        s_sections[i].fill();
    }

    reportI2cScan();

    if (s_echoSerial) Serial.println("==========================================\n");

    s_echoSerial = false; // Restore default (off) for any later line() calls.
}

} // namespace SystemReport
