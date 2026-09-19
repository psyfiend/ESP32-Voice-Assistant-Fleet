#include "UI/LogPage.h"
#include "UI/UITokens.h"
#include "UI/UIToolkit.h"
#include <lvgl.h>

namespace {

lv_obj_t *s_screen   = nullptr;
lv_obj_t *s_previous = nullptr;
lv_obj_t *s_label    = nullptr;

std::function<void()> s_onDump  = nullptr;
std::function<void()> s_onClose = nullptr;

lv_obj_t *topButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_height          (b, UI::minTouch());
    lv_obj_set_width           (b, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color  (b, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_radius    (b, UI::sc(UI::met().RADIUS / 2), 0);
    lv_obj_add_event_cb        (b, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text          (l, text);
    lv_obj_set_style_text_font (l, UI::type().TAG, 0);
    lv_obj_set_style_text_color(l, UI::c(UI::pal().TEXT), 0);
    lv_obj_center              (l);
    return b;
}

} // namespace

namespace LogPage {

void setDumpHandler (std::function<void()> cb) { s_onDump  = cb; }
void setCloseHandler(std::function<void()> cb) { s_onClose = cb; }

void close() {
    if (s_previous) lv_screen_load(s_previous);
    if (s_screen) { lv_obj_delete(s_screen); s_screen = nullptr; }
    s_label = nullptr;
    if (s_onClose) s_onClose();
}

// Diagnostics for the Dump Config freeze, 2026-09-18. Enable with
// -D DEBUG_LOGPAGE in an environment's build_flags.
//
// The report itself was ruled out by proving it completes - the closing
// separator reaches the serial line - so the hang is in one of the two calls
// below and this says which. Each print is flushed, because on a CDC board an
// unflushed line is lost when the CPU stops and the last thing you see is not
// the last thing that ran.
#ifdef DEBUG_LOGPAGE
    #define DBG_LOGPAGE(...) do { Serial.printf("[LogPage:debug] " __VA_ARGS__); \
                                  if (Serial) Serial.flush(); } while (0)
#else
    #define DBG_LOGPAGE(...) do {} while (0)
#endif

void refresh(const char *text) {
    if (s_label && text) {
        DBG_LOGPAGE("refresh: %u chars -> set_text\n", (unsigned)strlen(text));
        lv_label_set_text(s_label, text);
        DBG_LOGPAGE("refresh: set_text returned -> scroll\n");
        lv_obj_scroll_to_y(lv_obj_get_parent(s_label), LV_COORD_MAX, LV_ANIM_OFF);
        DBG_LOGPAGE("refresh: done\n");
    }
}

void show(const char *text) {
    if (!s_previous) s_previous = lv_screen_active();

    // Load the previous screen BEFORE deleting this one, the same order
    // CardDemo uses. Deleting the active screen works but LVGL rightly
    // complains every time, and a log full of warnings that are fine is how a
    // real one gets missed.
    if (s_screen) { lv_screen_load(s_previous); lv_obj_delete(s_screen); s_screen = nullptr; }

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI::c(UI::pal().GROUND), 0);
    lv_obj_clear_flag        (s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *col = lv_obj_create(s_screen);
    lv_obj_set_size               (col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow          (col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa       (col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (col, 0, 0);
    lv_obj_set_style_pad_all      (col, UI::sc(6), 0);
    lv_obj_set_style_pad_gap      (col, UI::sc(6), 0);
    lv_obj_clear_flag             (col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bar = lv_obj_create(col);
    lv_obj_set_width              (bar, lv_pct(100));
    lv_obj_set_height             (bar, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow          (bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa       (bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (bar, 0, 0);
    lv_obj_set_style_pad_all      (bar, 0, 0);
    lv_obj_set_style_pad_gap      (bar, UI::sc(6), 0);
    lv_obj_clear_flag             (bar, LV_OBJ_FLAG_SCROLLABLE);

    topButton(bar, LV_SYMBOL_LEFT " Back", [](lv_event_t *e) { (void)e; LogPage::close(); });
    topButton(bar, "Dump", [](lv_event_t *e) {
        (void)e;
        if (s_onDump) s_onDump();     // re-runs the report; refresh() redraws it
    });

    // The report itself. This is the scroller that used to live inside the
    // System panel and make its height animation stutter.
    lv_obj_t *box = lv_obj_create(col);
    lv_obj_set_width              (box, lv_pct(100));
    lv_obj_set_flex_grow          (box, 1);
    lv_obj_set_style_bg_color     (box, UI::c(UI::pal().SURFACE), 0);
    lv_obj_set_style_border_color (box, UI::border(), 0);
    lv_obj_set_style_border_width (box, 1, 0);
    lv_obj_set_style_radius       (box, UI::sc(UI::met().RADIUS / 2), 0);
    lv_obj_set_style_pad_all      (box, UI::sc(8), 0);
    UI::tameScroll                (box);

    s_label = lv_label_create(box);
    lv_obj_set_width           (s_label, lv_pct(100));
    lv_label_set_long_mode     (s_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text          (s_label, (text && text[0]) ? text : "(no report yet - press Dump)");
    lv_obj_set_style_text_color(s_label, UI::c(UI::pal().TEXT), 0);
    lv_obj_set_style_text_font (s_label, UI::type().TAG, 0);

    lv_screen_load(s_screen);
}

} // namespace LogPage
