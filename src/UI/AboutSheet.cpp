// "About this board" (2.12 P4) - see include/UI/AboutSheet.h. The Sheet's second client, made to
// prove the frame is a component: one view, no tabs, no panels; the X closes it, and it closes
// itself after a minute untouched, as the card window does.
#include "UI/AboutSheet.h"

#ifdef DEBUG_POPUP
#include "UI/Sheet.h"
#include "bsp_loader.h"
#include "fleet_fw_version.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace AboutSheet {

static Kit::Sheet     S;
static Kit::SheetSpec spec;
static lv_obj_t *s_up = nullptr, *s_heap = nullptr, *s_pool = nullptr;   // what changes while it shows

static void closeCb(lv_event_t *ev) { (void)ev; Kit::sheetClose(S); }

// What changes while it shows, in words.
static void upWords(char *buf, size_t n) {
    const uint32_t sec = millis() / 1000;
    snprintf(buf, n, "%lu h %02lu min", (unsigned long)(sec / 3600), (unsigned long)((sec / 60) % 60));
}
static void heapWords(char *buf, size_t n) {
    snprintf(buf, n, "%lu KB internal, %lu KB PSRAM",
             (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}
static void poolWords(char *buf, size_t n) {
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    snprintf(buf, n, "%lu of %lu KB used", (unsigned long)((m.total_size - m.free_size) / 1024),
             (unsigned long)(m.total_size / 1024));
}

static void build() {
    lv_label_set_text(S.lblTitleArea, "");
    lv_label_set_text(S.lblTitleName, "About this board");
    Kit::sheetLayoutTitle(S);

    const UIShape &k = UI::shape();
    lv_obj_t *list = Kit::plain(S.stage);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(list, UI::mm(k.S10), 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, UI::mm(k.S5), 0);

    char buf[64];
    Kit::factRow(list, "Board", bsp_hw.MODEL);
    Kit::factRow(list, "Made by", bsp_hw.MANUFACTURER);
    snprintf(buf, sizeof(buf), "%ld x %ld, %u.%u in, %u PPI", (long)bsp_display.WIDTH, (long)bsp_display.HEIGHT,
             (unsigned)(bsp_display.DIAGONAL_IN / 10), (unsigned)(bsp_display.DIAGONAL_IN % 10),
             (unsigned)bspPixelDensity());
    Kit::factRow(list, "Screen", buf);
    Kit::factRow(list, "Panel", bsp_display.PANEL_MODEL);
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    snprintf(buf, sizeof(buf), "%s, %u cores, revision %u.%u", CONFIG_IDF_TARGET, (unsigned)chip.cores,
             (unsigned)(chip.revision / 100), (unsigned)(chip.revision % 100));
    Kit::factRow(list, "Chip", buf);
    snprintf(buf, sizeof(buf), "%s (%s)", FW_VERSION, FW_COMMIT);
    Kit::factRow(list, "Firmware", buf);
    upWords(buf, sizeof(buf));
    s_up = Kit::factRow(list, "Running for", buf);
    heapWords(buf, sizeof(buf));
    s_heap = Kit::factRow(list, "Free memory", buf);
    poolWords(buf, sizeof(buf));
    s_pool = Kit::factRow(list, "LVGL's pool", buf);
}

// Once a second is plenty; only what changed is set (setting identical text still redraws).
static void tick(uint32_t now) {
    static uint32_t last = 0;
    if (now - last < 1000) return;
    last = now;
    char buf[64];
    auto set = [](lv_obj_t *l, const char *t) {
        if (l && strcmp(lv_label_get_text(l), t)) lv_label_set_text(l, t);
    };
    upWords(buf, sizeof(buf));   set(s_up, buf);
    heapWords(buf, sizeof(buf)); set(s_heap, buf);
    poolWords(buf, sizeof(buf)); set(s_pool, buf);
}

static void closed() { s_up = s_heap = s_pool = nullptr; }

bool open() {
    if (Kit::sheetShowing()) return false;
    spec = Kit::SheetSpec{};
    spec.leftCb     = closeCb;
    spec.buildFirst = build;
    spec.tick       = tick;
    spec.closed     = closed;
    spec.sparks     = true;
    Kit::sheetPlace(S);
    Kit::sheetOpen(S, spec);
    return true;
}

void close()  { Kit::sheetClose(S); }
bool isOpen() { return S.win != nullptr; }

} // namespace AboutSheet

#else   // not a debug build: nothing to open

namespace AboutSheet {
bool open()   { return false; }
void close()  {}
bool isOpen() { return false; }
}

#endif
