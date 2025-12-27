#include "GuiManager.h"
#include "UiToolkit.h" // Needed for HIGH_DPI_DISPLAY check

// --- LVGL CALLBACKS ---
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    GuiManager* gui = (GuiManager*)lv_display_get_user_data(disp);
    Arduino_GFX *gfx = gui->displayMgr.getGfx();
    
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    gfx->flush();
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
    
    // Allocate 1/10th of the screen per buffer
    #ifdef WS_P4_SMART86
        size_t pixel_count = (gfx->width() * cfg.DRAW_BUF_HEIGHT);
        Serial.println("[GUI] P4 Smart86 Detected: Using Configured Draw Buffer Height.");
    #else
        size_t pixel_count = (gfx->width() * gfx->height() / 10);
    #endif

    size_t byte_count = pixel_count * sizeof(uint16_t);

    // --= NEW: Smart Architecture Allocation =--
    uint32_t malloc_flags = MALLOC_CAP_DMA; // Base requirement: DMA capable

    #ifdef CONFIG_IDF_TARGET_ESP32P4
        // P4 Strategy: Ferrari Mode.
        // The P4 has ~2.5MB of Unified Internal SRAM. 
        // 100KB buffers fit easily and are MUCH faster than PSRAM.
        malloc_flags |= MALLOC_CAP_INTERNAL;
        Serial.println("[GUI] Allocating Buffers in INTERNAL SRAM (Fast)");
    #else
        // S3 Strategy: Cargo Truck Mode.
        // The S3 has limited/fragmented SRAM. Large buffers must go to PSRAM.
        malloc_flags |= MALLOC_CAP_SPIRAM;
        Serial.println("[GUI] Allocating Buffers in PSRAM");
    #endif

    _draw_buf = (uint16_t*)heap_caps_malloc(byte_count, malloc_flags);
    // Fallback: If optimized allocation fails, try generic malloc
    if (!_draw_buf) _draw_buf = (uint16_t*)malloc(byte_count);

    _draw_buf2 = (uint16_t*)heap_caps_malloc(byte_count, malloc_flags);
    if (!_draw_buf2) _draw_buf2 = (uint16_t*)malloc(byte_count);

    if (!_draw_buf) {
        Serial.println("[GUI] Critical: Failed to allocate draw buffer!");
        while(1) delay(100);
    }

    // 4. Driver Registration
    _disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(_disp, my_disp_flush);
    
    // Register BOTH buffers here
    lv_display_set_buffers(_disp, _draw_buf, _draw_buf2, byte_count, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    lv_display_set_user_data(_disp, this); 

    // --= DPI Awareness =--
    #ifdef HIGH_DPI_DISPLAY
        // P4 Smart86: ~180 PPI calculated, Waveshare uses 150.
        lv_display_set_dpi(_disp, 150); 
    #else
        // S3 Smart86: ~120 PPI.
        lv_display_set_dpi(_disp, 120);
    #endif

    _indev = lv_indev_create();
    lv_indev_set_type(_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(_indev, my_touch_read);
    lv_indev_set_user_data(_indev, this);

    // 5. Styles & Toolkit
    UiToolkit::init();
    
    Serial.println("[GUI] Engine Started (Double Buffer + DMA).");
}

void GuiManager::update() {
    lv_timer_handler();
}