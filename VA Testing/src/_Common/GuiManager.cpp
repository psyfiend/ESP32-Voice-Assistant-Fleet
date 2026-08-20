#include "GuiManager.h"
#include "UiToolkit.h" // Needed for HIGH_DPI_DISPLAY check

// --- LVGL CALLBACKS ---
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    GuiManager* gui = (GuiManager*)lv_display_get_user_data(disp);
    Arduino_GFX *gfx = gui->displayMgr.getGfx();
    
    // 1. Draw the chunk (Partial or Full)
    // Even in "Direct Mode" style usage with full buffers, we use this to copy
    // the pixels from our LVGL buffer into the Arduino_GFX internal driver.
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    
    // 2. The "Waveshare Logic" (Batching Optimization)
    // We check if this is the LAST chunk of the frame.
    // If it is, we tell the hardware to refresh. This prevents sending 
    // partial frames to MIPI/RGB displays which can cause tearing or high bus overhead.
    if (lv_display_flush_is_last(disp)) {
        gfx->flush();
    }
    
    // 3. Notify LVGL we are done
    lv_display_flush_ready(disp);
}

static void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    GuiManager* gui = (GuiManager*)lv_indev_get_user_data(indev);
    TouchPoint points[5];
    uint8_t count = gui->touchMgr.read(points, 5); 

    if (count > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = points[0].x;
        data->point.y = points[0].y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static uint32_t my_tick_get_cb(void) { return millis(); }

// --- IMPLEMENTATION ---

GuiManager::GuiManager() {
    _disp = NULL;
    _indev = NULL;
    _draw_buf = NULL;
    _draw_buf2 = NULL;
}

void GuiManager::begin() {
    // 1. Hardware Init
    if (!displayMgr.begin()) {
        Serial.println("[GUI] Display Init Failed!");
        while(1) delay(100);
    }
    touchMgr.begin();

    // 2. LVGL Init
    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    // 3. Buffer Alloc (Ping-Pong Strategy)
    Arduino_GFX *gfx = displayMgr.getGfx();
    
    // --= Intelligent Architecture Allocation =--
    
    size_t pixel_count = 0;
    uint32_t malloc_flags = MALLOC_CAP_DMA; // Always need DMA capability

    // Case A: High Bandwidth Interfaces (MIPI / RGB)
    #if defined(HAS_MIPI_PANEL) || defined(HAS_RGB_PANEL)
        Serial.println("[GUI] Config: High Bandwidth (MIPI/RGB)");
        
        // Use Full Frame Buffers in PSRAM.
        // Even though we use PARTIAL mode below, having the buffer be full size
        // allows LVGL to render huge chunks at once, and our 'is_last' check
        // ensures we sync with the display refresh rate.
        pixel_count = gfx->width() * gfx->height(); 
        malloc_flags |= MALLOC_CAP_SPIRAM;
        
        Serial.println("[GUI] Strategy: Full Frame Buffers (PSRAM) + Frame Sync");

    // Case B: Bus Constrained Interfaces (SPI / QSPI)
    #else 
        Serial.println("[GUI] Config: Bus Constrained (SPI/QSPI)");
        
        // 1/10th screen size in Internal SRAM.
        pixel_count = (gfx->width() * gfx->height()) / 10;
        
        #ifdef CONFIG_IDF_TARGET_ESP32P4
            malloc_flags |= MALLOC_CAP_INTERNAL; 
        #else
            malloc_flags |= MALLOC_CAP_INTERNAL;
        #endif
        
        Serial.println("[GUI] Strategy: 1/10th Partial Buffers (Internal SRAM)");
    #endif

    // Override: If a specific config defined a draw buffer height, respect it
    #ifdef DRAW_BUF_HEIGHT
        pixel_count = gfx->width() * DRAW_BUF_HEIGHT;
        Serial.println("[GUI] Override: Using Custom Draw Buffer Height");
    #endif

    size_t byte_count = pixel_count * sizeof(uint16_t);

    Serial.printf("[GUI] Allocating: %d bytes per buffer... ", byte_count);

    _draw_buf = (uint16_t*)heap_caps_malloc(byte_count, malloc_flags);
    
    // Fallback 1: If Internal failed, try PSRAM
    if (!_draw_buf && (malloc_flags & MALLOC_CAP_INTERNAL)) {
        Serial.print(" (Internal Full! Retrying PSRAM)... ");
        malloc_flags &= ~MALLOC_CAP_INTERNAL;
        malloc_flags |= MALLOC_CAP_SPIRAM;
        _draw_buf = (uint16_t*)heap_caps_malloc(byte_count, malloc_flags);
    }
    // Fallback 2: Generic Malloc
    if (!_draw_buf) {
        Serial.print(" (Struct alloc failed! Retrying generic)... ");
        _draw_buf = (uint16_t*)malloc(byte_count);
    }

    // Allocate Second Buffer (Double Buffering)
    _draw_buf2 = (uint16_t*)heap_caps_malloc(byte_count, malloc_flags);
    if (!_draw_buf2) _draw_buf2 = (uint16_t*)malloc(byte_count);

    if (!_draw_buf) {
        Serial.println("\n[GUI] Critical: Failed to allocate ANY draw buffer!");
        while(1) delay(100);
    }
    Serial.println("Success.");

    // 4. Driver Registration
    _disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(_disp, my_disp_flush);
    
    // Note: We use PARTIAL mode. This is safer for Arduino_GFX.
    // If we used DIRECT mode, we would need to handle 'strided' memory writes manually
    // because Arduino_GFX expects contiguous bitmaps.
    lv_display_set_buffers(_disp, _draw_buf, _draw_buf2, byte_count, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    lv_display_set_user_data(_disp, this); 

    // --= DPI Awareness =--
    #ifdef HIGH_DPI_DISPLAY
        lv_display_set_dpi(_disp, 150); 
    #endif

    _indev = lv_indev_create();
    lv_indev_set_type(_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(_indev, my_touch_read);
    lv_indev_set_user_data(_indev, this);

    // 5. Styles & Toolkit
    UiToolkit::init();
    
    Serial.println("[GUI] Engine Started.");
}

void GuiManager::update() {
    lv_timer_handler();
}