#ifndef BOARD_HAS_PSRAM
#error "PSRAM Required! Enable in Tools menu."
#endif

#include <Arduino.h>
#include <Wire.h>
#include <FleetI2C.h>
#include "GuiManager.h"
#ifdef HAS_AUDIO_HW
#include "AudioManager.h"
#include "Panel_Audio.h"
#endif
#include "Panel_Header.h"
#include "Panel_Display.h"
#include "Panel_System.h"

// --= FORCE DEPENDENCIES =--
#include <bb_captouch.h>
#include "Fleet_BSP.h"
#ifdef BSP_HEADER
    #include BSP_HEADER 
#endif
// --------------------------

// --= OBJECTS =--
GuiManager gui;
#ifdef HAS_AUDIO_HW
AudioManager audioMgr;
#endif
Panel_Header header;
Panel_System pnlSystem;
Panel_Display pnlDisplay(gui);
#ifdef HAS_AUDIO_HW
Panel_Audio pnlAudio(audioMgr);
#endif

// Global callback to toggle system panel
static void header_icon_click_cb(lv_event_t * e) {
    pnlSystem.toggle();
}

static void close_system_panel_cb() {
    pnlSystem.close();
}

// Helper to check ESP32 GPIO Registers directly
bool isGpioOutput(int pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) return false;
    if (pin < 32) return (REG_READ(GPIO_ENABLE_REG) & (1ULL << pin));
    else          return (REG_READ(GPIO_ENABLE1_REG) & (1ULL << (pin - 32)));
    }

// --= SYSTEM DOCTOR =--
// manualTrigger: true when called from the System panel's "Dump Config"
// button, false for the automatic boot-time run. Controls whether this
// dump's output also mirrors to Serial (see Panel_System::setSerialEcho) -
// on for a manual run or if -D DUMP_CONFIG is set, off otherwise, since an
// automatic boot dump would mostly just duplicate the dashboard's own direct
// Serial prints already produced during setup().
void debug_dump_config(bool manualTrigger) {
    #ifdef DUMP_CONFIG
        pnlSystem.setSerialEcho(true);
    #else
        pnlSystem.setSerialEcho(manualTrigger);
    #endif

    pnlSystem.log("=== SYSTEM DIAGNOSTICS ===");
    
    // Hardware Status (Runtime)
    pnlSystem.log("[HARDWARE STATUS]");
    pnlSystem.log("  I2C Backend: %s", FleetI2C::backendName());
    pnlSystem.log("  Uptime: %lu ms", millis());
    pnlSystem.log("  Free Heap: %d kb", ESP.getFreeHeap()/1024);
    pnlSystem.log("  PSRAM Size: %d mb", ESP.getPsramSize()/1024/1024);
    #ifdef HAS_IO_EXPANDER
        pnlSystem.log("  Pin Expander: Active (TCA9554/Similar)");
    #else
        pnlSystem.log("  Pin Expander: None (Native GPIOs)");
    #endif
    #ifdef HAS_BUTTON
        pnlSystem.log("  Button: ENABLED");
    #else
        pnlSystem.log("  Button: NOT PRESENT");
    #endif
    
    // Audio State
    #ifdef HAS_AUDIO_HW
    pnlSystem.log("[AUDIO]");
    #ifdef HAS_ES8311
        pnlSystem.log("  Codec ES8311: ENABLED");
    #else
        pnlSystem.log("  Codec ES8311: NOT PRESENT");
    #endif

    #ifdef HAS_ES7210
        pnlSystem.log("  ADC ES7210: ENABLED");
    #else
        pnlSystem.log("  ADC ES7210: NOT PRESENT");
    #endif
    pnlSystem.log("  Volume: %d%%", audioMgr.getVolume());
    bool isMuted = audioMgr.getMute();
    pnlSystem.log("  Muted: %s", isMuted ? "YES" : "NO");
    #else
    pnlSystem.log("[AUDIO]");
    pnlSystem.log("  No audio hardware on this board");
    #endif

    // AMP Pin Diagnostics
    if (bsp_hw.I2S_AMP_EN != -1) {        
        #ifdef HAS_IO_EXPANDER
            // If checking an Expander pin, we can't check ESP32 registers.
            // We rely on the fact that we wrote to it.
            pnlSystem.log("AMP Driver: VIA IO EXPANDER (Assumed OUTPUT)");
        #else
            // Native GPIO Check
            pnlSystem.log("  Amp Pin: GPIO %d", bsp_hw.I2S_AMP_EN);
            bool isOut = isGpioOutput(bsp_hw.I2S_AMP_EN);
            pnlSystem.log("  AMP Driver: %s", isOut ? "OUTPUT (OK)" : "INPUT/HI-Z (ERROR!)");
        #endif
        
        pnlSystem.log("  AMP Pin Level: %s", digitalRead(bsp_hw.I2S_AMP_EN) ? "HIGH (ON)" : "LOW (OFF)");
    } else {
        pnlSystem.log("  Amp Pin: UNDEFINED");
    }

    // Display State
    pnlSystem.log("[DISPLAY]");
    #ifdef HAS_RGB_PANEL
        pnlSystem.log("  Display Type: RGB - %s", bsp_display.PANEL_MODEL);
        pnlSystem.log("  Touch Type: %s", bsp_touch.NAME);
    #elif defined(HAS_QSPI_PANEL)
        pnlSystem.log("  Display Type: QSPI - %s", bsp_display.PANEL_MODEL);
        pnlSystem.log("  Touch Type: %s", bsp_touch.NAME); 
    #elif defined(HAS_MIPI_PANEL)
        pnlSystem.log("  Display Type: MIPI/DSI - %s", bsp_display.PANEL_MODEL);
        pnlSystem.log("  Touch Type: %s", bsp_touch.NAME);
    #endif
    #ifdef HIGH_DPI_DISPLAY
        pnlSystem.log("  Display Mode: HIGH DPI (1.5x Scaling)");
    #else
        pnlSystem.log("  Display Mode: STANDARD (1.0x Scaling)");
    #endif
    pnlSystem.log("  Resolution: %dx%d", bsp_display.WIDTH, bsp_display.HEIGHT);
    pnlSystem.log("  Rotation: %d", bsp_display.ROTATION);
    pnlSystem.log("  Brightness: %d%%", gui.displayMgr.getBrightness());
    // Note: Can't easily get rotation back from GFX in a generic way without casting, skipping for now.

    // 3. UI State
    pnlSystem.log("[UI STATE]");
    lv_obj_t* activePnl = UiToolkit::getActiveAccordionPanel();
    pnlSystem.log("  Active Panel: %s", activePnl ? "EXPANDED" : "NONE (Collapsed)");
    
    // 4. I2C Bus Scan
    pnlSystem.log("[I2C BUS SCAN]");
    byte error, address;
    int nDevices = 0;
    // Standard I2C Scan
    for(address = 1; address < 127; address++ ) {
        FleetI2C::beginTransmission(address);
        error = FleetI2C::endTransmission();
        if (error == 0) {
            const char* name = "";
            if (address == 0x18) name = "(ES8311)";
            else if (address == 0x40 || address == 0x41) name = "(ES7210)";
            else if (address == 0x5D || address == 0x14) name = "(GT911 Touch)";
            else if (address == 0x38 || address == 0x20) name = "(EXPANDER)";
            else if (address == 0x3B || address == 0x3C) name = "(AXS15231 Touch)";
            
            pnlSystem.log("  Found I2C Device: 0x%02X %s", address, name);
            nDevices++;
        }
    }
    if (nDevices == 0) {
        pnlSystem.log("  ERROR: NO I2C DEVICES FOUND! Check wiring/power.");
    } else {
        pnlSystem.log("  I2C Scan Complete");
    }

    if (manualTrigger) {
        Serial.println("==========================================\n");
    }
    #ifdef DUMP_CONFIG
    else {
        Serial.println("==========================================\n");
    }
    #endif

    pnlSystem.setSerialEcho(false); // Restore default (off) for any later log() calls.
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Fleet Hardware Dashboard (Modular) ===");

    // Init Engine
    gui.begin();
    #ifdef HAS_AUDIO_HW
    audioMgr.begin();
    #endif

    // --= ROOT SCREEN =--
    lv_obj_t * screen = lv_screen_active();
    lv_obj_set_style_bg_color   (screen, lv_color_hex(0x101010), LV_PART_MAIN); // Dark Background
    lv_obj_clear_flag           (screen, LV_OBJ_FLAG_SCROLLABLE); // Disable Global Scrolling

    // --= LAYER 3: HEADER BAR =--
    // Header Click -> Toggle System Panel
    header.init(screen, bsp_hw.device_name);
    lv_obj_add_event_cb(header.getStatusIcon(), header_icon_click_cb, LV_EVENT_CLICKED, NULL);

    // 2. BOTTOM DECK (The "Right" Way)
    // Instead of pushing it off-screen, we calculate the remaining space.
    // Height = Screen Height - Header Height (50px)
    int32_t header_h = UiToolkit::sc(50);
    int32_t deck_h = lv_obj_get_height(screen);

    // --= LAYER 1: BOTTOM DECK =--
    // Contains Audio/Display Panels
    lv_obj_t * deck = lv_obj_create(screen);
    lv_obj_set_size               (deck, lv_pct(100), deck_h); 
    lv_obj_set_y                  (deck, header_h); // Bottom of header
    lv_obj_set_flex_flow          (deck, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align         (deck, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END); 
    lv_obj_set_style_bg_opa       (deck, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (deck, 0, 0);
    lv_obj_set_style_pad_all      (deck, UiToolkit::sc(10), 0);
    lv_obj_set_style_pad_gap      (deck, UiToolkit::sc(10), 0);
    lv_obj_clear_flag             (deck, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_clear_flag             (deck, LV_OBJ_FLAG_SCROLLABLE); 

    // --= LAYER 2: UPPER DECK (System Panel) =--
    // Full screen transparent layer to hold the system drawer
    lv_obj_t * upper_deck = lv_obj_create(screen);
    lv_obj_set_size               (upper_deck, lv_pct(100), lv_pct(100)); 
    lv_obj_set_y                  (upper_deck, 0); // Hidden behind header
    lv_obj_set_style_bg_opa       (upper_deck, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (upper_deck, 0, 0);
    lv_obj_clear_flag             (upper_deck, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_clear_flag             (upper_deck, LV_OBJ_FLAG_SCROLLABLE);

    // Bottom Panel Open -> Close System
    UiToolkit::registerSystemCloseCb(close_system_panel_cb);
    
    // System Open -> Hide Touch Window
    pnlSystem.setOnToggleCallback([](bool isOpen) {
        // If System Panel is OPEN (true), Hide Touch Window (false)
        // pnlDisplay.setTouchWindowVisibility(!isOpen); 
    });

    #ifdef HAS_AUDIO_HW
    pnlAudio.init(deck);
    #endif
    pnlDisplay.init(deck);
    pnlSystem.init(upper_deck, &header);

    // --= Z-INDEX SANDWICH =--
    // 0. Touch Overlay (Bottom - Hidden by default)
    // 1. Deck (Bottom panels)
    // 2. System Panel (Middle - Slides out)
    // 3. Header (Top - Covers System Panel top edge)

    // Touchpoint Overlay - 0 (set in Panel_Display init())
    lv_obj_move_to_index(deck, 1);                  // 1. Deck
    lv_obj_move_to_index(upper_deck, 2);            // 2. System Panel
    lv_obj_move_to_index(header.getContainer(), 3); // 3. Header

    // 4. Run System Doctor
    debug_dump_config(false); // automatic boot run, not manually triggered

    #ifdef DEBUG_DISPLAY
    Serial.println("[Loop] Entering loop() for the first time.");
    #endif
}

void loop() {
    #ifdef DEBUG_DISPLAY
    // Heartbeat: if this stops incrementing (or the whole boot log repeats
    // from the top), the board is hanging/reboot-looping somewhere in or
    // just after this point, not silently succeeding.
    static uint32_t loopCount = 0;
    loopCount++;
    if (loopCount <= 5 || loopCount % 500 == 0) {
        Serial.printf("[Loop] Heartbeat #%lu (uptime %lu ms)\n", (unsigned long)loopCount, millis());
    }
    #endif

    #ifdef DEBUG_DISPLAY
    if (loopCount <= 5) Serial.println("[Loop] Calling gui.update()...");
    #endif
    gui.update();
    #ifdef DEBUG_DISPLAY
    if (loopCount <= 5) Serial.println("[Loop] gui.update() returned.");
    #endif

    header.tick();
    pnlDisplay.tick();
    #ifdef HAS_AUDIO_HW
    pnlAudio.tick();
    #endif
    delay(2);
}